// folia.core.parser — Markdown parser. Pure core, no Windows dependency.
//
// Faithful C++ port of the Rust `markdown-parse` crate with the bug fixes
// dictated by HANDOFF.md applied from the start:
//   * char-indexed slicing everywhere (no byte-slice-of-char-idx panics)
//   * strong requires TWO `*`/`_`; emphasis a single one (the old
//     `**word**` misread-as-strong bug never reproduced)
//   * `##no-space` is not a heading
//   * `rest_of_line()` is read-only; content consumers advance_n(len) to
//     avoid the list-reparse-as-paragraph duplication bug
//   * `$$` at line start is block math; a lone `$` immediately followed by
//     `$` is NOT inline math — left to the block path
//   * unsafe raw HTML becomes BlockNode::UnsupportedMarkup
//   * ` ```math ` fence becomes MathBlock{ FencedMath } (when enabled)
//   * list / blockquote recursion uses parse_blocks with stop conditions
//
// All offsets are CHAR indices into a Vec<char32> snapshot.
export module folia.core.parser;
import std;
import folia.core.types;
import folia.core.ids;
import folia.core.dialect;
import folia.core.image_dimension;
import folia.core.diagnostics;
import folia.core.metadata;
import folia.core.symbols;
import folia.core.ast;
import folia.core.container_source;
import folia.core.block_source;
import folia.core.block_tree;
import folia.core.block_line_recognizer;
import folia.core.callout;
import folia.core.document;
import folia.core.document_text;
import folia.core.document_symbols;
import folia.core.outline;
import folia.core.slug;
import folia.core.utf;
import folia.core.link_label;
import folia.core.html_entities;
import folia.core.markdown_html;
import folia.core.inline_cst;
import folia.core.inline_document;
import folia.core.inline_parser;
import folia.core.html_cst;
import folia.core.instrumentation;
import folia.core.serializer;
import folia.core.text_edit;

export namespace folia {

using ParseProgressCallback = std::function<void(std::size_t consumed, std::size_t total)>;

struct ParseInput {
    std::uint64_t revision = 1;
    std::string text;                       // UTF-8 source
    MarkdownDialect dialect = default_dialect();
    ParseProgressCallback progress;
    // Parse-time container context; never an editing coordinate.
    std::vector<std::size_t> lazy_lines;
    std::vector<BlockLineIndentation> indentation;

    ParseInput() = default;
    ParseInput(
        std::uint64_t rev,
        std::string t,
        MarkdownDialect d = default_dialect(),
        ParseProgressCallback callback = {},
        std::vector<BlockLineIndentation> line_indentation = {})
        : revision(rev), text(std::move(t)), dialect(d), progress(std::move(callback)),
          indentation(std::move(line_indentation)) {}
};

struct ParseOutput {
    std::uint64_t revision = 1;
    EditorDocument document;
    DocumentSymbolIndex symbols;
    DocumentSymbolContributions symbol_contributions;
    Outline outline;
    std::vector<Diagnostic> diagnostics;
};

// A block fragment is the smallest parser entry point used by structure-aware
// editing. Its physical ranges are transient mapping data for replacing one
// edited direct block; they never become a second selection coordinate.
struct BlockFragmentSourceRange {
    NodeId node_id{};
    SourceRange source_range;
    SourceRange content_range;
};

struct ParsedBlockFragment {
    BlockVec blocks;
    std::vector<BlockFragmentSourceRange> source_ranges;
};

// ---------------------------------------------------------------------------
//                              the Parser
// ---------------------------------------------------------------------------
namespace detail {

// Parse-time physical ranges are transient recognizer bookkeeping only. They
// are never stored in EditorDocument and never become an editing coordinate.
struct PhysicalRange {
    std::size_t start = 0;
    std::size_t end = 0;
};

struct ParserSourceRange {
    NodeId node_id{};
    PhysicalRange source_range;
    PhysicalRange content_range;
    std::vector<PhysicalRange> marker_ranges;
    std::vector<std::size_t> owned_offsets;
    std::optional<PhysicalRange> separator_range;

    ParserSourceRange() = default;
    ParserSourceRange(NodeId id, PhysicalRange source, PhysicalRange content)
        : node_id(id), source_range(source), content_range(content) {}
};

class Parser {
public:
    struct LinkDefinition {
        std::string label;
        std::string href;
        std::optional<std::string> title;
        std::size_t source_start = 0;
        std::size_t source_end = 0;
    };

    struct HtmlTag {
        std::string name;
        std::unordered_map<std::string, std::string> attributes;
        std::size_t start = 0;
        std::size_t end = 0;
        bool closing = false;
        bool self_closing = false;
    };

    const ParseInput* input;
    std::u32string cps;          // text as codepoints (this is the index space)
    std::size_t pos = 0;
    std::uint64_t node_counter = 1;
    std::vector<Diagnostic> diagnostics;
    std::vector<ParserSourceRange> source_ranges;
    std::unordered_map<std::string, LinkDefinition> link_definitions;
    std::unordered_map<std::size_t, LinkDefinition> link_definitions_by_start;

    explicit Parser(const ParseInput* in) : input(in) {
        cps = utf8_to_cps(in->text);
        scan_link_definitions();
    }

    BlockIndentation scan_indentation(
        std::size_t at,
        std::size_t maximum_columns = (std::numeric_limits<std::size_t>::max)()) const {
        auto line_start = at;
        while (line_start > 0 && cps[line_start - 1] != U'\n' && cps[line_start - 1] != U'\r') --line_start;
        const auto line = line_context(line_start);
        auto physical_column = line.physical_column;
        for (auto cursor = line_start; cursor < at; ++cursor) {
            physical_column += cps[cursor] == U'\t' ? 4 - physical_column % 4 : 1;
        }
        return scan_block_indentation(
            cps, at, maximum_columns, physical_column,
            at == line.source_offset ? line.residual_columns : 0);
    }

    BlockLineIndentation line_context(std::size_t at) const {
        const auto context = std::ranges::lower_bound(
            input->indentation, at, {}, &BlockLineIndentation::source_offset);
        if (context != input->indentation.end() && context->source_offset == at) return *context;
        return {at, 0, 0};
    }

    static std::string normalize_link_label(std::u32string_view label) {
        return normalize_markdown_link_label(label);
    }

    static std::u32string decode_link_component(std::u32string_view raw) {
        std::u32string decoded;
        for (std::size_t cursor = 0; cursor < raw.size();) {
            if (raw[cursor] == U'&') {
                auto semicolon = cursor + 1;
                while (semicolon < raw.size() && semicolon - cursor <= 35
                    && raw[semicolon] != U';' && !is_line_ending_character(raw[semicolon])) ++semicolon;
                if (semicolon < raw.size() && raw[semicolon] == U';') {
                    const auto reference = raw.substr(cursor, semicolon - cursor + 1);
                    if (auto value = decode_html_character_reference(reference)) {
                        decoded.append(*value);
                        cursor = semicolon + 1;
                        continue;
                    }
                }
            }
            if (raw[cursor] == U'\\' && cursor + 1 < raw.size() && ascii_punctuation(raw[cursor + 1])) {
                decoded.push_back(raw[cursor + 1]);
                cursor += 2;
            } else {
                decoded.push_back(raw[cursor++]);
            }
        }
        return decoded;
    }

    std::optional<LinkDefinition> link_definition_at(std::size_t start) const {
        const auto source_start = start;
        std::size_t cursor = start;
        std::size_t indentation = 0;
        while (cursor < cps.size() && cps[cursor] == U' ' && indentation < 4) {
            ++cursor;
            ++indentation;
        }
        if (indentation > 3 || cursor >= cps.size() || cps[cursor] != U'[') return std::nullopt;
        const auto label_start = ++cursor;
        bool escaped = false;
        while (cursor < cps.size()) {
            const auto ch = cps[cursor];
            if (is_line_ending_character(ch)) {
                if (ch == U'\r' && cursor + 1 < cps.size() && cps[cursor + 1] == U'\n') ++cursor;
                ++cursor;
                escaped = false;
                continue;
            }
            if (!escaped && ch == U'[') return std::nullopt;
            if (!escaped && ch == U']') break;
            if (cursor - label_start >= 999) return std::nullopt;
            if (!escaped && ch == U'\\') escaped = true;
            else escaped = false;
            ++cursor;
        }
        if (cursor >= cps.size() || cursor == label_start || cursor + 1 >= cps.size() || cps[cursor + 1] != U':') return std::nullopt;
        const auto label_end = cursor;
        const auto label = normalize_link_label(std::u32string_view(cps).substr(label_start, label_end - label_start));
        if (label.empty() || label_end - label_start > 999) return std::nullopt;
        cursor += 2;

        auto skip_space = [&](bool allow_one_line_break) {
            bool saw_line_break = false;
            bool saw_any = false;
            while (cursor < cps.size()) {
                if (cps[cursor] == U' ' || cps[cursor] == U'\t') {
                    ++cursor;
                    saw_any = true;
                } else if (is_line_ending_character(cps[cursor])) {
                    if (!allow_one_line_break || saw_line_break) break;
                    saw_line_break = true;
                    saw_any = true;
                    if (cps[cursor] == U'\r' && cursor + 1 < cps.size() && cps[cursor + 1] == U'\n') ++cursor;
                    ++cursor;
                } else {
                    break;
                }
            }
            return saw_any;
        };
        skip_space(true);

        std::u32string href;
        bool angle_destination = false;
        if (cursor < cps.size() && cps[cursor] == U'<') {
            angle_destination = true;
            ++cursor;
            bool destination_escape = false;
            while (cursor < cps.size() && !is_line_ending_character(cps[cursor])) {
                if (!destination_escape && cps[cursor] == U'>') break;
                if (!destination_escape && cps[cursor] == U'<') return std::nullopt;
                href.push_back(cps[cursor]);
                if (!destination_escape && cps[cursor] == U'\\') destination_escape = true;
                else destination_escape = false;
                ++cursor;
            }
            if (cursor >= cps.size() || cps[cursor] != U'>') return std::nullopt;
            ++cursor;
        } else {
            std::size_t paren_depth = 0;
            bool destination_escape = false;
            while (cursor < cps.size() && !is_line_ending_character(cps[cursor])
                && cps[cursor] != U' ' && cps[cursor] != U'\t') {
                const auto ch = cps[cursor];
                if (!destination_escape && (ch == U'<' || ch == U'>' || ch == U'"')) return std::nullopt;
                if (!destination_escape && ch == U'(') {
                    if (++paren_depth > 32) return std::nullopt;
                } else if (!destination_escape && ch == U')') {
                    if (paren_depth == 0) return std::nullopt;
                    --paren_depth;
                }
                href.push_back(ch);
                if (!destination_escape && ch == U'\\') destination_escape = true;
                else destination_escape = false;
                ++cursor;
            }
            if (paren_depth != 0) return std::nullopt;
        }
        if (href.empty() && !angle_destination) return std::nullopt;

        const auto destination_end = cursor;
        const auto had_title_separator = skip_space(true);
        bool title_on_next_line = false;
        for (auto index = destination_end; index < cursor; ++index) {
            if (is_line_ending_character(cps[index])) {
                title_on_next_line = true;
                break;
            }
        }
        std::optional<std::string> title;
        if (cursor < cps.size() && (cps[cursor] == U'"' || cps[cursor] == U'\'' || cps[cursor] == U'(')) {
            if (!had_title_separator) return std::nullopt;
            const auto opening = cps[cursor++];
            const auto closing = opening == U'(' ? U')' : opening;
            std::u32string value;
            bool title_escape = false;
            bool closed = false;
            bool invalid_blank_line = false;
            while (cursor < cps.size()) {
                const auto ch = cps[cursor];
                if (!title_escape && ch == closing) { ++cursor; closed = true; break; }
                if (!title_escape && opening == U'(' && ch == U'(') return std::nullopt;
                if (is_line_ending_character(ch)) {
                    auto next_line = cursor + line_ending_length(cursor);
                    while (next_line < cps.size() && (cps[next_line] == U' ' || cps[next_line] == U'\t')) ++next_line;
                    if (next_line < cps.size() && is_line_ending_character(cps[next_line])) {
                        invalid_blank_line = true;
                        break;
                    }
                    if (ch == U'\r' && cursor + 1 < cps.size() && cps[cursor + 1] == U'\n') ++cursor;
                    value.push_back(U'\n');
                    ++cursor;
                    title_escape = false;
                    continue;
                }
                if (!title_escape && ch == U'\\') title_escape = true;
                else title_escape = false;
                value.push_back(ch);
                ++cursor;
            }
            if (invalid_blank_line) return std::nullopt;
            if (closed) {
                skip_space(false);
                if (cursor >= cps.size() || is_line_ending_character(cps[cursor])) {
                    title = cps_to_utf8(value);
                } else if (title_on_next_line) {
                    // An invalid optional title on the following line is
                    // ordinary block content; keep the destination-only ref.
                    cursor = destination_end;
                    while (cursor < cps.size() && (cps[cursor] == U' ' || cps[cursor] == U'\t')) ++cursor;
                } else {
                    return std::nullopt;
                }
            } else if (title_on_next_line) {
                cursor = destination_end;
                while (cursor < cps.size() && (cps[cursor] == U' ' || cps[cursor] == U'\t')) ++cursor;
            } else {
                return std::nullopt;
            }
        } else {
            // A destination cannot be followed by non-whitespace on its last line.
            cursor = destination_end;
            while (cursor < cps.size() && (cps[cursor] == U' ' || cps[cursor] == U'\t')) ++cursor;
        }
        if (cursor < cps.size() && !is_line_ending_character(cps[cursor])) return std::nullopt;
        const auto definition_line_end = line_content_end(cursor);
        LinkDefinition definition;
        definition.label = label;
        definition.href = cps_to_utf8(decode_link_component(href));
        if (title) title = cps_to_utf8(decode_link_component(utf8_to_cps(*title)));
        definition.title = std::move(title);
        definition.source_start = source_start;
        definition.source_end = next_line_start(definition_line_end);
        return definition;
    }

    InlineParseContext inline_parse_context() {
        InlineParseContext context;
        context.dialect = input->dialect;
        context.allocate_id = [this] { return next_node_id(); };
        context.resolve_link_label = [this](const std::string& label) -> std::optional<InlineLinkDef> {
            const auto found = link_definitions.find(label);
            if (found == link_definitions.end()) return std::nullopt;
            return InlineLinkDef{found->second.href, found->second.title};
        };
        return context;
    }

    InlineDocument make_inline_document(
        std::size_t start,
        std::size_t end,
        InlineSyntaxMode syntax_mode = InlineSyntaxMode::Markdown) {
        start = (std::min)(start, cps.size());
        end = (std::min)((std::max)(end, start), cps.size());
        InlineDocument document;
        document.source = cps.substr(start, end - start);
        document.syntax_mode = syntax_mode;
        reparse_inline_document(document, inline_parse_context());
        return document;
    }

    void scan_link_definitions() {
        std::size_t line_start = 0;
        bool paragraph_open = false;
        bool quote_paragraph_open = false;
        char32_t fence_marker = 0;
        std::size_t fence_length = 0;
        char32_t quote_fence_marker = 0;
        std::size_t quote_fence_length = 0;
        while (line_start < cps.size()) {
            const auto line_end = line_content_end(line_start);
            const auto next = next_line_start(line_end);
            if (fence_marker != 0) {
                auto cursor = line_start;
                std::size_t indent = 0;
                while (cursor < line_end && cps[cursor] == U' ' && indent < 4) { ++cursor; ++indent; }
                const auto run_start = cursor;
                while (cursor < line_end && cps[cursor] == fence_marker) ++cursor;
                const auto run_length = cursor - run_start;
                while (cursor < line_end && (cps[cursor] == U' ' || cps[cursor] == U'\t')) ++cursor;
                if (indent <= 3 && run_length >= fence_length && cursor == line_end) {
                    fence_marker = 0;
                    fence_length = 0;
                }
                line_start = next;
                continue;
            }

            auto content_start = line_start;
            std::size_t indentation = 0;
            while (content_start < line_end && cps[content_start] == U' ' && indentation < 4) {
                ++content_start;
                ++indentation;
            }
            if (indentation > 3) {
                if (is_blank_line_at(line_start)) {
                    paragraph_open = false;
                    quote_paragraph_open = false;
                }
                line_start = next;
                continue;
            }
            if (content_start == line_end) {
                paragraph_open = false;
                quote_paragraph_open = false;
                line_start = next;
                continue;
            }

            if (line_starts_fenced_code(line_start)) {
                auto cursor = content_start;
                fence_marker = cps[cursor];
                while (cursor < line_end && cps[cursor] == fence_marker) ++cursor;
                fence_length = cursor - content_start;
                paragraph_open = false;
                quote_paragraph_open = false;
                line_start = next;
                continue;
            }

            if (cps[content_start] == U'>') {
                auto quote_cursor = content_start;
                bool quote_prefix = false;
                while (quote_cursor < line_end && cps[quote_cursor] == U'>') {
                    quote_prefix = true;
                    ++quote_cursor;
                    if (quote_cursor < line_end && (cps[quote_cursor] == U' ' || cps[quote_cursor] == U'\t')) ++quote_cursor;
                    std::size_t nested_indent = 0;
                    while (quote_cursor < line_end && cps[quote_cursor] == U' ' && nested_indent < 4) {
                        ++quote_cursor;
                        ++nested_indent;
                    }
                    if (nested_indent > 3) break;
                }
                if (quote_prefix) {
                    if (quote_fence_marker != 0) {
                        auto cursor = quote_cursor;
                        std::size_t indent = 0;
                        while (cursor < line_end && cps[cursor] == U' ' && indent < 4) { ++cursor; ++indent; }
                        const auto run_start = cursor;
                        while (cursor < line_end && cps[cursor] == quote_fence_marker) ++cursor;
                        const auto run_length = cursor - run_start;
                        while (cursor < line_end && (cps[cursor] == U' ' || cps[cursor] == U'\t')) ++cursor;
                        if (indent <= 3 && run_length >= quote_fence_length && cursor == line_end) {
                            quote_fence_marker = 0;
                            quote_fence_length = 0;
                        }
                        quote_paragraph_open = false;
                        paragraph_open = false;
                        line_start = next;
                        continue;
                    }
                    if (line_starts_fenced_code(quote_cursor)) {
                        auto cursor = quote_cursor;
                        while (cursor < line_end && cps[cursor] == U' ') ++cursor;
                        quote_fence_marker = cps[cursor];
                        const auto run_start = cursor;
                        while (cursor < line_end && cps[cursor] == quote_fence_marker) ++cursor;
                        quote_fence_length = cursor - run_start;
                        quote_paragraph_open = false;
                        paragraph_open = false;
                        line_start = next;
                        continue;
                    }
                    bool consumed_definition = false;
                    if (!quote_paragraph_open) {
                        if (auto definition = link_definition_at(quote_cursor)) {
                            link_definitions.try_emplace(definition->label, *definition);
                            consumed_definition = true;
                        }
                    }
                    const auto quote_blank = quote_cursor >= line_end;
                    const auto quote_interrupt = line_is_thematic_break(quote_cursor)
                        || setext_underline_at(quote_cursor)
                        || line_starts_atx_heading(quote_cursor)
                        || line_starts_fenced_code(quote_cursor)
                        || line_starts_block_html(quote_cursor)
                        || (input->dialect.math.block_dollar
                            && quote_cursor < line_end && cps[quote_cursor] == U'$'
                            && quote_cursor + 1 < line_end && cps[quote_cursor + 1] == U'$')
                        || (input->dialect.math.block_bracket
                            && quote_cursor < line_end && cps[quote_cursor] == U'\\'
                            && quote_cursor + 1 < line_end && cps[quote_cursor + 1] == U'[');
                    quote_paragraph_open = !consumed_definition && !quote_blank && !quote_interrupt;
                    paragraph_open = false;
                    line_start = next;
                    continue;
                }
            } else {
                quote_paragraph_open = false;
            }

            if (const auto html = html_block_start(line_start);
                html && markdown_html_block_can_interrupt(html->kind)) {
                line_start = html_block_end(line_start, html->kind);
                paragraph_open = false;
                continue;
            }

            if (!paragraph_open) {
                if (auto definition = link_definition_at(line_start)) {
                    link_definitions.try_emplace(definition->label, *definition);
                    link_definitions_by_start.emplace(line_start, *definition);
                    line_start = definition->source_end > line_start
                        ? definition->source_end
                        : next;
                    paragraph_open = false;
                    continue;
                }

                if (const auto html = html_block_start(line_start)) {
                    line_start = html_block_end(line_start, html->kind);
                    paragraph_open = false;
                    continue;
                }

            }

            if (line_is_thematic_break(line_start)
                || setext_underline_at(line_start)
                || line_starts_atx_heading(line_start)
                || line_starts_block_html(content_start)
                || cps[content_start] == U'>') {
                paragraph_open = false;
            } else {
                paragraph_open = true;
            }
            line_start = next;
        }
    }

    std::optional<HtmlTag> html_tag_at(std::size_t at, std::size_t limit) const {
        const auto parsed = scan_markdown_html_tag(cps, at, limit);
        if (!parsed) return std::nullopt;
        HtmlTag tag;
        tag.start = parsed->range.start;
        tag.end = parsed->range.end;
        tag.name = parsed->name;
        tag.closing = parsed->closing;
        tag.self_closing = parsed->self_closing;
        for (const auto& source_attribute : parsed->attributes) {
            auto attribute = cps_to_utf8(std::u32string_view(cps).substr(
                source_attribute.name.start,
                source_attribute.name.length()));
            std::ranges::transform(attribute, attribute.begin(), [](unsigned char cp) {
                return static_cast<char>(std::tolower(cp));
            });
            if (attribute.starts_with("on") || attribute == "style") continue;
            std::string value;
            if (source_attribute.value) {
                value = cps_to_utf8(std::u32string_view(cps).substr(
                    source_attribute.value->start,
                    source_attribute.value->length()));
            }
            tag.attributes[attribute] = std::move(value);
        }
        return tag;
    }

    std::optional<MarkdownHtmlBlockStart> html_block_start(std::size_t line_start) const {
        return scan_markdown_html_block_start(cps, line_start, line_content_end(line_start));
    }

    std::size_t html_block_end(std::size_t start, MarkdownHtmlBlockKind kind) const {
        auto cursor = start;
        while (cursor < cps.size()) {
            if (cursor != start && (kind == MarkdownHtmlBlockKind::BlockTag
                    || kind == MarkdownHtmlBlockKind::OtherTag) && is_blank_line_at(cursor)) {
                return cursor;
            }
            const auto line_end = line_content_end(cursor);
            const auto next = next_line_start(line_end);
            if (markdown_html_block_ends_on_line(kind, cps, cursor, line_end)) return next;
            if (next <= cursor) return line_end;
            cursor = next;
        }
        return cursor;
    }

    std::optional<std::pair<std::size_t, std::size_t>> html_closing_tag(std::string_view name, std::size_t from, std::size_t limit) const {
        std::size_t depth = 1;
        for (auto cursor = from; cursor < limit; ++cursor) {
            if (cps[cursor] != U'<') continue;
            auto tag = html_tag_at(cursor, limit);
            if (!tag || tag->name != name) continue;
            if (tag->closing) {
                if (--depth == 0) return std::pair{cursor, tag->end};
            } else if (!tag->self_closing) {
                ++depth;
            }
            cursor = tag->end - 1;
        }
        return std::nullopt;
    }

    static bool safe_link_target(std::string_view value, bool image) {
        std::size_t start = 0;
        std::size_t end = value.size();
        while (start < end && static_cast<unsigned char>(value[start]) <= 0x20) ++start;
        while (end > start && static_cast<unsigned char>(value[end - 1]) <= 0x20) --end;
        auto lower = std::string(value.substr(start, end - start));
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        auto colon = lower.find(':');
        auto boundary = lower.find_first_of("/?#");
        if (colon != std::string::npos && (boundary == std::string::npos || colon < boundary)) {
            auto scheme = lower.substr(0, colon);
            if (scheme == "http" || scheme == "https" || (!image && scheme == "mailto")) return true;
            return image && scheme == "data" && lower.starts_with("data:image/");
        }
        return true;
    }

    static bool ascii_punctuation(char32_t value) {
        return (value >= U'!' && value <= U'/') || (value >= U':' && value <= U'@') || (value >= U'[' && value <= U'`') || (value >= U'{' && value <= U'~');
    }

    static std::optional<float> html_dimension(HtmlTag const& tag, std::string_view name) {
        auto found = tag.attributes.find(std::string(name));
        if (found == tag.attributes.end()) return std::nullopt;
        auto value = found->second;
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
        if (value.size() >= 2 && value.ends_with("px")) value.resize(value.size() - 2);
        try {
            auto parsed = std::stof(value);
            if (!std::isfinite(parsed) || parsed <= 0.0f) return std::nullopt;
            return (std::min)(parsed, 4096.0f);
        } catch (...) {
            return std::nullopt;
        }
    }

    std::optional<std::pair<std::u32string, std::size_t>> html_entity_at(std::size_t at, std::size_t limit) const {
        if (at >= limit || cps[at] != U'&') return std::nullopt;
        auto end = at + 1;
        while (end < limit && end - at <= 12 && cps[end] != U';'
            && !is_line_ending_character(cps[end])) ++end;
        if (end >= limit || cps[end] != U';') return std::nullopt;
        auto entity = cps_to_utf8(std::u32string_view(cps).substr(at + 1, end - at - 1));
        if (entity == "amp") return std::pair{std::u32string(1, U'&'), end + 1};
        if (entity == "lt") return std::pair{std::u32string(1, U'<'), end + 1};
        if (entity == "gt") return std::pair{std::u32string(1, U'>'), end + 1};
        if (entity == "quot") return std::pair{std::u32string(1, U'"'), end + 1};
        if (entity == "apos" || entity == "#39") return std::pair{std::u32string(1, U'\''), end + 1};
        if (entity == "nbsp") return std::pair{std::u32string(1, U' '), end + 1};
        if (entity.starts_with("#")) {
            try {
                auto hex = entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X');
                auto value = std::stoul(entity.substr(hex ? 2 : 1), nullptr, hex ? 16 : 10);
                if (value <= 0x10ffff && !(0xd800 <= value && value <= 0xdfff)) return std::pair{std::u32string(1, static_cast<char32_t>(value)), end + 1};
            } catch (...) {}
        }
        return std::nullopt;
    }

    NodeId next_node_id() { return NodeId(node_counter++); }
    std::size_t cur() const { return std::size_t(pos); }
    bool eof() const { return pos >= cps.size(); }
    char32_t peek(std::size_t k = 0) const {
        return (pos + k < cps.size()) ? cps[pos + k] : 0;
    }
    // returns first of peek_n or 0
    char32_t peek1() const { return peek(0); }
    char32_t peek2(std::size_t k = 1) const { return peek(k); }
    void advance() { if (pos < cps.size()) ++pos; }
    void advance_n(std::size_t n) {
        pos = (pos + n > cps.size()) ? cps.size() : pos + n;
    }
    void report_progress() const {
        if (input->progress) input->progress((std::min)(pos, cps.size()), cps.size());
    }
    char32_t ch_at(std::size_t i) const { return (i < cps.size()) ? cps[i] : 0; }
    void skip_ws_inline() { while (peek1() == ' ' || peek1() == '\t') advance(); }
    BlockLineRecognizer line_recognizer() const { return BlockLineRecognizer(cps); }
    static bool is_line_ending_character(char32_t value) {
        return BlockLineRecognizer::is_line_ending_character(value);
    }
    std::size_t line_ending_length(std::size_t at) const {
        return line_recognizer().line_ending_length(at);
    }
    std::size_t line_content_end(std::size_t at) const {
        return line_recognizer().line_content_end(at);
    }
    bool consume_line_ending() {
        const auto length = line_ending_length(pos);
        if (length == 0) return false;
        advance_n(length);
        return true;
    }
    std::size_t next_line_start(std::size_t line_end) const {
        return line_recognizer().next_line_start(line_end);
    }
    bool is_line_start_at(std::size_t at) const {
        return line_recognizer().is_line_start_at(at);
    }
    bool peek_line_start() const {
        return is_line_start_at(pos);
    }
    bool is_blank_line() const {
        return is_blank_line_at(pos);
    }
    bool is_blank_line_at(std::size_t at) const {
        std::size_t i = at;
        while (i < cps.size() && !is_line_ending_character(cps[i])) {
            if (cps[i] != ' ' && cps[i] != '\t') return false;
            ++i;
        }
        return true;
    }
    bool line_starts_fenced_code(std::size_t at) const {
        std::size_t cursor = at;
        std::size_t indentation = 0;
        while (cursor < cps.size() && cps[cursor] == U' ' && indentation < 4) {
            ++cursor;
            ++indentation;
        }
        if (indentation > 3 || cursor >= cps.size()
            || (cps[cursor] != U'`' && cps[cursor] != U'~')) return false;
        const auto marker = cps[cursor];
        const auto fence_start = cursor;
        while (cursor < cps.size() && cps[cursor] == marker) ++cursor;
        if (cursor - fence_start < 3) return false;
        const auto line_end = line_content_end(cursor);
        if (marker == U'`') {
            for (auto i = cursor; i < line_end; ++i) {
                if (cps[i] == U'`') return false;
            }
        }
        return true;
    }
    bool line_starts_fenced_code() const {
        return peek_line_start() && line_starts_fenced_code(pos);
    }
    bool line_is_thematic_break(std::size_t start, std::size_t* end = nullptr) const {
        const auto line_end = line_content_end(start);
        auto cursor = start;
        std::size_t indentation = 0;
        while (cursor < line_end && cps[cursor] == U' ' && indentation < 4) {
            ++cursor;
            ++indentation;
        }
        if (indentation > 3 || cursor >= line_end) return false;
        const auto marker = cps[cursor];
        if (marker != U'-' && marker != U'*' && marker != U'_') return false;
        std::size_t count = 0;
        bool need_marker = false;
        while (cursor < line_end) {
            if (cps[cursor] == marker) {
                ++count;
                need_marker = false;
                ++cursor;
            } else if ((cps[cursor] == U' ' || cps[cursor] == U'\t') && !need_marker) {
                need_marker = true;
                while (cursor < line_end && (cps[cursor] == U' ' || cps[cursor] == U'\t')) ++cursor;
            } else {
                return false;
            }
        }
        if (count < 3) return false;
        if (end) *end = line_end;
        return true;
    }

    bool line_starts_atx_heading(std::size_t at) const {
        const auto line_end = line_content_end(at);
        const auto leading = scan_indentation(at, 3);
        auto cursor = leading.end;
        const auto indentation = leading.columns;
        if (indentation > 3 || cursor >= line_end || cps[cursor] != U'#') return false;
        std::size_t count = 0;
        while (cursor < line_end && cps[cursor] == U'#') { ++count; ++cursor; }
        return count <= 6
            && (cursor == line_end || cps[cursor] == U' ' || cps[cursor] == U'\t');
    }

    bool line_starts_blockquote(std::size_t at) const {
        const auto leading = scan_indentation(at, 3);
        auto cursor = leading.end;
        const auto indentation = leading.columns;
        return indentation <= 3 && cursor < cps.size() && cps[cursor] == U'>';
    }

    bool line_starts_ordered_list(std::size_t at, bool paragraph_open = true) const {
        auto cursor = at;
        if (cursor >= cps.size() || cps[cursor] < U'0' || cps[cursor] > U'9') return false;
        std::size_t digits = 0;
        while (cursor < cps.size() && cps[cursor] >= U'0' && cps[cursor] <= U'9' && digits < 10) {
            ++cursor;
            ++digits;
        }
        if (digits == 0 || digits > 9 || cursor >= cps.size()
            || (cps[cursor] != U'.' && cps[cursor] != U')')) return false;
        // Only an ordered list starting at 1 may interrupt a paragraph.
        if (paragraph_open && (digits != 1 || cps[at] != U'1')) return false;
        ++cursor;
        const auto line_end = line_content_end(at);
        if (cursor == line_end) return !paragraph_open;
        if (cps[cursor] != U' ' && cps[cursor] != U'\t') return false;
        while (cursor < line_end && (cps[cursor] == U' ' || cps[cursor] == U'\t')) ++cursor;
        // An empty ordered item does not interrupt an open paragraph.
        return !paragraph_open || cursor < line_end;
    }
    using SetextUnderline = BlockLineRecognizer::SetextUnderline;
    std::optional<SetextUnderline> setext_underline_at(std::size_t start) const {
        const auto line_end = line_content_end(start);
        auto cursor = start;
        std::size_t indentation = 0;
        while (cursor < line_end && cps[cursor] == U' ' && indentation < 4) {
            ++cursor;
            ++indentation;
        }
        if (indentation > 3 || cursor >= line_end) return std::nullopt;
        const auto marker = cps[cursor];
        if (marker != U'=' && marker != U'-') return std::nullopt;
        while (cursor < line_end && cps[cursor] == marker) ++cursor;
        const auto count = cursor - (start + indentation);
        while (cursor < line_end && (cps[cursor] == U' ' || cps[cursor] == U'\t')) ++cursor;
        if (cursor != line_end || count == 0) return std::nullopt;
        return SetextUnderline{static_cast<std::uint8_t>(marker == U'=' ? 1 : 2), line_end};
    }
    bool line_starts_block_html(std::size_t at) const {
        return line_recognizer().line_starts_block_html(at);
    }
    bool line_starts_block_html() const {
        return peek_line_start() && line_starts_block_html(pos);
    }
    bool line_starts_interrupting_block(std::size_t at, bool paragraph_open = true) const {
        const auto leading = scan_indentation(at, 3);
        auto cursor = leading.end;
        const auto indentation = leading.columns;
        if (indentation > 3) return false;
        return (paragraph_open && (setext_underline_at(at).has_value() || line_starts_gfm_table(at)))
            || line_is_thematic_break(at)
            || (paragraph_open ? line_starts_block_html(cursor) : html_block_start(at).has_value())
            || ch_at(cursor) == U'>'
            || line_starts_fenced_code(at)
            || line_starts_atx_heading(at)
            || (input->dialect.math.block_dollar
                && ch_at(cursor) == U'$' && ch_at(cursor + 1) == U'$')
            || (input->dialect.math.block_bracket
                && ch_at(cursor) == U'\\' && ch_at(cursor + 1) == U'[')
            || ((ch_at(cursor) == U'-' || ch_at(cursor) == U'+' || ch_at(cursor) == U'*')
                && (ch_at(cursor + 1) == U' ' || ch_at(cursor + 1) == U'\t'
                    || (!paragraph_open && cursor + 1 == line_content_end(at))))
            || line_starts_ordered_list(cursor, paragraph_open);
    }
    bool line_starts_interrupting_block() const {
        return peek_line_start() && line_starts_interrupting_block(pos);
    }

    // Returns whether a physical line, after iteratively removing only
    // blockquote/list/task prefixes, ends in paragraph content. This is used
    // to carry paragraph-open state across container boundaries without
    // rewriting the source that nested parsers receive.
    bool line_has_leaf_paragraph_content(
        std::size_t start,
        std::size_t line_end,
        bool allow_container_prefixes = true) const {
        auto cursor = start;
        while (cursor < line_end) {
            const auto indentation = scan_indentation(cursor);
            if (indentation.columns > 3 || indentation.end >= line_end) return false;
            cursor = indentation.end;

            if (cps[cursor] == U'>') {
                if (!allow_container_prefixes) return false;
                ++cursor;
                if (cursor < line_end && (cps[cursor] == U' ' || cps[cursor] == U'\t')) ++cursor;
                continue;
            }

            // Block thematic breaks take precedence over the bullet grammar.
            if (line_is_thematic_break(cursor)) return false;

            std::size_t marker_end = cursor;
            bool list_marker = false;
            const auto marker = cps[cursor];
            if (marker == U'-' || marker == U'+' || marker == U'*') {
                marker_end = cursor + 1;
                list_marker = marker_end == line_end
                    || cps[marker_end] == U' '
                    || cps[marker_end] == U'\t';
            } else if (is_ascii_digit_(marker)) {
                std::size_t digits = 0;
                while (marker_end < line_end && is_ascii_digit_(cps[marker_end]) && digits < 10) {
                    ++marker_end;
                    ++digits;
                }
                list_marker = digits > 0 && digits <= 9 && marker_end < line_end
                    && (cps[marker_end] == U'.' || cps[marker_end] == U')');
                if (list_marker) {
                    ++marker_end;
                    list_marker = marker_end == line_end
                        || cps[marker_end] == U' '
                        || cps[marker_end] == U'\t';
                }
            }
            if (!list_marker) break;
            if (!allow_container_prefixes) return false;

            auto column = indentation.physical_column + marker_end - cursor;
            const auto padding_start_column = column;
            cursor = marker_end;
            while (cursor < line_end && (cps[cursor] == U' ' || cps[cursor] == U'\t')) {
                column += cps[cursor] == U'\t' ? 4 - column % 4 : 1;
                ++cursor;
            }
            if (column - padding_start_column > 4) return false;
            if (cursor >= line_end) return false;
            if (input->dialect.gfm.task_lists
                && (marker == U'-' || marker == U'+' || marker == U'*')
                && cursor + 2 < line_end
                && cps[cursor] == U'['
                && (cps[cursor + 1] == U' ' || cps[cursor + 1] == U'x' || cps[cursor + 1] == U'X')
                && cps[cursor + 2] == U']') {
                cursor += 3;
                if (cursor < line_end && (cps[cursor] == U' ' || cps[cursor] == U'\t')) ++cursor;
                if (cursor >= line_end) return false;
            }
        }

        if (cursor >= line_end) return false;

        if (input->dialect.callouts && cps[cursor] == U'['
            && cursor + 1 < line_end && cps[cursor + 1] == U'!') {
            auto kind_end = cursor + 2;
            while (kind_end < line_end && cps[kind_end] != U']') ++kind_end;
            if (kind_end < line_end
                && normalize_callout_kind(cps_to_utf8(std::u32string_view(cps).substr(
                    cursor + 2, kind_end - cursor - 2)))) return false;
        }
        if (link_definition_at(cursor)) return false;
        if (input->dialect.footnotes && cursor + 3 < line_end
            && cps[cursor] == U'[' && cps[cursor + 1] == U'^') {
            auto label_end = cursor + 2;
            while (label_end < line_end && cps[label_end] != U']') ++label_end;
            if (label_end < line_end && label_end + 1 < line_end && cps[label_end + 1] == U':') return false;
        }
        if ((input->dialect.toc.bracket_toc && line_end - cursor >= 5
                && std::u32string_view(cps).substr(cursor, 5) == U"[TOC]")
            || (input->dialect.toc.wiki_toc && line_end - cursor >= 7
                && std::u32string_view(cps).substr(cursor, 7) == U"[[toc]]")) return false;

        return !setext_underline_at(cursor).has_value()
            && !line_starts_interrupting_block(cursor);
    }

    bool is_lazy_line(std::size_t at) const {
        return std::ranges::find(input->lazy_lines, at) != input->lazy_lines.end();
    }

    struct PrefixLine {
        std::size_t inner_start = 0;
        std::u32string text;
        bool lazy = false;
        ContainerPrefixRole role = ContainerPrefixRole::Literal;
    };

    // Retain only transient physical mappings while extracting nested inputs.
    // Durable prefix anchors use the existing block's source or syntax fields.
    ParserSourceRange* source_range_for(NodeId id) {
        // parse_block appends the completed outer block range after any nested
        // child ranges. Most callers have just completed that block, so avoid
        // searching all earlier ranges on the common path.
        if (!source_ranges.empty() && source_ranges.back().node_id == id) {
            return &source_ranges.back();
        }
        const auto found = std::ranges::find_if(source_ranges, [&](const auto& range) {
            return range.node_id == id;
        });
        return found == source_ranges.end() ? nullptr : &*found;
    }

    void record_owned_offsets(const BlockNode& block) {
        const std::u32string* source = editable_inline_document(block)
            ? &block.inline_content.source : editable_raw_block_source(block);
        if (!source) return;
        auto* range = source_range_for(block.id);
        if (!range || !range->owned_offsets.empty()) return;
        const auto begin = editable_inline_document(block)
            ? range->content_range.start : range->source_range.start;
        if (begin + source->size() > cps.size()
            || std::u32string_view(cps).substr(begin, source->size()) != *source) return;
        range->owned_offsets.resize(source->size() + 1);
        std::iota(range->owned_offsets.begin(), range->owned_offsets.end(), begin);
    }

    static ContainerSource bind_prefix_lines(
        std::u32string_view inner,
        const std::vector<ParserSourceRange>& ranges,
        const std::vector<PrefixLine>& lines) {
        ContainerSource result;
        std::vector<std::optional<ContainerPrefixAnchor>> source_anchors(lines.size());
        std::vector<std::size_t> nearest(lines.size(), (std::numeric_limits<std::size_t>::max)());
        std::vector<std::size_t> ends;
        ends.reserve(lines.size());
        for (const auto& line : lines) {
            auto end = line.inner_start;
            while (end < inner.size() && inner[end] != U'\n' && inner[end] != U'\r') ++end;
            ends.push_back(end);
        }
        // Index source anchors once; repeatedly scanning a long paragraph for
        // every container line would make large quoted documents quadratic.
        for (const auto& range : ranges) {
            const auto count = range.owned_offsets.size();
            for (std::size_t i = 0; i < count; ++i) {
                if (i + 1 == count && count > 1) break;
                const auto physical = range.owned_offsets[i];
                const auto next = std::ranges::upper_bound(lines, physical, {}, &PrefixLine::inner_start);
                if (next == lines.begin()) continue;
                const auto line = static_cast<std::size_t>(next - lines.begin() - 1);
                if (physical > ends[line] || physical >= nearest[line]) continue;
                nearest[line] = physical;
                source_anchors[line] = ContainerPrefixAnchor{range.node_id, ContainerAnchorPart::Source, i};
            }
        }
        for (std::size_t index = 0; index < lines.size(); ++index) {
            const auto& line = lines[index];
            const auto end = ends[index];
            auto anchor = source_anchors[index];
            if (!anchor) {
                for (const auto& range : ranges) {
                    if (!range.separator_range) continue;
                    const auto gap = *range.separator_range;
                    if (gap.start <= line.inner_start && line.inner_start < gap.end) {
                        anchor = ContainerPrefixAnchor{range.node_id, ContainerAnchorPart::Separator,
                            line.inner_start - gap.start};
                        break;
                    }
                }
            }
            if (!anchor) {
                auto width = (std::numeric_limits<std::size_t>::max)();
                for (const auto& range : ranges) {
                    const auto raw = range.source_range;
                    if (raw.start > end || raw.end < line.inner_start) continue;
                    const auto size = raw.end - raw.start;
                    if (size >= width) continue;
                    width = size;
                    anchor = ContainerPrefixAnchor{range.node_id,
                        raw.start >= line.inner_start ? ContainerAnchorPart::Start : ContainerAnchorPart::End, 0};
                }
            }
            if (anchor) result.lines.push_back({*anchor, line.text, line.lazy, line.role});
        }
        return result;
    }

    // Read the current physical line without its CRLF/CR/LF terminator.
    std::pair<std::u32string, std::size_t> rest_of_line() const {
        const auto end = line_content_end(pos);
        return {std::u32string(std::u32string_view(cps).substr(pos, end - pos)), end - pos};
    }
    // Push a source range. idempotent.
    void push_range(NodeId id, PhysicalRange sr, PhysicalRange cr) {
        ParserSourceRange r(id, sr, cr);
        source_ranges.push_back(r);
    }
    // ---- block dispatch ----
    void parse_blank_lines(
        std::vector<BlockNode>& blocks,
        std::optional<std::size_t>& preceding_source_end) {
        struct BlankLineRange {
            std::size_t start{};
            std::size_t content_end{};
            std::size_t end{};
        };
        std::vector<BlankLineRange> blank_ranges;
        while (!eof() && is_blank_line()) {
            const auto start = cur();
            while (peek1() == ' ' || peek1() == '\t') advance();
            const auto content_end = cur();
            consume_line_ending();
            blank_ranges.push_back({start, content_end, cur()});
        }
        // A blank physical line between two non-empty blocks is Markdown's
        // block separator, not a third editable block.  Keep its exact bytes
        // in the following block's separator_before.  Additional blank lines
        // are genuine editable empty paragraphs.  At document boundaries no
        // following/preceding content absorbs a separator, so every physical
        // blank line keeps block identity.
        const auto first_editable_blank = !eof()
            && !blocks.empty()
            && preceding_source_end
            && !(blocks.back().kind == BlockKind::Paragraph
                && blocks.back().inline_content.source.empty())
            ? 1u
            : 0u;
        for (std::size_t index = first_editable_blank; index < blank_ranges.size(); ++index) {
            BlockNode paragraph;
            paragraph.id = next_node_id();
            paragraph.kind = BlockKind::Paragraph;
            if (!blocks.empty() && preceding_source_end) {
                paragraph.separator_before = std::u32string(
                    std::u32string_view(cps).substr(
                        *preceding_source_end,
                        blank_ranges[index].start - *preceding_source_end));
            }
            paragraph.inline_content = make_inline_document(
                blank_ranges[index].start,
                blank_ranges[index].content_end);
            push_range(
                paragraph.id,
                PhysicalRange(blank_ranges[index].start, blank_ranges[index].end),
                PhysicalRange(blank_ranges[index].start, blank_ranges[index].content_end));
            if (paragraph.separator_before) {
                source_ranges.back().separator_range = PhysicalRange{
                    *preceding_source_end, blank_ranges[index].start};
            }
            record_owned_offsets(paragraph);
            blocks.push_back(std::move(paragraph));
            preceding_source_end = blank_ranges[index].content_end;
        }
    }

    std::vector<BlockNode> parse_blocks(std::function<bool(const std::u32string&)> stop = nullptr) {
        std::vector<BlockNode> blocks;
        std::optional<std::size_t> preceding_source_end;
        while (!eof()) {
            if (is_blank_line()) {
                parse_blank_lines(blocks, preceding_source_end);
                report_progress();
                continue;
            }
            if (stop) {
                auto [line, _] = rest_of_line();
                if (stop(line)) break;
            }
            const auto block_start = cur();
            if (auto b = parse_block()) {
                if (!blocks.empty() && preceding_source_end) {
                    b->separator_before = std::u32string(
                        std::u32string_view(cps).substr(
                            *preceding_source_end,
                            block_start - *preceding_source_end));
                }
                if (b->separator_before) {
                    if (auto* mapped = source_range_for(b->id)) {
                        mapped->separator_range = PhysicalRange{*preceding_source_end, block_start};
                    }
                }
                record_owned_offsets(*b);
                auto block_end = cur();
                const auto serialized = serializer_detail::serialize_block(*b);
                if (block_end > block_start
                    && is_line_ending_character(cps[block_end - 1])
                    && (serialized.text.empty()
                        || !is_line_ending_character(serialized.text.back()))) {
                    --block_end;
                    if (block_end > block_start
                        && cps[block_end] == U'\n'
                        && cps[block_end - 1] == U'\r') --block_end;
                }
                blocks.push_back(std::move(*b));
                preceding_source_end = block_end;
            } else {
                advance();
            }
            report_progress();
        }
        return blocks;
    }

    bool is_callout_marker_line(const std::u32string& line) const {
        // detect "> [!KIND]" already-without-">"? used to stop block body
        // we just strip leading whitespace
        std::size_t i = 0; while (i < line.size() && (line[i]==' '||line[i]=='\t')) ++i;
        return false;
    }

    std::optional<BlockNode> parse_block() {
        // The dispatch order matches HANDOFF closely.
        bool ls = peek_line_start();
        if (ls) {
            if (auto b = try_parse_frontmatter())        return b;
            if (peek1() == '[' && ch_at(pos+1) == '^') {
                if (auto b = try_parse_footnote_definition()) return b;
            }
            if (auto b = try_parse_link_definition())    return b;
            if (line_starts_atx_heading(pos)) {
                if (auto b = parse_heading()) return b;
            }
            if (line_starts_blockquote(pos)) {
                if (auto b = parse_blockquote_or_callout()) return b;
            }
            if (auto b = try_parse_indented_code_block()) return b;
            if (auto b = try_parse_code_block())         return b;
            if (auto b = try_parse_math_block())         return b;
            if (auto b = try_parse_image_block())        return b;
            if (auto b = try_parse_table())              return b;
            if (auto b = try_parse_thematic_break())     return b;
            if (auto b = try_parse_list_or_task())       return b;
            if (try_match_toc())                          return parse_toc();
        }
        if (html_block_start(pos)) {
            if (auto b = try_parse_raw_html_block())     return b;
        }
        return parse_paragraph();
    }

    // ---- frontmatter ----
    std::optional<BlockNode> try_parse_frontmatter() {
        if (pos != 0) return std::nullopt;
        auto [opening_line, opening_length] = rest_of_line();
        auto opening = trim_utf8(cps_to_utf8(opening_line));
        FrontmatterFormat fmt = FrontmatterFormat::Yaml;
        if (opening == "---") {
            if (!input->dialect.frontmatter.yaml) return std::nullopt;
            fmt = FrontmatterFormat::Yaml;
        } else if (opening == "---toml" || opening == "--- toml") {
            if (!input->dialect.frontmatter.toml) return std::nullopt;
            fmt = FrontmatterFormat::Toml;
        } else if (opening == "---json" || opening == "--- json") {
            if (!input->dialect.frontmatter.json) return std::nullopt;
            fmt = FrontmatterFormat::Json;
        }
        else return std::nullopt;
        std::size_t save = pos;
        advance_n(opening_length);
        consume_line_ending();
        while (true) {
            if (eof()) { pos = save; return std::nullopt; }
            auto [closing_line, closing_length] = rest_of_line();
            if (peek_line_start() && trim_utf8(cps_to_utf8(closing_line)) == "---") {
                std::size_t closing_start = pos;
                advance_n(closing_length);
                consume_line_ending();
                std::size_t content_end = closing_start;
                // slice by CHAR indices into UTF8 — safe for CJK
                std::size_t b1 = char_to_byte_in_input_(save), b2 = char_to_byte_in_input_(save + 3);
                std::size_t eC = char_to_byte_in_input_(content_end);
                std::string inner = input->text.substr(b2, (b1 < eC ? eC : b1) - b2);
                NodeId id = next_node_id();
                push_range(id, PhysicalRange(std::size_t(save), std::size_t(pos)), PhysicalRange(std::size_t(save), std::size_t(pos)));
                BlockNode b; b.id = id; b.kind = BlockKind::Frontmatter; b.ensure_atomic_special().fmt = fmt; b.ensure_atomic_special().raw = inner;
                return b;
            }
            if (!consume_line_ending()) advance();
        }
    }

    // ---- heading ----
    std::optional<BlockNode> parse_heading() {
        std::size_t start = pos;
        std::size_t indentation = 0;
        while (peek1() == U' ' && indentation < 4) { advance(); ++indentation; }
        if (indentation > 3) { pos = start; return std::nullopt; }
        std::uint8_t level = 0;
        while (peek1() == '#' && level < 6) { advance(); ++level; }
        if (peek1() != U' ' && peek1() != U'\t'
            && !eof() && !is_line_ending_character(peek1())) {
            pos = start;
            return std::nullopt;
        }
        while (peek1() == U' ' || peek1() == U'\t') advance();
        std::size_t content_start = pos;
        const auto line_end = line_content_end(pos);
        std::size_t trimmed_end = line_end;
        while (trimmed_end > content_start && (cps[trimmed_end - 1] == U' ' || cps[trimmed_end - 1] == U'\t')) --trimmed_end;
        std::size_t content_end = trimmed_end;
        std::size_t closing_start = trimmed_end;
        while (closing_start > content_start && cps[closing_start - 1] == U'#') --closing_start;
        if (closing_start < trimmed_end
            && (closing_start == content_start
                || cps[closing_start - 1] == U' ' || cps[closing_start - 1] == U'\t')) {
            content_end = closing_start;
            while (content_end > content_start && (cps[content_end - 1] == U' ' || cps[content_end - 1] == U'\t')) --content_end;
        } else {
            closing_start = line_end;
            // Without a closing hash sequence, trailing whitespace is part
            // of the heading's exact inline source and must remain editable.
            content_end = line_end;
        }
        pos = line_end;
        consume_line_ending();
        auto inline_content = make_inline_document(content_start, content_end);
        std::u32string title = trim_cps_(inline_visible_text(inline_content));
        std::string title_utf8 = cps_to_utf8(title);
        std::string slug = generate_slug(title_utf8, {});
        NodeId id = next_node_id();
        ParserSourceRange range(id, PhysicalRange(std::size_t(start), std::size_t(pos)), PhysicalRange(std::size_t(content_start), std::size_t(content_end)));
        range.marker_ranges.push_back(PhysicalRange(std::size_t(start), std::size_t(content_start)));
        if (content_end < line_end) range.marker_ranges.push_back(PhysicalRange(std::size_t(content_end), std::size_t(line_end)));
        source_ranges.push_back(std::move(range));
        BlockNode b; b.id = id; b.kind = BlockKind::Heading; b.ensure_text_special().level = level; b.inline_content = std::move(inline_content); b.ensure_text_special().slug = slug;
        b.ensure_text_special().opening_marker = std::u32string(std::u32string_view(cps).substr(start, content_start - start));
        if (content_end < line_end) b.ensure_text_special().closing_marker = std::u32string(std::u32string_view(cps).substr(content_end, line_end - content_end));
        return b;
    }

    // The block parser owns paragraph boundaries only. Inline syntax is
    // parsed exactly once into the node-local lossless CST.
    std::optional<BlockNode> try_parse_link_definition() {
        const auto found = link_definitions_by_start.find(pos);
        if (found == link_definitions_by_start.end()) return std::nullopt;
        const auto start = pos;
        const auto& definition = found->second;
        pos = definition.source_end;
        BlockNode block;
        block.id = next_node_id();
        block.kind = BlockKind::LinkDefinition;
        block.ensure_atomic_special().raw = cps_to_utf8(std::u32string_view(cps).substr(start, definition.source_end - start));
        push_range(block.id, PhysicalRange(std::size_t(start), std::size_t(pos)), PhysicalRange(std::size_t(start), std::size_t(definition.source_end)));
        return block;
    }

    std::optional<BlockNode> parse_paragraph(std::optional<std::size_t> stop_at = std::nullopt) {
        const auto start = pos;
        auto content_end = pos;
        const auto limit = (std::min)(cps.size(), stop_at.value_or(cps.size()));
        while (pos < limit) {
            if (!is_line_ending_character(peek1())) {
                advance();
                content_end = pos;
                continue;
            }
            const auto newline = pos;
            consume_line_ending();
            if (pos >= limit || is_line_ending_character(peek1())
                || is_blank_line_at(pos) || (!is_lazy_line(pos) && line_starts_interrupting_block(pos))) {
                const auto hard_break = (newline >= start + 2 && cps[newline - 1] == U' ' && cps[newline - 2] == U' ')
                    || (newline > start && cps[newline - 1] == U'\\');
                content_end = hard_break ? pos : newline;
                break;
            }
            content_end = pos;
        }
        if (content_end == start) return std::nullopt;

        if (const auto underline = is_lazy_line(pos) ? std::nullopt : setext_underline_at(pos)) {
            auto body_start = start;
            std::size_t indentation = 0;
            while (body_start < content_end && cps[body_start] == U' ' && indentation < 4) {
                ++body_start;
                ++indentation;
            }
            if (indentation > 3) body_start = start;
            pos = underline->line_end;
            consume_line_ending();
            auto body_end = content_end;
            if (body_end > body_start && cps[body_end - 1] == U'\n') {
                --body_end;
                if (body_end > body_start && cps[body_end - 1] == U'\r') --body_end;
            } else if (body_end > body_start && cps[body_end - 1] == U'\r') {
                --body_end;
            }
            while (body_end > body_start && (cps[body_end - 1] == U' ' || cps[body_end - 1] == U'\t')) --body_end;
            auto inline_content = make_inline_document(body_start, body_end);
            auto title_utf8 = cps_to_utf8(inline_visible_text(inline_content));
            auto slug = generate_slug(title_utf8, {});
            const auto id = next_node_id();
            ParserSourceRange range(id, PhysicalRange(std::size_t(start), std::size_t(pos)), PhysicalRange(std::size_t(body_start), std::size_t(body_end)));
            if (body_start > start) range.marker_ranges.push_back(PhysicalRange(std::size_t(start), std::size_t(body_start)));
            range.marker_ranges.push_back(PhysicalRange(std::size_t(body_end), std::size_t(underline->line_end)));
            source_ranges.push_back(std::move(range));
            BlockNode heading;
            heading.id = id;
            heading.kind = BlockKind::Heading;
            heading.ensure_text_special().level = underline->level;
            heading.inline_content = std::move(inline_content);
            heading.ensure_text_special().slug = std::move(slug);
            if (body_start > start) heading.ensure_text_special().opening_marker = std::u32string(std::u32string_view(cps).substr(start, body_start - start));
            heading.ensure_text_special().closing_marker = std::u32string(std::u32string_view(cps).substr(body_end, underline->line_end - body_end));
            return heading;
        }

        const auto id = next_node_id();
        push_range(id, PhysicalRange(std::size_t(start), std::size_t(pos)), PhysicalRange(std::size_t(start), std::size_t(content_end)));
        BlockNode paragraph;
        paragraph.id = id;
        paragraph.kind = BlockKind::Paragraph;
        paragraph.inline_content = make_inline_document(start, content_end);
        return paragraph;
    }

    std::optional<BlockNode> try_parse_image_block() {
        const auto start = pos;
        const auto line_end = line_content_end(pos);
        if (line_end == start) return std::nullopt;

        if (peek1() == U'<') {
            const auto tag = html_tag_at(start, line_end);
            if (tag && !tag->closing && tag->name == "img" && tag->end == line_end) {
                const auto source = tag->attributes.contains("src") ? tag->attributes.at("src") : std::string{};
                if (!safe_link_target(source, true)) return std::nullopt;
                auto dimension = [&](std::string_view name) -> std::optional<ImageDimension> {
                    const auto found = tag->attributes.find(std::string(name));
                    if (found == tag->attributes.end()) return std::nullopt;
                    return parse_html_image_dimension(found->second);
                };
                pos = line_end;
                consume_line_ending();
                BlockNode block;
                block.id = next_node_id();
                block.kind = BlockKind::ImageBlock;
                block.ensure_image_special().src = source;
                block.ensure_image_special().image_alt = tag->attributes.contains("alt") ? tag->attributes.at("alt") : std::string{};
                if (tag->attributes.contains("title")) block.ensure_image_special().image_title = tag->attributes.at("title");
                block.ensure_image_special().image_width = dimension("width");
                block.ensure_image_special().image_height = dimension("height");
                auto raw = std::u32string(std::u32string_view(cps).substr(
                    start,
                    line_end - start));
                auto& html = block.ensure_html_special();
                html.source = raw;
                html.tree = parse_html_cst(raw);
                html.structure_shape = html_block_structure_shape(block);
                html.root_tag = "img";
                push_range(block.id, PhysicalRange(std::size_t(start), std::size_t(pos)), PhysicalRange(std::size_t(start), std::size_t(line_end)));
                return block;
            }
        }

        auto inline_document = make_inline_document(start, line_end);
        if (inline_document.tree.nodes.size() != 1) return std::nullopt;
        const auto& root = inline_document.tree.nodes.front();
        const InlineCstNode* image = nullptr;
        std::optional<std::string> link;
        if (root.kind == InlineCstKind::Image) {
            image = &root;
        } else if (root.kind == InlineCstKind::Link && root.children.size() == 1
            && root.children.front().kind == InlineCstKind::Image) {
            image = &root.children.front();
            link = root.semantics().href;
        }
        if (!image || image->status != ParseStatus::Complete) return std::nullopt;

        pos = line_end;
        consume_line_ending();
        BlockNode block;
        block.id = next_node_id();
        block.kind = BlockKind::ImageBlock;
        block.ensure_image_special().src = image->semantics().href;
        block.ensure_image_special().image_alt = image->semantics().alt;
        block.ensure_image_special().image_title = image->semantics().title;
        block.ensure_image_special().image_link = std::move(link);
        block.inline_content = std::move(inline_document);
        push_range(block.id, PhysicalRange(std::size_t(start), std::size_t(pos)), PhysicalRange(std::size_t(start), std::size_t(line_end)));
        return block;
    }

    static bool html_whitespace_only(std::u32string_view value) {
        return std::ranges::all_of(value, [](char32_t cp) {
            return cp == U' ' || cp == U'\t' || cp == U'\r' || cp == U'\n';
        });
    }

    static bool html_contains_unsafe(const HtmlCstNode& node) {
        if (node.kind == HtmlCstKind::Element && html_is_unsafe_element(node.tag_name)) {
            return true;
        }
        return std::ranges::any_of(node.children, [](const auto& child) {
            return html_contains_unsafe(child);
        });
    }

    std::optional<std::u32string> html_attribute_value(
        const HtmlCstNode& node,
        std::string_view name,
        std::size_t source_start) const {
        const auto found = std::ranges::find_if(node.attributes, [&](const auto& attribute) {
            return attribute.name == name;
        });
        if (found == node.attributes.end() || !found->value_range) return std::nullopt;
        const auto range = *found->value_range;
        if (source_start + range.end > cps.size()) return std::nullopt;
        return std::u32string(std::u32string_view(cps).substr(
            source_start + range.start,
            range.length()));
    }

    static std::string_view trim_html_css_value(std::string_view value) {
        while (!value.empty() && static_cast<unsigned char>(value.front()) <= 0x20) {
            value.remove_prefix(1);
        }
        while (!value.empty() && static_cast<unsigned char>(value.back()) <= 0x20) {
            value.remove_suffix(1);
        }
        return value;
    }

    static std::string lower_html_css_value(std::string_view value) {
        std::string result(value);
        std::ranges::transform(result, result.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return result;
    }

    static std::optional<TextAlignment> parse_html_text_alignment(std::string_view value) {
        value = trim_html_css_value(value);
        auto lower = lower_html_css_value(value);
        if (const auto important = lower.find("!important"); important != std::string::npos) {
            lower = std::string(trim_html_css_value(std::string_view(lower).substr(0, important)));
        }
        if (lower == "left" || lower == "start") return TextAlignment::Start;
        if (lower == "center") return TextAlignment::Center;
        if (lower == "right" || lower == "end") return TextAlignment::End;
        if (lower == "justify") return TextAlignment::Justify;
        return std::nullopt;
    }

    std::optional<TextAlignment> html_text_alignment(
        const HtmlCstNode& node,
        std::size_t source_start) const {
        std::optional<TextAlignment> result;
        if (auto align = html_attribute_value(node, "align", source_start)) {
            result = parse_html_text_alignment(cps_to_utf8(*align));
        }
        auto style = html_attribute_value(node, "style", source_start);
        if (!style) return result;

        // CSS declarations cascade in source order. Only the safe enumerated
        // text-align property is projected; the exact style spelling remains
        // owned by the lossless HTML CST.
        const auto style_utf8 = cps_to_utf8(*style);
        auto declarations = std::string_view(style_utf8);
        while (!declarations.empty()) {
            const auto semicolon = declarations.find(';');
            auto declaration = trim_html_css_value(declarations.substr(0, semicolon));
            declarations = semicolon == std::string_view::npos
                ? std::string_view{}
                : declarations.substr(semicolon + 1);
            const auto colon = declaration.find(':');
            if (colon == std::string_view::npos) continue;
            auto property = lower_html_css_value(trim_html_css_value(declaration.substr(0, colon)));
            if (property != "text-align") continue;
            if (auto parsed = parse_html_text_alignment(declaration.substr(colon + 1))) {
                result = parsed;
            }
        }
        return result;
    }

    void push_html_range(
        NodeId id,
        const HtmlCstNode& node,
        std::size_t source_start) {
        ParserSourceRange range(
            id,
            {source_start + node.range.start, source_start + node.range.end},
            {source_start + node.content.start, source_start + node.content.end});
        if (!node.opening.empty()) {
            range.marker_ranges.push_back({
                source_start + node.opening.start,
                source_start + node.opening.end});
        }
        if (node.closing) {
            range.marker_ranges.push_back({
                source_start + node.closing->start,
                source_start + node.closing->end});
        }
        source_ranges.push_back(std::move(range));
    }

    void attach_html_element(
        BlockNode& block,
        const HtmlCstNode& node,
        std::size_t source_start) {
        auto& html = block.ensure_html_special();
        html.root_tag = node.tag_name;
        html.text_alignment = html_text_alignment(node, source_start);
        if (node.opening.valid_for(cps.size() - source_start)) {
            html.opening_marker = cps.substr(
                source_start + node.opening.start,
                node.opening.length());
        }
        if (node.closing && node.closing->valid_for(cps.size() - source_start)) {
            html.closing_marker = cps.substr(
                source_start + node.closing->start,
                node.closing->length());
        }
    }

    BlockNode make_html_inline_block(
        BlockKind kind,
        const HtmlCstNode& node,
        std::size_t source_start,
        std::vector<HtmlContentSlot>& slots,
        std::uint8_t heading_level = 0) {
        BlockNode block;
        block.id = next_node_id();
        block.kind = kind;
        block.inline_content = make_inline_document(
            source_start + node.content.start,
            source_start + node.content.end,
            InlineSyntaxMode::HtmlText);
        if (heading_level != 0) block.ensure_text_special().level = heading_level;
        attach_html_element(block, node, source_start);
        push_html_range(block.id, node, source_start);
        slots.push_back({block.id, node.content});
        return block;
    }

    BlockNode make_html_inline_run(
        SourceRange range,
        std::size_t source_start,
        std::vector<HtmlContentSlot>& slots) {
        BlockNode block;
        block.id = next_node_id();
        block.kind = BlockKind::Paragraph;
        block.inline_content = make_inline_document(
            source_start + range.start,
            source_start + range.end,
            InlineSyntaxMode::HtmlText);
        push_range(
            block.id,
            {source_start + range.start, source_start + range.end},
            {source_start + range.start, source_start + range.end});
        slots.push_back({block.id, range});
        return block;
    }

    void collect_html_rows(
        const HtmlCstNode& node,
        std::vector<const HtmlCstNode*>& rows,
        bool root = true) const {
        for (const auto& child : node.children) {
            if (child.kind != HtmlCstKind::Element) continue;
            if (child.tag_name == "tr") {
                rows.push_back(&child);
            } else if (child.tag_name != "table" || root) {
                collect_html_rows(child, rows, false);
            }
        }
    }

    std::optional<BlockNode> make_html_table(
        const HtmlCstNode& node,
        std::size_t source_start,
        std::vector<HtmlContentSlot>& slots) {
        std::vector<const HtmlCstNode*> html_rows;
        collect_html_rows(node, html_rows);
        if (html_rows.empty()) return std::nullopt;

        BlockNode table;
        table.id = next_node_id();
        table.kind = BlockKind::Table;
        std::size_t columns = 0;
        for (const auto* html_row : html_rows) {
            BlockNode row;
            row.id = next_node_id();
            row.kind = BlockKind::TableRow;
            bool header = false;
            for (const auto& html_cell : html_row->children) {
                if (html_cell.kind != HtmlCstKind::Element
                    || (html_cell.tag_name != "td" && html_cell.tag_name != "th")) {
                    continue;
                }
                header = header || html_cell.tag_name == "th";
                row.children.push_back(make_html_inline_block(
                    BlockKind::TableCell,
                    html_cell,
                    source_start,
                    slots));
            }
            if (row.children.empty()) continue;
            columns = (std::max)(columns, row.children.size());
            row.ensure_table_special().table_header_row = header;
            attach_html_element(row, *html_row, source_start);
            push_html_range(row.id, *html_row, source_start);
            table.children.push_back(std::move(row));
        }
        if (table.children.empty()) return std::nullopt;
        table.ensure_table_special().table_aligns.assign(columns, TableAlignment::None);
        attach_html_element(table, node, source_start);
        push_html_range(table.id, node, source_start);
        return table;
    }

    std::vector<BlockNode> make_html_children(
        std::span<const HtmlCstNode> children,
        std::size_t source_start,
        std::vector<HtmlContentSlot>& slots) {
        std::vector<BlockNode> blocks;
        std::optional<SourceRange> inline_run;
        auto flush_inline = [&]() {
            if (!inline_run) return;
            const auto source = std::u32string_view(cps).substr(
                source_start + inline_run->start,
                inline_run->length());
            if (!html_whitespace_only(source)) {
                blocks.push_back(make_html_inline_run(*inline_run, source_start, slots));
            }
            inline_run.reset();
        };

        for (const auto& child : children) {
            const auto is_block = child.kind == HtmlCstKind::Element
                && html_is_block_element(child.tag_name);
            if (!is_block) {
                if (!inline_run) inline_run = child.range;
                else inline_run->end = child.range.end;
                continue;
            }
            flush_inline();
            if (auto block = make_html_semantic(child, source_start, slots)) {
                blocks.push_back(std::move(*block));
            }
        }
        flush_inline();
        return blocks;
    }

    std::optional<BlockNode> make_html_list(
        const HtmlCstNode& node,
        std::size_t source_start,
        std::vector<HtmlContentSlot>& slots) {
        BlockNode list;
        list.id = next_node_id();
        list.kind = BlockKind::List;
        list.ensure_list_special().ordered = node.tag_name == "ol";
        if (auto start = html_attribute_value(node, "start", source_start)) {
            try {
                list.ensure_list_special().start = std::stoull(cps_to_utf8(*start));
            } catch (...) {
                list.ensure_list_special().start = 1;
            }
        }
        for (const auto& child : node.children) {
            if (child.kind != HtmlCstKind::Element || child.tag_name != "li") continue;
            BlockNode item;
            item.id = next_node_id();
            item.kind = BlockKind::ListItem;
            item.children = make_html_children(child.children, source_start, slots);
            if (item.children.empty()) {
                item.children.push_back(make_html_inline_run(
                    {child.content.start, child.content.start}, source_start, slots));
            }
            attach_html_element(item, child, source_start);
            push_html_range(item.id, child, source_start);
            list.children.push_back(std::move(item));
        }
        if (list.children.empty()) return std::nullopt;
        attach_html_element(list, node, source_start);
        push_html_range(list.id, node, source_start);
        return list;
    }

    std::optional<BlockNode> make_html_semantic(
        const HtmlCstNode& node,
        std::size_t source_start,
        std::vector<HtmlContentSlot>& slots) {
        if (node.kind != HtmlCstKind::Element) return std::nullopt;
        if (node.tag_name == "p" || node.tag_name == "summary"
            || node.tag_name == "dt" || node.tag_name == "dd") {
            return make_html_inline_block(BlockKind::Paragraph, node, source_start, slots);
        }
        if (node.tag_name.size() == 2 && node.tag_name[0] == 'h'
            && node.tag_name[1] >= '1' && node.tag_name[1] <= '6') {
            return make_html_inline_block(
                BlockKind::Heading,
                node,
                source_start,
                slots,
                static_cast<std::uint8_t>(node.tag_name[1] - '0'));
        }
        if (node.tag_name == "table") return make_html_table(node, source_start, slots);
        if (node.tag_name == "ul" || node.tag_name == "ol") {
            return make_html_list(node, source_start, slots);
        }
        if (node.tag_name == "blockquote") {
            BlockNode quote;
            quote.id = next_node_id();
            quote.kind = BlockKind::BlockQuote;
            quote.children = make_html_children(node.children, source_start, slots);
            if (quote.children.empty()) {
                quote.children.push_back(make_html_inline_run(
                    {node.content.start, node.content.start}, source_start, slots));
            }
            attach_html_element(quote, node, source_start);
            push_html_range(quote.id, node, source_start);
            return quote;
        }
        if (node.tag_name == "hr") {
            BlockNode rule;
            rule.id = next_node_id();
            rule.kind = BlockKind::ThematicBreak;
            attach_html_element(rule, node, source_start);
            push_html_range(rule.id, node, source_start);
            return rule;
        }
        if (node.tag_name == "pre") {
            BlockNode code;
            code.id = next_node_id();
            code.kind = BlockKind::CodeBlock;
            code.block_source = make_block_source(
                std::u32string(std::u32string_view(cps).substr(
                    source_start + node.range.start,
                    node.range.length())),
                BlockSourceKind::HtmlCode);
            attach_html_element(code, node, source_start);
            slots.push_back({code.id, node.range});
            push_html_range(code.id, node, source_start);
            return code;
        }

        BlockNode container;
        container.id = next_node_id();
        container.kind = BlockKind::HtmlContainer;
        container.children = make_html_children(node.children, source_start, slots);
        if (container.children.empty()) {
            container.children.push_back(make_html_inline_run(
                node.content, source_start, slots));
        }
        attach_html_element(container, node, source_start);
        push_html_range(container.id, node, source_start);
        return container;
    }

    std::optional<BlockNode> try_parse_raw_html_block() {
        const auto start = pos;
        const auto opening = html_block_start(start);
        if (!opening) return std::nullopt;
        const auto block_end = html_block_end(start, opening->kind);
        auto content_end = block_end;
        if (content_end > start && cps[content_end - 1] == U'\n') {
            --content_end;
            if (content_end > start && cps[content_end - 1] == U'\r') --content_end;
        } else if (content_end > start && cps[content_end - 1] == U'\r') {
            --content_end;
        }

        // Type 7 keeps the established one-source editable paragraph. It does
        // not need a second HTML tree to decide its presentation.
        if (opening->kind == MarkdownHtmlBlockKind::OtherTag) {
            BlockNode block;
            block.id = next_node_id();
            block.kind = BlockKind::Paragraph;
            block.inline_content = make_inline_document(start, content_end, InlineSyntaxMode::HtmlText);
            push_range(block.id, {start, block_end}, {start, content_end});
            pos = block_end;
            return block;
        }

        auto raw = std::u32string(std::u32string_view(cps).substr(start, content_end - start));
        auto tree = parse_html_cst(raw);
        const HtmlCstNode* element = nullptr;
        bool single_element = true;
        for (const auto& node : tree.nodes) {
            if (node.kind == HtmlCstKind::Text && std::ranges::all_of(
                    std::u32string_view(raw).substr(node.range.start, node.range.length()),
                    [](char32_t ch) { return ch == U' ' || ch == U'\t'
                        || ch == U'\r' || ch == U'\n'; })) continue;
            if (element || (node.kind != HtmlCstKind::Element && node.kind != HtmlCstKind::Comment)) {
                single_element = false;
                break;
            }
            element = &node;
        }
        const bool complete = !tree.has_error && std::ranges::all_of(tree.nodes,
            [](const HtmlCstNode& node) { return node.status == HtmlParseStatus::Complete; });
        // CommonMark owns the boundary; presentation errors cannot extend it.
        const auto unsafe = std::ranges::any_of(tree.nodes,
            [](const auto& node) { return html_contains_unsafe(node); });
        if (!complete || unsafe) {
            BlockNode block;
            block.id = next_node_id();
            block.kind = BlockKind::UnsupportedMarkup;
            block.ensure_atomic_special().raw = cps_to_utf8(raw);
            block.ensure_atomic_special().unsup_reason = unsafe
                ? UnsupportedMarkupReason::UnsafeHtml : UnsupportedMarkupReason::MalformedSyntax;
            push_range(block.id, {start, content_end}, {start, content_end});
            diagnostics.push_back(make_diagnostic(DiagnosticSeverity::Warning,
                unsafe ? "Unsafe HTML block is shown as text" : "Malformed HTML block is shown as text",
                std::nullopt, DIAG_UNSAFE_HTML));
            pos = block_end;
            return block;
        }
        if (single_element && element && element->kind == HtmlCstKind::Comment) {
            BlockNode block;
            block.id = next_node_id();
            block.kind = BlockKind::HtmlContainer;
            auto& html = block.ensure_html_special();
            html.source = std::move(raw);
            html.tree = std::move(tree);
            html.structure_shape = html_block_structure_shape(block);
            push_range(block.id, {start, content_end}, {start, content_end});
            pos = block_end;
            return block;
        }

        std::vector<HtmlContentSlot> slots;
        std::optional<BlockNode> block;
        std::string root_tag;
        if (single_element && element) {
            root_tag = element->tag_name;
            block = make_html_semantic(*element, start, slots);
        } else {
            // A raw HTML block can contain adjacent complete elements. Keep
            // each element's existing semantic owner/style under the existing
            // container; the container owns only wrapper syntax and slots.
            BlockNode container;
            container.id = next_node_id();
            container.kind = BlockKind::HtmlContainer;
            container.children = make_html_children(tree.nodes, start, slots);
            push_range(container.id, {start, content_end}, {start, content_end});
            block = std::move(container);
        }
        if (!block) return std::nullopt;
        // <pre> is already an exact block-local source owner. All other HTML
        // structures retain wrapper source and slots for editable descendants.
        if (block->kind != BlockKind::CodeBlock) {
            auto& html = block->ensure_html_special();
            html.source = std::move(raw);
            html.tree = std::move(tree);
            html.content_slots = std::move(slots);
            html.structure_shape = html_block_structure_shape(*block);
            html.root_tag = std::move(root_tag);
        }
        pos = block_end;
        return block;
    }

    // ---- footnote definition ----
    std::optional<BlockNode> try_parse_footnote_definition() {
        const auto start = pos;
        const auto save = pos;
        advance_n(2);
        std::u32string label;
        while (!eof() && peek1() != ']' && !is_line_ending_character(peek1())) {
            label.push_back(peek1());
            advance();
        }
        if (peek1() != ']') { pos = save; return std::nullopt; }
        advance();
        if (peek1() != ':') { pos = save; return std::nullopt; }
        advance();
        while (peek1() == ' ') advance();
        const auto content_start = pos;

        struct BodyLine {
            std::size_t start = 0;
            std::size_t end = 0;
            std::size_t content_start = 0;
            bool blank = false;
        };
        auto inspect_line = [&](std::size_t line_start) {
            BodyLine line;
            line.start = line_start;
            line.end = line_content_end(line_start);
            line.blank = true;
            for (auto cursor = line.start; cursor < line.end; ++cursor) {
                if (cps[cursor] != U' ' && cps[cursor] != U'\t') {
                    line.blank = false;
                    break;
                }
            }
            line.content_start = line.start;
            if (!line.blank) {
                if (line.content_start < line.end && cps[line.content_start] == U'\t') {
                    ++line.content_start;
                } else {
                    std::size_t spaces = 0;
                    while (line.content_start < line.end && cps[line.content_start] == U' ' && spaces < 4) {
                        ++line.content_start;
                        ++spaces;
                    }
                    if (spaces < 4) line.content_start = line.start;
                }
            } else {
                line.content_start = line.end;
            }
            return line;
        };
        auto is_continuation = [](const BodyLine& line) {
            return !line.blank && line.content_start > line.start;
        };

        const auto first_line_end = line_content_end(content_start);
        std::vector<BodyLine> continuation_lines;
        auto source_end = next_line_start(first_line_end);
        auto cursor = source_end;
        while (cursor < cps.size()) {
            auto line = inspect_line(cursor);
            if (is_continuation(line)) {
                continuation_lines.push_back(line);
                source_end = next_line_start(line.end);
                cursor = source_end;
                continue;
            }
            if (!line.blank) break;

            std::vector<BodyLine> pending_blank_lines;
            auto lookahead = cursor;
            while (lookahead < cps.size()) {
                auto blank = inspect_line(lookahead);
                if (!blank.blank) break;
                pending_blank_lines.push_back(blank);
                lookahead = next_line_start(blank.end);
                if (lookahead == blank.end) break;
            }
            if (lookahead >= cps.size()) break;
            auto next = inspect_line(lookahead);
            if (!is_continuation(next)) break;
            continuation_lines.insert(
                continuation_lines.end(), pending_blank_lines.begin(), pending_blank_lines.end());
            continuation_lines.push_back(next);
            source_end = next_line_start(next.end);
            cursor = source_end;
        }

        std::u32string inner;
        std::vector<std::size_t> offset_map;
        std::vector<PhysicalRange> marker_ranges;
        marker_ranges.push_back(PhysicalRange(start, content_start));
        std::size_t inner_end_source = content_start;
        auto append_range = [&](std::size_t begin, std::size_t end) {
            for (auto source_offset = begin; source_offset < end; ++source_offset) {
                offset_map.push_back(source_offset);
                inner.push_back(cps[source_offset]);
            }
            inner_end_source = end;
        };
        append_range(content_start, first_line_end);
        for (const auto& line : continuation_lines) {
            offset_map.push_back(line.start > 0 ? line.start - 1 : line.start);
            inner.push_back(U'\n');
            if (!line.blank) {
                marker_ranges.push_back(PhysicalRange(line.start, line.content_start));
                append_range(line.content_start, line.end);
            } else {
                inner_end_source = line.end;
            }
        }
        offset_map.push_back(inner_end_source);

        ParseInput nested_input(input->revision, cps_to_utf8(inner), input->dialect);
        Parser nested(&nested_input);
        nested.node_counter = node_counter;
        auto children = nested.parse_blocks(nullptr);
        node_counter = nested.node_counter;
        auto remap = [&](std::size_t value) {
            return offset_map[(std::min)(value, offset_map.size() - 1)];
        };
        for (auto& range : nested.source_ranges) {
            range.source_range = PhysicalRange(remap(range.source_range.start), remap(range.source_range.end));
            range.content_range = PhysicalRange(remap(range.content_range.start), remap(range.content_range.end));
            for (auto& offset : range.owned_offsets) offset = remap(offset);
            if (range.separator_range) range.separator_range = PhysicalRange{
                remap(range.separator_range->start), remap(range.separator_range->end)};
            for (auto& prefix : range.marker_ranges) {
                prefix = PhysicalRange(remap(prefix.start), remap(prefix.end));
            }
            source_ranges.push_back(std::move(range));
        }
        if (children.empty()) {
            BlockNode paragraph;
            paragraph.id = next_node_id();
            paragraph.kind = BlockKind::Paragraph;
            source_ranges.emplace_back(
                paragraph.id,
                PhysicalRange(content_start, content_start),
                PhysicalRange(content_start, content_start));
            children.push_back(std::move(paragraph));
        }
        for (auto& diagnostic : nested.diagnostics) diagnostics.push_back(std::move(diagnostic));
        pos = source_end;
        const auto id = next_node_id();
        ParserSourceRange definition_range(
            id,
            PhysicalRange(start, source_end),
            PhysicalRange(content_start, inner_end_source));
        definition_range.marker_ranges = std::move(marker_ranges);
        source_ranges.push_back(std::move(definition_range));
        BlockNode block;
        block.id = id;
        block.kind = BlockKind::FootnoteDefinition;
        block.ensure_container_special().footnote_label = cps_to_utf8(label);
        block.ensure_text_special().opening_marker = std::u32string(std::u32string_view(cps).substr(start, content_start - start));
        block.children = std::move(children);
        return block;
    }

    // ---- code fence / fenced math ----
    std::optional<BlockNode> try_parse_indented_code_block() {
        auto indentation_end = [&](std::size_t line_start) -> std::optional<std::size_t> {
            const auto indent = scan_indentation(line_start, 4);
            if (indent.columns >= 4) return indent.end;
            return std::nullopt;
        };
        auto start = pos;
        if (!indentation_end(start)) return std::nullopt;
        std::u32string text;
        std::vector<PhysicalRange> markers;
        std::vector<BlockLineIndentation> code_contexts;
        std::size_t content_start = start;
        std::size_t content_end = start;
        bool first_line = true;
        while (pos < cps.size()) {
            auto line_start = pos;
            const auto line_end = line_content_end(line_start);
            const auto context = line_context(line_start);
            auto indent_end = indentation_end(line_start);
            if (!indent_end) {
                bool blank = true;
                for (auto cursor = line_start; cursor < line_end; ++cursor) {
                    if (cps[cursor] != U' ' && cps[cursor] != U'\t') { blank = false; break; }
                }
                if (!blank) break;
                auto next = line_end + line_ending_length(line_end);
                while (next < cps.size()) {
                    const auto next_end = line_content_end(next);
                    bool next_blank = true;
                    for (auto cursor = next; cursor < next_end; ++cursor) {
                        if (cps[cursor] != U' ' && cps[cursor] != U'\t') { next_blank = false; break; }
                    }
                    if (!next_blank) break;
                    next = next_end + line_ending_length(next_end);
                }
                if (next >= cps.size() || !indentation_end(next)) break;
                code_contexts.push_back({line_start - start, context.physical_column, context.residual_columns});
                markers.push_back(PhysicalRange(std::size_t(line_start), std::size_t(line_end)));
                text.push_back(U'\n');
                pos = line_end;
                consume_line_ending();
                content_end = line_end;
                continue;
            }
            code_contexts.push_back({line_start - start, context.physical_column, context.residual_columns});
            markers.push_back(PhysicalRange(std::size_t(line_start), std::size_t(*indent_end)));
            if (first_line) content_start = *indent_end;
            first_line = false;
            text.append(cps.begin() + *indent_end, cps.begin() + line_end);
            content_end = line_end;
            if (line_end < cps.size()) text.push_back(U'\n');
            pos = line_end;
            consume_line_ending();
        }
        NodeId id = next_node_id();
        ParserSourceRange range(id, PhysicalRange(std::size_t(start), std::size_t(pos)), PhysicalRange(std::size_t(content_start), std::size_t(content_end)));
        range.marker_ranges = std::move(markers);
        source_ranges.push_back(std::move(range));
        BlockNode block;
        block.id = id;
        block.kind = BlockKind::CodeBlock;
        block.ensure_atomic_special().code_indented = true;
        block.block_source = make_block_source(
            std::u32string(std::u32string_view(cps).substr(start, pos - start)),
            BlockSourceKind::IndentedCode,
            std::move(code_contexts));
        trim_indented_code_blank_edges(block.block_source);
        return block;
    }

    static void trim_indented_code_blank_edges(BlockSourceDocument& document) {
        if (!document.storage) return;
        auto& tree = document.tree();
        if (tree.content_to_source.size() != tree.content.size() + 1) return;
        struct Line { std::size_t start; std::size_t end; std::size_t break_end; };
        std::vector<Line> lines;
        std::size_t start = 0;
        while (start < tree.content.size()) {
            auto end = start;
            while (end < tree.content.size() && tree.content[end] != U'\n') ++end;
            auto break_end = end < tree.content.size() ? end + 1 : end;
            lines.push_back({start, end, break_end});
            start = break_end;
        }
        if (lines.empty()) return;
        auto has_code = [&](const Line& line) {
            for (auto index = line.start; index < line.end; ++index) {
                if (tree.content[index] != U' ' && tree.content[index] != U'\t') return true;
            }
            return false;
        };
        auto first = std::size_t{0};
        while (first < lines.size() && !has_code(lines[first])) ++first;
        if (first == lines.size()) {
            const auto source_end = tree.content_to_source.back();
            tree.content.clear();
            tree.content_to_source.assign(1, source_end);
            tree.content_ranges.clear();
            tree.generated_indentation.clear();
            return;
        }
        auto last = lines.size() - 1;
        while (last > first && !has_code(lines[last])) --last;
        const auto begin = lines[first].start;
        const auto end = lines[last].break_end;
        std::u32string content = tree.content.substr(begin, end - begin);
        std::vector<std::size_t> mapping(
            tree.content_to_source.begin() + begin,
            tree.content_to_source.begin() + end);
        mapping.push_back(tree.content_to_source[end]);
        std::vector<SourceRange> ranges;
        for (std::size_t index = 0; index < content.size(); ++index) {
            const auto source_offset = mapping[index];
            if (!ranges.empty() && ranges.back().end == source_offset) ranges.back().end = source_offset + 1;
            else ranges.push_back({source_offset, source_offset + 1});
        }
        std::vector<GeneratedCodeIndentation> generated;
        for (auto indentation : tree.generated_indentation) {
            if (indentation.content_offset < begin || indentation.content_offset > end) continue;
            indentation.content_offset -= begin;
            generated.push_back(indentation);
        }
        tree.content = std::move(content);
        tree.content_to_source = std::move(mapping);
        tree.content_ranges = std::move(ranges);
        tree.generated_indentation = std::move(generated);
    }

    std::optional<BlockNode> try_parse_code_block() {
        std::size_t start = pos;
        std::size_t opening_indent = 0;
        while (opening_indent < 4 && peek1() == U' ') {
            ++opening_indent;
            advance();
        }
        if (opening_indent > 3) { pos = start; return std::nullopt; }

        const auto marker = peek1();
        if (marker != U'`' && marker != U'~') { pos = start; return std::nullopt; }
        std::size_t count = 0;
        while (peek1() == marker) { ++count; advance(); }
        if (count < 3) { pos = start; return std::nullopt; }
        auto [info_line, info_length] = rest_of_line();
        if (marker == U'`' && info_line.find(U'`') != std::u32string::npos) {
            pos = start;
            return std::nullopt;
        }
        advance_n(info_length);
        consume_line_ending();
        std::size_t content_start = pos;
        std::size_t content_end = pos;
        auto info_utf8 = cps_to_utf8(info_line);
        std::optional<std::string> lang;
        auto trimmed = trim_utf8(info_utf8);
        if (!trimmed.empty()) lang = trimmed;
        std::u32string text;
        std::optional<PhysicalRange> closing_marker;
        while (!eof()) {
            std::size_t line_start = pos;
            std::size_t scan = pos;
            std::size_t closing_indent = 0;
            while (scan < cps.size() && cps[scan] == U' ' && closing_indent < 4) {
                ++scan;
                ++closing_indent;
            }
            const auto fence_start = scan;
            while (scan < cps.size() && cps[scan] == marker) ++scan;
            std::size_t fence_count = scan - fence_start;
            std::size_t marker_end = scan;
            while (marker_end < cps.size() && (cps[marker_end] == U' ' || cps[marker_end] == U'\t')) ++marker_end;
            if (closing_indent <= 3
                && fence_count >= count
                && (marker_end == cps.size() || is_line_ending_character(cps[marker_end]))) {
                content_end = line_start;
                closing_marker = PhysicalRange(std::size_t(line_start), std::size_t(marker_end));
                pos = marker_end;
                consume_line_ending();
                break;
            }
            while (!eof() && !is_line_ending_character(peek1())) {
                text.push_back(peek1());
                advance();
            }
            if (is_line_ending_character(peek1())) {
                text.push_back(U'\n');
                consume_line_ending();
            }
            content_end = pos;
        }
        if (content_end < content_start) content_end = pos;
        NodeId id = next_node_id();
        ParserSourceRange node_range(id, PhysicalRange(std::size_t(start), cur()), PhysicalRange(std::size_t(content_start), std::size_t(content_end)));
        node_range.marker_ranges.push_back(PhysicalRange(std::size_t(start), std::size_t(content_start)));
        if (closing_marker) node_range.marker_ranges.push_back(*closing_marker);
        source_ranges.push_back(std::move(node_range));
        if (lang && *lang == "math" && input->dialect.math.fenced_math) {
            const auto local_end = closing_marker ? closing_marker->end : pos;
            BlockNode b; b.id = id; b.kind = BlockKind::MathBlock; b.ensure_atomic_special().math_delim = MathDelimiter::FencedMath;
            b.block_source = make_block_source(
                std::u32string(std::u32string_view(cps).substr(start, local_end - start)),
                BlockSourceKind::FencedMath);
            return b;
        }
        const auto local_end = closing_marker ? closing_marker->end : pos;
        BlockNode b; b.id = id; b.kind = BlockKind::CodeBlock;
        b.block_source = make_block_source(
            std::u32string(std::u32string_view(cps).substr(start, local_end - start)),
            BlockSourceKind::FencedCode);
        project_fenced_code_indentation(b.block_source, opening_indent);
        return b;
    }

    static void project_fenced_code_indentation(
        BlockSourceDocument& document,
        std::size_t opening_indent) {
        if (opening_indent == 0 || !document.storage) return;
        auto& tree = document.tree();
        if (tree.content_ranges.empty() || tree.content_to_source.size() != tree.content.size() + 1) return;

        std::u32string content;
        std::vector<std::size_t> content_to_source;
        std::vector<SourceRange> content_ranges;
        content.reserve(tree.content.size());
        content_to_source.reserve(tree.content_to_source.size());
        bool line_start = true;
        std::size_t line_indent = 0;
        for (std::size_t index = 0; index < tree.content.size(); ++index) {
            const auto ch = tree.content[index];
            if (line_start && ch == U' ' && line_indent < opening_indent) {
                ++line_indent;
                continue;
            }
            if (line_start) {
                line_start = false;
                line_indent = 0;
            }
            const auto source_offset = tree.content_to_source[index];
            content.push_back(ch);
            content_to_source.push_back(source_offset);
            if (!content_ranges.empty() && content_ranges.back().end == source_offset) {
                content_ranges.back().end = source_offset + 1;
            } else {
                content_ranges.push_back({source_offset, source_offset + 1});
            }
            if (is_line_ending_character(ch)) {
                line_start = true;
                line_indent = 0;
            }
        }
        content_to_source.push_back(tree.content_to_source.back());
        tree.content = std::move(content);
        tree.content_to_source = std::move(content_to_source);
        tree.content_ranges = std::move(content_ranges);
    }

    // ---- math blocks `$$ ... $$` and `\\[ ... \\]` ----
    std::optional<BlockNode> try_parse_math_block() {
        std::size_t start = pos;
        bool dollar = peek1() == '$' && peek2() == '$' && input->dialect.math.block_dollar;
        bool bracket = peek1() == '\\' && peek2() == '[' && input->dialect.math.block_bracket;
        if (!dollar && !bracket) return std::nullopt;
        advance_n(2);
        consume_line_ending();
        const auto content_start = pos;
        std::u32string tex;
        while (!eof()) {
            bool closed = dollar ? (peek1() == '$' && peek2() == '$') : (peek1() == '\\' && peek2() == ']');
            if (closed) {
                const auto closing_start = pos;
                advance_n(2);
                const auto closing_end = pos;
                consume_line_ending();
                NodeId id = next_node_id();
                ParserSourceRange range(
                    id,
                    PhysicalRange(std::size_t(start), cur()),
                    PhysicalRange(std::size_t(content_start), std::size_t(closing_start)));
                range.marker_ranges.push_back(PhysicalRange(std::size_t(start), std::size_t(content_start)));
                range.marker_ranges.push_back(PhysicalRange(std::size_t(closing_start), std::size_t(closing_end)));
                source_ranges.push_back(std::move(range));
                BlockNode b; b.id = id; b.kind = BlockKind::MathBlock; b.ensure_atomic_special().math_delim = dollar ? MathDelimiter::BlockDollar : MathDelimiter::BlockBracket;
                b.block_source = make_block_source(
                    std::u32string(std::u32string_view(cps).substr(start, closing_end - start)),
                    dollar ? BlockSourceKind::DollarMath : BlockSourceKind::BracketMath);
                return b;
            }
            tex.push_back(peek1()); advance();
        }
        diagnostics.push_back(make_diagnostic(DiagnosticSeverity::Warning,
            dollar ? "Unclosed math block delimiter $$" : "Unclosed math block delimiter \\[",
           std::nullopt, DIAG_UNCLOSED_MATH_DOLLAR));
        NodeId id = next_node_id();
        ParserSourceRange range(
            id,
            PhysicalRange(std::size_t(start), cur()),
            PhysicalRange(std::size_t(content_start), cur()));
        range.marker_ranges.push_back(PhysicalRange(std::size_t(start), std::size_t(content_start)));
        source_ranges.push_back(std::move(range));
        BlockNode b; b.id = id; b.kind = BlockKind::MathBlock; b.ensure_atomic_special().math_delim = dollar ? MathDelimiter::BlockDollar : MathDelimiter::BlockBracket;
        b.block_source = make_block_source(
            std::u32string(std::u32string_view(cps).substr(start, pos - start)),
            dollar ? BlockSourceKind::DollarMath : BlockSourceKind::BracketMath);
        return b;
    }

    // A delimiter row closes the preceding paragraph before the physical
    // header line. Recognition is read-only; node/source ownership remains in
    // try_parse_table, including when a table follows text without a blank line.
    bool line_starts_gfm_table(std::size_t at) const {
        if (!input->dialect.gfm.tables || !input->dialect.tables.gfm_pipe_tables) return false;
        if (scan_indentation(at, 3).columns > 3) return false;
        const auto end = line_content_end(at);
        const auto header = cps.substr(at, end - at);
        const auto next = next_line_start(end);
        if (next >= cps.size() || scan_indentation(next, 3).columns > 3) return false;
        // cmark resolves a legal setext underline before trying extension
        // block openers. Preserve that precedence for pipe-less headers.
        if (header.find(U'|') == std::u32string_view::npos
            && setext_underline_at(next)) return false;
        const auto separator = cps.substr(next, line_content_end(next) - next);
        const auto alignments = parse_table_separator(separator);
        return alignments && alignments->size() == table_cell_segments(header).size();
    }

    // ---- table ----
    std::optional<BlockNode> try_parse_table() {
        // Recognize both rows before constructing editable cells. A rejected
        // table must not parse an ordinary paragraph's inline source twice.
        if (!line_starts_gfm_table(pos)) return std::nullopt;
        std::size_t save = pos;
        std::size_t range_count = source_ranges.size();
        std::uint64_t node_count = node_counter;
        auto fail = [&]() -> std::optional<BlockNode> {
            pos = save;
            source_ranges.resize(range_count);
            node_counter = node_count;
            return std::nullopt;
        };
        auto [header_line, _] = rest_of_line();
        const auto header_end = save + header_line.size();
        std::vector<std::u32string> physical_line_endings;
        physical_line_endings.emplace_back(std::u32string_view(cps).substr(
            header_end,
            next_line_start(header_end) - header_end));
        auto header_row = parse_table_row();
        if (!header_row) return fail();
        const auto separator_start = pos;
        auto [separator_line, separator_length] = rest_of_line();
        auto alignments = parse_table_separator(separator_line);
        if (!alignments || alignments->size() != header_row->children.size()) return fail();
        advance_n(separator_length);
        const auto separator_end = separator_start + separator_length;
        physical_line_endings.emplace_back(std::u32string_view(cps).substr(
            separator_end,
            next_line_start(separator_end) - separator_end));
        consume_line_ending();
        BlockVec rows;
        while (!eof()) {
            std::size_t row_start = pos;
            auto [line, row_length] = rest_of_line();
            if (is_blank_line_at(pos) || line_starts_interrupting_block(pos, false)) break;
            const auto row_end = row_start + row_length;
            auto row = parse_table_row(header_row->children.size());
            if (!row) break;
            physical_line_endings.emplace_back(std::u32string_view(cps).substr(
                row_end,
                next_line_start(row_end) - row_end));
            while (row->children.size() < header_row->children.size()) {
                BlockNode cell;
                cell.id = next_node_id();
                cell.kind = BlockKind::TableCell;
                cell.inline_content = make_inline_document(
                    row_start + row_length, row_start + row_length, InlineSyntaxMode::TableCell);
                auto offset = std::size_t(row_start + row_length);
                push_range(cell.id, PhysicalRange(offset, offset), PhysicalRange(offset, offset));
                row->ensure_table_special().row_syntax->implicit_cell_owners.push_back(cell.id);
                row->children.push_back(std::move(cell));
            }
            rows.push_back(std::move(*row));
        }
        NodeId id = next_node_id();
        push_range(id, PhysicalRange(std::size_t(save), cur()), PhysicalRange(std::size_t(save), cur()));
        BlockNode b; b.id = id; b.kind = BlockKind::Table;
        header_row->ensure_table_special().table_header_row = true;
        b.children.push_back(std::move(*header_row));
        b.children.insert(b.children.end(), std::make_move_iterator(rows.begin()), std::make_move_iterator(rows.end()));
        auto& table = b.ensure_table_special();
        table.table_aligns = std::move(*alignments);
        table.table_separator_source = std::move(separator_line);
        physical_line_endings.resize(b.children.size());
        table.table_internal_line_endings = std::move(physical_line_endings);
        return b;
    }

    std::vector<std::pair<std::size_t, std::size_t>> table_cell_segments(const std::u32string& line) const {
        std::vector<std::pair<std::size_t, std::size_t>> segments;
        auto first = line.find_first_not_of(U" \t");
        std::size_t start = first != std::u32string::npos && line[first] == U'|' ? first + 1 : 0;
        for (std::size_t index = start; index < line.size(); ++index) {
            if (line[index] == U'\\' && index + 1 < line.size()) {
                ++index;
                continue;
            }
            if (line[index] != U'|') continue;
            segments.push_back({start, index});
            start = index + 1;
        }
        if (start < line.size() && (segments.empty() || line.find_first_not_of(U" \t", start) != std::u32string::npos))
            segments.push_back({start, line.size()});
        return segments;
    }

    std::optional<BlockNode> parse_table_row(std::size_t maximum_cells = (std::numeric_limits<std::size_t>::max)()) {
        std::size_t row_start = pos;
        auto [line, length] = rest_of_line();
        auto segments = table_cell_segments(line);
        if (segments.empty()) return std::nullopt;
        BlockVec cells;
        cells.reserve(segments.size());
        BlockTableSpecial::RowSyntax row_syntax;
        std::vector<std::pair<std::size_t, std::size_t>> body_ranges;
        for (auto const& segment : segments) {
            if (cells.size() >= maximum_cells) break;
            std::size_t content_start = segment.first;
            std::size_t content_end = segment.second;
            while (content_start < content_end && (line[content_start] == U' ' || line[content_start] == U'\t')) ++content_start;
            while (content_end > content_start && (line[content_end - 1] == U' ' || line[content_end - 1] == U'\t')) --content_end;
            if (content_start == segment.second) {
                content_start = segment.first;
                if (content_start < segment.second && (line[content_start] == U' ' || line[content_start] == U'\t')) ++content_start;
                content_end = content_start;
            }
            NodeId cell_id = next_node_id();
            auto source_start = std::size_t(row_start + segment.first);
            auto source_end = std::size_t(row_start + segment.second);
            auto text_start = std::size_t(row_start + content_start);
            auto text_end = std::size_t(row_start + content_end);
            BlockNode cell;
            cell.id = cell_id;
            cell.kind = BlockKind::TableCell;
            cell.inline_content = make_inline_document(text_start, text_end, InlineSyntaxMode::TableCell);
            push_range(cell_id, PhysicalRange(source_start, source_end), PhysicalRange(text_start, text_end));
            row_syntax.cell_owners.push_back(cell_id);
            body_ranges.emplace_back(content_start, content_end);
            cells.push_back(std::move(cell));
        }
        row_syntax.gaps.reserve(body_ranges.size() + 1);
        std::size_t gap_start = 0;
        for (const auto& [body_start, body_end] : body_ranges) {
            row_syntax.gaps.push_back(line.substr(gap_start, body_start - gap_start));
            gap_start = body_end;
        }
        row_syntax.gaps.push_back(line.substr(gap_start));
        advance_n(length);
        consume_line_ending();
        NodeId row_id = next_node_id();
        push_range(row_id, PhysicalRange(std::size_t(row_start), cur()), PhysicalRange(std::size_t(row_start), std::size_t(row_start + length)));
        BlockNode r; r.id = row_id; r.kind = BlockKind::TableRow; r.children = std::move(cells);
        r.ensure_table_special().row_syntax = std::move(row_syntax);
        return r;
    }

    static bool is_table_marker_space_(char32_t ch) {
        return ch == U' ' || ch == U'\t' || ch == U'\v' || ch == U'\f' || ch == U'\r';
    }

    std::optional<std::vector<TableAlignment>> parse_table_separator(const std::u32string& line) const {
        auto segments = table_cell_segments(line);
        if (segments.empty()) return std::nullopt;
        std::vector<TableAlignment> alignments;
        for (auto const& segment : segments) {
            auto begin = segment.first;
            auto end = segment.second;
            while (begin < end && is_table_marker_space_(line[begin])) ++begin;
            while (end > begin && is_table_marker_space_(line[end - 1])) --end;
            if (begin == end) return std::nullopt;
            bool leading_colon = line[begin] == U':';
            bool trailing_colon = line[end - 1] == U':';
            if (leading_colon) ++begin;
            if (trailing_colon) --end;
            if (begin >= end) return std::nullopt;
            for (std::size_t index = begin; index < end; ++index) if (line[index] != U'-') return std::nullopt;
            TableAlignment alignment = TableAlignment::None;
            if (leading_colon && trailing_colon) alignment = TableAlignment::Center;
            else if (leading_colon) alignment = TableAlignment::Left;
            else if (trailing_colon) alignment = TableAlignment::Right;
            alignments.push_back(alignment);
        }
        return alignments;
    }

    // ---- list / task ----
    std::optional<BlockNode> try_parse_list_or_task() {
        struct Marker {
            bool ordered = false;
            bool task = false;
            bool checked = false;
            std::uint64_t number = 1;
            char32_t delimiter = U'.';
            char32_t bullet = U'-';
            std::size_t start = 0;
            std::size_t indent = 0;
            std::size_t content_indent = 0;
            std::size_t marker_end = 0;
            std::size_t content_start = 0;
            std::size_t content_end = 0;
            std::size_t source_end = 0;
            std::u32string initial_prefix;
            std::vector<std::size_t> initial_prefix_map;
            std::size_t initial_physical_column = 0;
            std::size_t initial_residual_columns = 0;
            std::size_t content_physical_column = 0;
            std::u32string text;
        };
        auto inspect = [&](std::size_t at) -> std::optional<Marker> {
            if (at >= cps.size() || !is_line_start_at(at)) return std::nullopt;
            Marker marker;
            marker.start = at;
            const auto leading = scan_indentation(at, 3);
            marker.indent = leading.columns;
            if (marker.indent > 3) return std::nullopt;
            std::size_t cursor = leading.end;
            const auto line_end = line_content_end(cursor);
            if (cursor >= line_end) return std::nullopt;
            if (cps[cursor] == U'-' || cps[cursor] == U'*' || cps[cursor] == U'+') {
                marker.bullet = cps[cursor];
                ++cursor;
            } else if (is_ascii_digit_(cps[cursor])) {
                marker.ordered = true;
                marker.number = 0;
                std::size_t digits = 0;
                while (cursor < cps.size() && is_ascii_digit_(cps[cursor])) {
                    ++digits;
                    marker.number = marker.number * 10 + static_cast<std::uint64_t>(cps[cursor] - U'0');
                    ++cursor;
                }
                if (digits > 9 || cursor >= line_end
                    || (cps[cursor] != U'.' && cps[cursor] != U')')) return std::nullopt;
                marker.delimiter = cps[cursor++];
            } else {
                return std::nullopt;
            }

            marker.marker_end = cursor;
            const auto marker_width = cursor - leading.end;
            auto whitespace_end = cursor;
            auto column = leading.physical_column + marker_width;
            const auto marker_physical_column = column;
            while (whitespace_end < line_end
                && (cps[whitespace_end] == U' ' || cps[whitespace_end] == U'\t')) {
                if (cps[whitespace_end] == U' ')
                    ++column;
                else
                    column += 4 - column % 4;
                ++whitespace_end;
            }
            const bool has_content = whitespace_end < line_end;
            const auto padding_columns = column - marker_physical_column;
            // A bullet or ordered-list marker can be followed by whitespace
            // or end-of-line. Without that separator, characters such as
            // `+++` and `---a---` are paragraph text, not nested list items.
            if (has_content && padding_columns == 0) return std::nullopt;
            const bool overpadded = has_content && padding_columns > 4;
            const auto effective_padding = has_content && !overpadded
                ? padding_columns
                : std::size_t{1};
            marker.content_indent = marker.indent + marker_width + effective_padding;
            marker.content_start = whitespace_end;
            marker.content_physical_column = column;

            if (overpadded) {
                // Keep actual whitespace in the child source. A tab spanning
                // the list-prefix boundary must not become invented spaces
                // in its lossless source; its remaining visual columns belong
                // in the code indentation projection.
                for (auto index = marker.marker_end + 1; index < whitespace_end; ++index) {
                    marker.initial_prefix.push_back(cps[index]);
                    marker.initial_prefix_map.push_back(index);
                }
                auto prefix_column = marker_physical_column;
                for (auto source = marker.marker_end; source < marker.initial_prefix_map.front(); ++source) {
                    if (cps[source] == U' ') ++prefix_column;
                    else if (cps[source] == U'\t') prefix_column += 4 - prefix_column % 4;
                    else ++prefix_column;
                }
                marker.initial_physical_column = prefix_column;
                const auto child_column = line_context(at).physical_column + marker.content_indent;
                marker.initial_residual_columns = prefix_column > child_column
                    ? prefix_column - child_column : 0;
            }

            if (!has_content) marker.content_start = line_end;
            if (input->dialect.gfm.task_lists
                && marker.content_start + 2 < line_end
                && cps[marker.content_start] == U'['
                && (cps[marker.content_start + 1] == U' '
                    || cps[marker.content_start + 1] == U'\t'
                    || cps[marker.content_start + 1] == U'x'
                    || cps[marker.content_start + 1] == U'X')
                && cps[marker.content_start + 2] == U']'
                && (marker.content_start + 3 == line_end
                    || cps[marker.content_start + 3] == U' '
                    || cps[marker.content_start + 3] == U'\t')) {
                marker.task = true;
                marker.checked = cps[marker.content_start + 1] == U'x'
                    || cps[marker.content_start + 1] == U'X';
                marker.content_start += 3;
                if (marker.content_start < line_end
                    && (cps[marker.content_start] == U' ' || cps[marker.content_start] == U'\t')) {
                    ++marker.content_start;
                }
                // The checkbox spelling belongs to the item marker, but does
                // not change its structural content column. Continuation
                // indentation is the same as for an ordinary list item.
            }
            marker.content_end = line_end;
            marker.source_end = next_line_start(line_end);
            const auto marker_text_end = overpadded
                ? marker.marker_end + 1
                : (has_content ? whitespace_end : line_end);
            marker.text = std::u32string(
                std::u32string_view(cps).substr(marker.start, marker_text_end - marker.start));
            if (marker.task) {
                marker.text = std::u32string(
                    std::u32string_view(cps).substr(marker.start, marker.content_start - marker.start));
            }
            return marker;
        };

        auto first = inspect(pos);
        if (!first) return std::nullopt;
        auto same_list_kind = [](const Marker& left, const Marker& right) {
            return left.ordered == right.ordered
                && (left.ordered ? left.delimiter == right.delimiter : left.bullet == right.bullet);
        };
        auto start = pos;
        auto first_content = first->content_start;
        auto last_content = first->content_end;
        auto sibling_indent_limit = first->content_indent;
        NodeId list_id = next_node_id();
        BlockNode result;
        result.id = list_id;
        result.kind = first->task ? BlockKind::TaskList : BlockKind::List;
        result.ensure_list_special().ordered = first->ordered;
        result.ensure_list_special().start = first->number;
        result.ensure_list_special().delimiter = first->delimiter;
        result.ensure_list_special().tight = true;

        auto line_ending_count = [](std::u32string_view source) {
            std::size_t count = 0;
            for (std::size_t index = 0; index < source.size(); ++index) {
                const auto ch = source[index];
                if (ch == U'\r' || ch == U'\n') {
                    ++count;
                    if (ch == U'\r' && index + 1 < source.size()
                        && source[index + 1] == U'\n') ++index;
                }
            }
            return count;
        };
        auto is_blank_separator = [&](std::u32string_view source, bool previous_owns_ending) {
            for (const auto ch : source) {
                if (ch != U' ' && ch != U'\t' && ch != U'\r' && ch != U'\n') return false;
            }
            const auto count = line_ending_count(source);
            return count >= (previous_owns_ending ? 1u : 2u);
        };

        std::optional<std::u32string> separator_before_next_item;
        bool separator_before_next_item_has_blank_line = false;
        while (auto marker = inspect(pos)) {
            // A thematic break can look like a same-marker list item (for
            // example, `* * *`). The item scanner leaves it at `pos`; keep
            // the outer sibling loop from claiming it as another list item.
            if (line_is_thematic_break(pos)) break;
            if (!same_list_kind(*first, *marker) || marker->indent >= sibling_indent_limit) break;
            auto separator_before_this_item = std::move(separator_before_next_item);
            separator_before_next_item.reset();
            const bool has_blank_sibling_gap = separator_before_next_item_has_blank_line;
            separator_before_next_item_has_blank_line = false;
            // Set from the raw physical line scan below. This avoids inferring
            // looseness from a serializer-normalized separator whose EOL may
            // already belong to a code/quote block.
            if (has_blank_sibling_gap) {
                result.ensure_list_special().tight = false;
            }
            NodeId item_id = next_node_id();
            const auto content_indent = marker->content_indent;
            const bool first_line_has_content = !marker->initial_prefix.empty()
                || marker->content_start < marker->content_end;
            bool lazy_paragraph_open = marker->initial_prefix.empty()
                && first_line_has_content
                && line_has_leaf_paragraph_content(
                    marker->content_start, marker->content_end);
            auto item_end = marker->source_end;
            auto item_inner_end = item_end;
            auto last_nonblank_line_end = marker->content_end;
            bool previous_blank = false;
            std::optional<std::size_t> trailing_blank_start;
            while (item_end < cps.size()) {
                auto line_start = item_end;
                const auto line_end = line_content_end(line_start);
                bool blank = true;
                const auto indent = scan_indentation(line_start);
                const auto indentation = indent.columns;
                for (auto cursor = line_start; cursor < line_end; ++cursor) {
                    if (cps[cursor] != U' ' && cps[cursor] != U'\t') blank = false;
                }
                if (!blank && indentation < content_indent
                    && line_is_thematic_break(line_start)) {
                    // An outdented thematic break terminates the list before
                    // sibling-marker inspection; `* * *` is also shaped like
                    // a bullet item but must remain a horizontal rule here.
                    break;
                }
                auto next_marker = inspect(line_start);
                if (next_marker && next_marker->indent < content_indent) {
                    const bool same_list_sibling = same_list_kind(*first, *next_marker);
                    if (previous_blank && trailing_blank_start) {
                        item_inner_end = *trailing_blank_start;
                        if (same_list_sibling) {
                            separator_before_next_item = std::u32string(
                                std::u32string_view(cps).substr(
                                    last_nonblank_line_end,
                                    line_start - last_nonblank_line_end));
                            separator_before_next_item_has_blank_line = true;
                            item_end = line_start;
                        } else {
                            separator_before_next_item.reset();
                            separator_before_next_item_has_blank_line = false;
                            item_end = *trailing_blank_start;
                        }
                    } else {
                        if (same_list_sibling) {
                            separator_before_next_item = cps.substr(
                                last_nonblank_line_end, line_start - last_nonblank_line_end);
                        } else separator_before_next_item.reset();
                        separator_before_next_item_has_blank_line = false;
                        item_inner_end = item_end = line_start;
                    }
                    break;
                }
                if (blank && !first_line_has_content) {
                    auto lookahead = next_line_start(line_end);
                    while (lookahead < cps.size()) {
                        const auto lookahead_end = line_content_end(lookahead);
                        bool lookahead_blank = true;
                        for (auto cursor = lookahead; cursor < lookahead_end; ++cursor) {
                            if (cps[cursor] != U' ' && cps[cursor] != U'\t') {
                                lookahead_blank = false;
                                break;
                            }
                        }
                        if (!lookahead_blank) break;
                        lookahead = next_line_start(lookahead_end);
                    }
                    const auto following_marker = inspect(lookahead);
                    if (!following_marker
                        || !same_list_kind(*first, *following_marker)
                        || following_marker->indent >= content_indent) break;
                }
                if (!blank && indentation < content_indent && previous_blank) {
                    // A blank run before an outdented sibling is the separator
                    // between blocks, not trailing source owned by this list
                    // item. Leave it for parse_blocks so separator_before can
                    // preserve the exact number of blank lines.
                    item_end = trailing_blank_start.value_or(item_end);
                    item_inner_end = item_end;
                    separator_before_next_item.reset();
                    break;
                }
                if (!blank && indentation < content_indent && !lazy_paragraph_open) break;
                if (!blank && indentation < content_indent
                    && line_starts_interrupting_block(line_start)) {
                    // Lazy paragraph continuations may remain unindented, but
                    // an explicit peer-level block marker ends the item even
                    // without a separating blank line. Otherwise the nested
                    // parse would incorrectly absorb the following heading,
                    // quote, fence, list, math or HTML block into this item.
                    break;
                }
                previous_blank = blank;
                if (blank) {
                    if (!trailing_blank_start) trailing_blank_start = line_start;
                    lazy_paragraph_open = false;
                } else {
                    trailing_blank_start.reset();
                    last_nonblank_line_end = line_end;
                    lazy_paragraph_open = line_has_leaf_paragraph_content(indent.end, line_end);
                }
                item_end = next_line_start(line_end);
                item_inner_end = item_end;
            }

            std::u32string inner;
            std::vector<std::size_t> offset_map;
            std::vector<PrefixLine> prefix_lines;
            std::vector<std::size_t> lazy_lines;
            std::vector<BlockLineIndentation> nested_indentation;
            std::size_t inner_end_source = marker->content_start;
            auto append_range = [&](std::size_t begin, std::size_t end) {
                for (auto cursor = begin; cursor < end; ++cursor) {
                    offset_map.push_back(cursor);
                    inner.push_back(cps[cursor]);
                }
                inner_end_source = end;
            };
            for (std::size_t index = 0; index < marker->initial_prefix.size(); ++index) {
                inner.push_back(marker->initial_prefix[index]);
                offset_map.push_back(marker->initial_prefix_map[index]);
            }
            if (first_line_has_content) {
                nested_indentation.push_back({
                    0,
                    marker->initial_prefix.empty()
                        ? marker->content_physical_column
                        : marker->initial_physical_column,
                    marker->initial_prefix.empty() ? 0 : marker->initial_residual_columns});
            }
            append_range(marker->content_start, marker->content_end);
            prefix_lines.push_back({0, U"", false, ContainerPrefixRole::ItemOpening});
            const bool first_line_has_inner_content = !marker->initial_prefix.empty()
                || marker->content_start < marker->content_end;
            auto line_start = marker->source_end;
            while (line_start < item_inner_end) {
                const auto line_end = (std::min)(line_content_end(line_start), item_inner_end);
                const auto line_indent = scan_indentation(line_start, content_indent);
                const auto indent = line_indent;
                auto content = (std::min)(indent.end, line_end);
                const bool collapsed_marker_line = line_start == marker->source_end
                    && !first_line_has_inner_content && content < line_end;
                if (!collapsed_marker_line) {
                    offset_map.push_back(line_start > 0 ? line_start - 1 : line_start);
                    inner.push_back(U'\n');
                    inner_end_source = line_start;
                }
                const auto inner_line_start = inner.size();
                const bool lazy = content < line_end && indent.columns < content_indent;
                auto prefix = cps.substr(line_start, content - line_start);
                if (collapsed_marker_line) {
                    prefix_lines.front().text = cps.substr(marker->start + marker->text.size(),
                        marker->source_end - marker->start - marker->text.size()) + prefix;
                } else {
                    prefix_lines.push_back({inner_line_start, std::move(prefix), lazy});
                }
                if (lazy || is_lazy_line(line_start)) lazy_lines.push_back(inner_line_start);
                // Retain source characters after the container prefix. Any
                // partial tab columns require a semantic indentation mapping,
                // not synthetic characters in this block-local source.
                nested_indentation.push_back({
                    inner.size(), line_indent.physical_column, line_indent.residual_columns});
                append_range(content, line_end);
                line_start = line_end < item_end ? next_line_start(line_end) : line_end;
            }
            offset_map.push_back(inner_end_source);
            ParseInput nested_input(
                input->revision, cps_to_utf8(inner), input->dialect, {}, std::move(nested_indentation));
            nested_input.lazy_lines = std::move(lazy_lines);
            Parser nested(&nested_input);
            nested.node_counter = node_counter;
            auto children = nested.parse_blocks(nullptr);
            node_counter = nested.node_counter;
            for (std::size_t child_index = 1; child_index < children.size(); ++child_index) {
                const auto& separator = children[child_index].separator_before;
                const auto previous_serialized = serializer_detail::serialize_block(children[child_index - 1]);
                const bool previous_owns_ending = !previous_serialized.text.empty()
                    && is_line_ending_character(previous_serialized.text.back());
                if (separator && is_blank_separator(*separator, previous_owns_ending)) {
                    // Only direct block siblings of this item determine its
                    // list's looseness. A nested list's internal gaps remain
                    // owned by that nested list.
                    result.ensure_list_special().tight = false;
                    break;
                }
            }
            auto remap = [&](std::size_t value) {
                return std::size_t(offset_map[(std::min)(value, offset_map.size() - 1)]);
            };
            if (children.empty()) {
                BlockNode paragraph;
                paragraph.id = next_node_id();
                paragraph.kind = BlockKind::Paragraph;
                nested.push_range(paragraph.id, {0, 0}, {0, 0});
                nested.record_owned_offsets(paragraph);
                children.push_back(std::move(paragraph));
            }
            auto prefixes = bind_prefix_lines(inner, nested.source_ranges, prefix_lines);
            for (auto& range : nested.source_ranges) {
                range.source_range = PhysicalRange(remap(range.source_range.start), remap(range.source_range.end));
                range.content_range = PhysicalRange(remap(range.content_range.start), remap(range.content_range.end));
                for (auto& offset : range.owned_offsets) offset = remap(offset);
                if (range.separator_range) range.separator_range = PhysicalRange{
                    remap(range.separator_range->start), remap(range.separator_range->end)};
                for (auto& prefix : range.marker_ranges) prefix = PhysicalRange(remap(prefix.start), remap(prefix.end));
                source_ranges.push_back(std::move(range));
            }
            for (auto& diagnostic : nested.diagnostics) {
                diagnostics.push_back(std::move(diagnostic));
            }
            ParserSourceRange item_range(item_id, PhysicalRange(std::size_t(marker->start), std::size_t(item_end)), PhysicalRange(std::size_t(marker->content_start), std::size_t(inner_end_source)));
            if (separator_before_this_item) {
                item_range.separator_range = PhysicalRange{
                    marker->start - separator_before_this_item->size(), marker->start};
            }
            item_range.marker_ranges.push_back(PhysicalRange(std::size_t(marker->start), std::size_t(marker->content_start)));
            source_ranges.push_back(std::move(item_range));
            BlockNode item;
            item.id = item_id;
            item.kind = marker->task ? BlockKind::TaskListItem : BlockKind::ListItem;
            item.ensure_item_special().checked = marker->checked;
            item.ensure_item_special().marker = marker->text;
            item.separator_before = std::move(separator_before_this_item);
            item.ensure_container_source() = std::move(prefixes);
            item.children = std::move(children);
            // The stored gap starts after the item's serialized source, just
            // like separators between ordinary blocks. Code may already own
            // the ending of its last content line.
            if (separator_before_next_item) {
                const auto serialized = serializer_detail::serialize_list_item_blocks(item.children);
                if (!serialized.text.empty() && is_line_ending_character(serialized.text.back())) {
                    auto& gap = *separator_before_next_item;
                    const auto ending = gap.starts_with(U"\r\n") ? 2u
                        : !gap.empty() && is_line_ending_character(gap.front()) ? 1u : 0u;
                    gap.erase(0, ending);
                }
            }
            result.children.push_back(std::move(item));
            last_content = item_end;
            sibling_indent_limit = content_indent;
            pos = item_end;
            if (pos >= cps.size()) break;
        }
        push_range(list_id, PhysicalRange(std::size_t(start), std::size_t(pos)), PhysicalRange(std::size_t(first_content), std::size_t(last_content)));
        return result;
    }

    // ---- blockquote / callout ----
    std::optional<BlockNode> parse_blockquote_or_callout() {
        std::size_t start = pos;
        std::u32string inner;
        std::vector<std::size_t> offset_map;
        std::vector<PrefixLine> prefix_lines;
        std::vector<std::size_t> lazy_lines;
        std::vector<BlockLineIndentation> nested_indentation;
        std::vector<PhysicalRange> marker_ranges;
        std::size_t content_start = start;
        std::size_t content_end = start;
        bool first = true;
        bool last_line_empty = false;
        auto paragraph_content_line = [&](std::size_t line_start, std::size_t line_end, bool allow_container_line) {
            if (line_start >= line_end) return false;
            const auto indentation = scan_indentation(line_start);
            if (indentation.columns >= 4 || indentation.end >= line_end) return false;
            return line_has_leaf_paragraph_content(
                line_start, line_end, allow_container_line);
        };
        bool lazy_paragraph_open = false;
        while (pos < cps.size() && peek_line_start()) {
            const auto line_start = pos;
            const auto line_end = line_content_end(line_start);
            const auto quote_indent = scan_indentation(line_start, 3);
            const bool quoted = quote_indent.columns <= 3
                && quote_indent.end < line_end
                && cps[quote_indent.end] == U'>';
            const auto inner_line_start = inner.size();
            std::size_t line_content_start = line_start;
            if (quoted) {
                const auto marker_start = line_start;
                pos = quote_indent.end;
                auto physical_column = quote_indent.physical_column;
                advance();
                ++physical_column;
                std::size_t residual_columns = 0;
                if (peek1() == U' ') {
                    advance();
                    ++physical_column;
                } else if (peek1() == U'\t') {
                    const auto next = physical_column + (4 - physical_column % 4);
                    residual_columns = next - physical_column - 1;
                    physical_column = next;
                    advance();
                }
                line_content_start = pos;
                if (first) content_start = line_content_start;
                first = false;
                marker_ranges.push_back(PhysicalRange(std::size_t(marker_start), std::size_t(line_content_start)));
                nested_indentation.push_back({inner.size(), physical_column, residual_columns});
                bool blank = true;
                while (pos < line_end) {
                    if (cps[pos] != U' ' && cps[pos] != U'\t') blank = false;
                    offset_map.push_back(pos);
                    inner.push_back(cps[pos]);
                    advance();
                }
                last_line_empty = blank;
                content_end = line_end;
                lazy_paragraph_open = !blank
                    && paragraph_content_line(line_content_start, line_end, true);
            } else {
                if (!lazy_paragraph_open || is_blank_line_at(line_start)
                    || (!is_lazy_line(line_start) && line_starts_interrupting_block(line_start)
                        && !(setext_underline_at(line_start) && ch_at(scan_indentation(line_start).end) == U'='))) break;
                const auto indentation = scan_indentation(line_start);
                line_content_start = indentation.end;
                lazy_lines.push_back(inner_line_start);
                nested_indentation.push_back({inner.size(), indentation.physical_column, indentation.residual_columns});
                for (auto source = line_content_start; source < line_end; ++source) {
                    offset_map.push_back(source);
                    inner.push_back(cps[source]);
                }
                content_end = line_end;
                last_line_empty = false;
                lazy_paragraph_open = true;
                pos = line_end;
            }
            prefix_lines.push_back({inner_line_start,
                cps.substr(line_start, line_content_start - line_start), !quoted});
            if (pos < cps.size() && is_line_ending_character(cps[pos])) {
                const auto ending_start = pos;
                consume_line_ending();
                for (auto ending = ending_start; ending < pos; ++ending) {
                    offset_map.push_back(ending);
                    inner.push_back(cps[ending]);
                }
            }
        }
        // The block dispatcher may recognize an indented quote. If no quote
        // prefix was consumed, leave the input untouched so the caller can
        // try another block kind instead of returning a zero-width quote.
        if (first) return std::nullopt;
        auto source_end = pos;
        offset_map.push_back(source_end);
        auto first_newline = inner.find(U'\n');
        auto first_line = first_newline == std::u32string::npos ? inner : inner.substr(0, first_newline);
        auto callout = input->dialect.callouts
            ? try_parse_callout_from_line(first_line)
            : std::optional<BlockNode>{};
        if (callout) {
            callout->ensure_text_special().opening_marker = std::u32string(
                std::u32string_view(cps).substr(start, content_start - start))
                + callout->text_special().opening_marker;
            if (const auto* title = callout_title_block(*callout)) {
                const auto title_start = first_line.size() - title->inline_content.source.size();
                source_ranges.emplace_back(
                    title->id,
                    PhysicalRange(offset_map[title_start], offset_map[first_line.size()]),
                    PhysicalRange(offset_map[title_start], offset_map[first_line.size()]));
            }
        }
        auto body_start = callout ? (first_newline == std::u32string::npos ? inner.size() : first_newline + 1) : 0;
        auto body = inner.substr(body_start);
        std::vector<BlockLineIndentation> body_indentation;
        for (auto context : nested_indentation) {
            if (context.source_offset < body_start) continue;
            context.source_offset -= body_start;
            body_indentation.push_back(context);
        }
        ParseInput nested_input(
            input->revision, cps_to_utf8(body), input->dialect, {}, std::move(body_indentation));
        for (const auto line : lazy_lines) {
            if (line >= body_start) nested_input.lazy_lines.push_back(line - body_start);
        }
        Parser nested(&nested_input);
        nested.node_counter = node_counter;
        auto children = nested.parse_blocks(nullptr);
        node_counter = nested.node_counter;
        auto remap = [&](std::size_t value) {
            auto index = (std::min)(body_start + value, offset_map.size() - 1);
            return std::size_t(offset_map[index]);
        };
        const auto trailing_blank_belongs_to_unclosed_fence = [&] {
            if (!last_line_empty || children.empty()) return false;
            const auto& final_child = children.back();
            if (final_child.kind != BlockKind::CodeBlock
                || final_child.block_source.tree().kind != BlockSourceKind::FencedCode
                || final_child.block_source.tree().complete_closing) {
                return false;
            }

            // An unclosed fence consumes through nested EOF.  Only suppress
            // the synthetic quote-boundary paragraph when its lossless raw
            // block source proves it owns the body's exact suffix, including
            // the final quoted blank line.
            const auto& raw = final_child.block_source.source();
            return !raw.empty()
                && raw.size() <= body.size()
                && std::u32string_view(body).substr(body.size() - raw.size()) == raw;
        }();
        if (children.empty()) {
            BlockNode paragraph;
            paragraph.id = next_node_id();
            paragraph.kind = BlockKind::Paragraph;
            nested.push_range(paragraph.id, {0, 0}, {0, 0});
            nested.record_owned_offsets(paragraph);
            children.push_back(std::move(paragraph));
        } else if (last_line_empty
            && !trailing_blank_belongs_to_unclosed_fence
            && !(children.back().kind == BlockKind::Paragraph
                && std::ranges::all_of(children.back().inline_content.source, [](char32_t ch) {
                    return ch == U' ' || ch == U'\t' || ch == U'\r' || ch == U'\n';
                }))) {
            BlockNode paragraph;
            paragraph.id = next_node_id();
            paragraph.kind = BlockKind::Paragraph;
            const auto blank = body.size();
            const auto previous = serializer_detail::serialize_block(children.back());
            if (!previous.text.empty() && is_line_ending_character(previous.text.back())) {
                paragraph.separator_before = U"";
            } else {
                auto ending = blank;
                if (ending > 0 && body[ending - 1] == U'\n') --ending;
                if (ending > 0 && body[ending - 1] == U'\r') --ending;
                paragraph.separator_before = body.substr(ending);
            }
            nested.push_range(paragraph.id, {blank, blank}, {blank, blank});
            nested.record_owned_offsets(paragraph);
            children.push_back(std::move(paragraph));
        }
        auto body_prefixes = prefix_lines;
        if (body_start > 0) {
            std::erase_if(body_prefixes, [&](const auto& line) { return line.inner_start < body_start; });
            for (auto& line : body_prefixes) line.inner_start -= body_start;
        }
        auto prefixes = bind_prefix_lines(body, nested.source_ranges, body_prefixes);
        for (auto& range : nested.source_ranges) {
            range.source_range = PhysicalRange(remap(range.source_range.start), remap(range.source_range.end));
            range.content_range = PhysicalRange(remap(range.content_range.start), remap(range.content_range.end));
            for (auto& offset : range.owned_offsets) offset = remap(offset);
            if (range.separator_range) range.separator_range = PhysicalRange{
                remap(range.separator_range->start), remap(range.separator_range->end)};
            for (auto& marker : range.marker_ranges) marker = PhysicalRange(remap(marker.start), remap(marker.end));
            source_ranges.push_back(std::move(range));
        }
        for (auto& diagnostic : nested.diagnostics) {
            diagnostics.push_back(std::move(diagnostic));
        }
        NodeId id = next_node_id();
        ParserSourceRange quote_range(id, PhysicalRange(std::size_t(start), std::size_t(source_end)), PhysicalRange(std::size_t(content_start), std::size_t(content_end)));
        quote_range.marker_ranges = std::move(marker_ranges);
        source_ranges.push_back(std::move(quote_range));
        if (callout) {
            callout->id = id;
            callout->children.insert(
                callout->children.end(),
                std::make_move_iterator(children.begin()),
                std::make_move_iterator(children.end()));
            return callout;
        }
        BlockNode b;
        b.id = id;
        b.kind = BlockKind::BlockQuote;
        b.ensure_container_source() = std::move(prefixes);
        b.children = std::move(children);
        return b;
    }
    std::optional<BlockNode> try_parse_callout_from_line(const std::u32string& line) {
        std::size_t marker_start = 0;
        while (marker_start < line.size()
            && (line[marker_start] == U' ' || line[marker_start] == U'\t')) ++marker_start;
        if (marker_start + 1 < line.size()
            && line[marker_start] == U'[' && line[marker_start + 1] == U'!') {
            std::size_t kind_end = std::u32string::npos;
            for (std::size_t i = marker_start + 2; i < line.size(); ++i) {
                if (line[i] == U']') { kind_end = i; break; }
            }
            if (kind_end == std::u32string::npos) return std::nullopt;
            std::u32string kind = substr_cps_(line, marker_start + 2, kind_end - marker_start - 2);
            auto normalized_kind = normalize_callout_kind(cps_to_utf8(kind));
            if (!normalized_kind) return std::nullopt;
            auto title_start = kind_end + 1;
            if (title_start < line.size()
                && (line[title_start] == U' ' || line[title_start] == U'\t')) ++title_start;
            std::u32string title_part = title_start < line.size()
                ? substr_cps_(line, title_start, std::u32string::npos)
                : U"";
            if (!title_part.empty()) {
                BlockNode title;
                title.id = next_node_id();
                title.kind = BlockKind::CalloutTitle;
                title.inline_content.source = std::move(title_part);
                title.inline_content.tree = parse_inline(
                    title.inline_content.source,
                    inline_parse_context());
                BlockNode b;
                b.id = NodeId(0);
                b.kind = BlockKind::Callout;
                b.ensure_container_special().callout_kind = *normalized_kind;
                b.children.push_back(std::move(title));
                b.ensure_text_special().opening_marker = line.substr(0, title_start);
                return b;
            }
            BlockNode b; b.id = NodeId(0); b.kind = BlockKind::Callout; b.ensure_container_special().callout_kind = *normalized_kind;
            b.ensure_text_special().opening_marker = line.substr(0, title_start);
            return b;
        }
        return std::nullopt;
    }

    // ---- thematic break ----
    std::optional<BlockNode> try_parse_thematic_break() {
        std::size_t start = pos;
        std::size_t line_end = start;
        if (!line_is_thematic_break(start, &line_end)) return std::nullopt;
        pos = line_end;
        consume_line_ending();
        NodeId id = next_node_id();
        push_range(id, PhysicalRange(std::size_t(start), std::size_t(line_end)), PhysicalRange(std::size_t(start), std::size_t(line_end)));
        BlockNode b; b.id = id; b.kind = BlockKind::ThematicBreak;
        b.ensure_text_special().opening_marker = cps.substr(start, line_end - start);
        return b;
    }

    // ---- TOC ----
    bool try_match_toc() {
        if (peek1()=='[' && peek(1)=='T' && peek(2)=='O' && peek(3)=='C' && peek(4)==']') return true;
        if (peek1()=='[' && peek(1)=='[' && peek(2)=='t' && peek(3)=='o' && peek(4)=='c' && peek(5)==']' && peek(6)==']') return true;
        return false;
    }
    std::optional<BlockNode> parse_toc() {
        std::size_t start = pos;
        TocMarkerKind mk = TocMarkerKind::BracketToc;
        if (peek1()=='[' && peek2()=='T') { advance_n(5); mk = TocMarkerKind::BracketToc; }
        else { advance_n(7); mk = TocMarkerKind::WikiToc; }
        consume_line_ending();
        NodeId id = next_node_id();
        push_range(id, PhysicalRange(std::size_t(start), cur()), PhysicalRange(std::size_t(start), cur()));
        BlockNode b; b.id = id; b.kind = BlockKind::Toc; b.ensure_atomic_special().toc_marker = mk;
        return b;
    }

    // helpers
    static bool is_alnum_(char32_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }
    static bool is_ascii_digit_(char32_t c) { return c >= '0' && c <= '9'; }
    static std::u32string to_lower_(const std::u32string& s) {
        std::u32string r = s;
        for (auto& c : r) if (c >= 'A' && c <= 'Z') c = static_cast<char32_t>(c - 'A' + 'a');
        return r;
    }
    static bool starts_with_(const std::u32string& s, std::u32string_view p) {
        if (s.size() < p.size()) return false;
        for (std::size_t i = 0; i < p.size(); ++i) if (s[i] != p[i]) return false;
        return true;
    }
    static std::u32string trim_cps_(const std::u32string& s) {
        std::size_t a = 0, b = s.size();
        while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
        while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\r')) --b;
        return s.substr(a, b - a);
    }

    static std::string trim_utf8(const std::string& s) {
        std::size_t a = 0, b = s.size();
        while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
        while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\r' || s[b-1] == '\n')) --b;
        return s.substr(a, b - a);
    }
    static std::u32string substr_cps_(const std::u32string& s, std::size_t off, std::size_t n) {
        if (off >= s.size()) return {};
        return s.substr(off, (n == std::u32string::npos) ? std::u32string::npos : n);
    }
    std::size_t char_to_byte_in_input_(std::size_t char_idx) const {
        std::size_t byte = 0;
        for (std::size_t i = 0; i < char_idx && i < cps.size(); ++i)
            byte += utf8_seq_len_(cps[i]);
        return byte;
    }
    static std::size_t utf8_seq_len_(char32_t c) {
        if (c <= 0x7F) return 1;
        if (c <= 0x7FF) return 2;
        if (c <= 0xFFFF) return 3;
        return 4;
    }
};

} // namespace detail

// Public entry point.
inline ParseOutput parse(const ParseInput& input) {
    record_full_document_parse();
    detail::Parser p(&input);
    auto blocks = p.parse_blocks(nullptr);
    EditorDocument doc;
    doc.root.id = NodeId(p.node_counter++);
    doc.next_node_id = p.node_counter;
    doc.dialect = input.dialect;
    doc.revision = input.revision;
    doc.root.children = std::move(blocks);
    if (!p.cps.empty() && p.cps.back() == U'\n') {
        doc.trailing_line_ending = p.cps.size() >= 2 && p.cps[p.cps.size() - 2] == U'\r'
            ? U"\r\n"
            : U"\n";
    } else if (!p.cps.empty() && p.cps.back() == U'\r') {
        doc.trailing_line_ending = U"\r";
    }

    // metadata: frontmatter first
    bool has_fm = false;
    for (const auto& b : doc.root.children) {
        if (b.kind == BlockKind::Frontmatter) {
            doc.metadata = from_frontmatter(b.atomic_special().raw, b.atomic_special().fmt);
            has_fm = true;
            break;
        }
    }
    if (!has_fm) {
        std::size_t lc = 0, wc = 0; std::u32string buf;
        auto flush = [&]() {
            if (!buf.empty()) { ++wc; buf.clear(); }
        };
        for (char32_t c : p.cps) {
            if (c == '\n') ++lc;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') flush();
            else buf.push_back(c);
        }
        flush();
        doc.metadata.char_count = p.cps.size();
        doc.metadata.line_count = lc + 1;
        doc.metadata.word_count = wc;
    }

    doc.diagnostics = std::move(p.diagnostics);

    ParseOutput out;
    out.revision = input.revision;
    out.document = std::move(doc);
    rebuild_document_block_index(out.document);
    out.symbols = build_document_symbol_index(out.document, &out.symbol_contributions);
    out.outline = build_outline_from_headings(input.revision, out.symbols.headings);
    out.diagnostics = out.document.diagnostics;
    return out;
}

inline ParseOutput parse_text(
    std::uint64_t rev,
    const std::string& text,
    MarkdownDialect d = default_dialect(),
    ParseProgressCallback progress = {}) {
    return parse(ParseInput(rev, text, d, std::move(progress)));
}

inline ParsedBlockFragment parse_block_fragment(
    std::u32string_view source,
    MarkdownDialect dialect = default_dialect()) {
    ParseInput input{0, cps_to_utf8(source), std::move(dialect)};
    detail::Parser parser(&input);
    ParsedBlockFragment result;
    result.blocks = parser.parse_blocks(nullptr);
    result.source_ranges.reserve(parser.source_ranges.size());
    for (const auto& range : parser.source_ranges) {
        result.source_ranges.push_back({
            range.node_id,
            {range.source_range.start, range.source_range.end},
            {range.content_range.start, range.content_range.end},
        });
    }
    return result;
}

} // namespace folia
