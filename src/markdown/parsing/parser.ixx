// folia.markdown.parser — Markdown parser. Pure core, no Windows dependency.
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
//   * container recognition uses one explicit parse-session frame stack
//
// All offsets are CHAR indices into a Vec<char32> snapshot.
export module folia.markdown.parser;
import std;
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.image_dimension;
import folia.markdown.diagnostics;
import folia.markdown.metadata;
import folia.markdown.symbols;
import folia.markdown.ast;
import folia.markdown.container_source;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.block_line_recognizer;
import folia.markdown.callout;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.document_symbols;
import folia.markdown.outline;
import folia.markdown.slug;
import folia.markdown.utf;
import folia.markdown.link_label;
import folia.markdown.html_entities;
import folia.markdown.markdown_html;
import folia.markdown.inline_cst;
import folia.markdown.parser_source_session;
import folia.markdown.parser_leaf_line_view;
import folia.markdown.parser_code_math_leaf_source;
import folia.markdown.parser_html_source_view;
import folia.markdown.image_block_promotion;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.html_cst;
import folia.markdown.instrumentation;
import folia.markdown.text_edit;

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
    std::function<std::optional<InlineLinkDef>(const std::string&)> references_before;
    std::function<std::optional<InlineLinkDef>(const std::string&)> references_after;

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
    enum class Completion : std::uint8_t { Empty, Other, LineEnding };
    using LeafLine = parser_source_detail::LeafLine;
    using PhysicalLineCursor = parser_source_detail::PhysicalLineCursor;
    using LeafSourceAccumulator = parser_source_detail::LeafSourceAccumulator;
    // Transient facts are emitted at recognizer completion. They are not AST
    // state and deliberately carry no serialized text.
    std::unordered_map<std::uint64_t, Completion> completions;
    std::unordered_map<std::string, LinkDefinition> link_definitions;

    explicit Parser(const ParseInput* in) : input(in) {
        cps = utf8_to_cps(in->text);
        // Definitions are collected by the sole block dispatch before inline finalization.
    }
    Parser(Parser const&) = delete;
    Parser& operator=(Parser const&) = delete;
    Parser(Parser&&) = delete;
    Parser& operator=(Parser&&) = delete;

    static Completion completion_from_source(std::u32string_view source) {
        if (source.empty()) return Completion::Empty;
        return is_line_ending_character(source.back()) ? Completion::LineEnding : Completion::Other;
    }

    static Completion completion_from_utf8(std::string_view source) {
        if (source.empty()) return Completion::Empty;
        return source.back() == '\r' || source.back() == '\n'
            ? Completion::LineEnding : Completion::Other;
    }

    void record_completion(NodeId id, Completion fact) {
        completions.insert_or_assign(id.v, fact);
    }

    Completion completion(NodeId id) const { return completions.at(id.v); }

    static Completion append_completion(Completion previous, Completion separator, Completion child) {
        if (child != Completion::Empty) return child;
        if (separator != Completion::Empty) return separator;
        return previous;
    }

    Completion sequence_completion(const BlockVec& blocks) const {
        auto result = Completion::Empty;
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            auto separator = Completion::Empty;
            if (index > 0) {
                if (blocks[index].separator_before) {
                    separator = completion_from_source(*blocks[index].separator_before);
                } else {
                    separator = Completion::LineEnding;
                }
            }
            result = append_completion(result, separator, completion(blocks[index].id));
        }
        return result;
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



    InlineParseContext inline_parse_context() {
        InlineParseContext context;
        context.dialect = input->dialect;
        context.allocate_id = [this] { return next_node_id(); };
        context.resolve_link_label = [this](const std::string& label) -> std::optional<InlineLinkDef> {
            if (input->references_before)
                if (auto definition=input->references_before(label)) return definition;
            const auto found = link_definitions.find(label);
            if (found == link_definitions.end())
                return input->references_after ? input->references_after(label) : std::nullopt;
            return InlineLinkDef{found->second.href, found->second.title};
        };
        return context;
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
        return line_recognizer().line_is_thematic_break(start,end);
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



    bool is_lazy_line(std::size_t at) const {
        return std::ranges::find(input->lazy_lines, at) != input->lazy_lines.end();
    }



    // Lookup physical source ranges produced by this one parse session.
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


// Sole block dispatch loop. Source discovery precedes final inline CSTs.
std::vector<BlockNode> parse_blocks(std::function<bool(const std::u32string&)> stop=nullptr) {
    if (!active_container_frames.empty()) throw std::logic_error("block session already active");
    BlockNode root; root.kind=BlockKind::Document;
    push_container_frame(std::move(root),DocumentFrameState{},{0,0},{0,0});
    while (const auto* first=peek_leaf_line()) {
        // A newly dispatched block cannot begin as lazy continuation. Only an
        // already open paragraph may accept the engine's staged lazy offer.
        if (!leaf_line_fully_matched(*first)) {
            const auto matched=first->matched_container_frames;
            while (active_container_frames.size()>matched) close_innermost_frame();
            continue;
        }
        if (std::holds_alternative<ListFrameState>(container_frames[active_container_frames.back()].state)) {
            const auto marker=consume_list_opening(physical_source(),first->content_cursor,input->dialect.gfm.task_lists);
            const auto& list=std::get<ListFrameState>(container_frames[active_container_frames.back()].state);
            const bool thematic=BlockLineRecognizer(copy_leaf_line_text(physical_source(),*first)).line_is_thematic_break(0);
            if (thematic || !marker || !same_list_kind(list.first,*marker) || marker->indent>=list.sibling_indent_limit) {
                close_innermost_frame();
                continue;
            }
            if (!open_list_item_frame()) throw std::logic_error("compatible item rejected");
            continue;
        }
        if (leaf_line_blank(physical_source(),*first)) {
            parse_frame_blank_lines();
            report_progress();
            continue;
        }
        if (stop && stop(copy_leaf_line_text(physical_source(),*first))) break;
        if (first->at_logical_line_start) {
            if (auto block=try_parse_frontmatter(*first)) {
                append_completed_block(container_frames[active_container_frames.back()],std::move(*block),pos);
                continue;
            }
            if (open_footnote_frame()) continue;
            if (auto block=try_parse_link_definition(*first)) {
                append_completed_block(container_frames[active_container_frames.back()],std::move(*block),pos);
                continue;
            }
            if (auto block=try_parse_atx_heading(*first)) {
                append_completed_block(container_frames[active_container_frames.back()],std::move(*block),pos);
                continue;
            }
            if (open_quote_frame()) {
                recognize_callout_header();
                continue;
            }
        }
        std::optional<BlockNode> block;
        if (first->at_logical_line_start) {
            block=try_parse_indented_code_block(*first);
            if (!block) block=try_parse_code_block(*first);
            if (!block) block=try_parse_math_block(*first);
            if (!block) block=try_parse_table(*first);
            if (!block) block=try_parse_thematic_break(*first);
            if (!block && open_list_item_frame()) continue;
            if (!block) block=try_parse_toc(*first);
        }
        if (!block) block=try_parse_raw_html_block(*first);
        if (!block) block=try_parse_paragraph(*first);
        if (!block) throw std::logic_error("block dispatch made no progress");
        append_completed_block(container_frames[active_container_frames.back()],std::move(*block),pos);
        report_progress();
    }
    while (active_container_frames.size()>1) close_innermost_frame();
    auto& completed=container_frames[active_container_frames.back()].node;
    finalize_inline_owners(completed);
    bind_completed_prefixes(completed);
    active_container_frames.clear();
    return std::move(completed.children);
}






    // ---- frontmatter ----


    // ---- heading ----


    // The block parser owns paragraph boundaries only. Inline syntax is
    // parsed exactly once into the node-local lossless CST.






// Replacement for Parser's HTML source/semantic helpers through
// try_parse_raw_html_block. This is pasted inside the existing Parser class.
// Required existing imports: parser_source_session, parser_leaf_line_view,
// parser_html_source_view, html_cst, markdown_html, image_block_promotion.
using HtmlSourceView = parser_source_detail::HtmlSourceView;
using LeafLineText = parser_source_detail::LeafLineText;
using PhysicalSourceRange = parser_source_detail::PhysicalSourceRange;
using RootPhysicalSource = parser_source_detail::RootPhysicalSource;

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
    const HtmlSourceView& source) const {
    const auto found = std::ranges::find_if(node.attributes, [&](const auto& attribute) {
        return attribute.name == name;
    });
    if (found == node.attributes.end() || !found->value_range) return std::nullopt;
    const auto value = source.slice(found->value_range->start, found->value_range->end);
    if (!value) return std::nullopt;
    return std::u32string(*value);
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
    const HtmlSourceView& source) const {
    std::optional<TextAlignment> result;
    if (auto align = html_attribute_value(node, "align", source)) {
        result = parse_html_text_alignment(cps_to_utf8(*align));
    }
    auto style = html_attribute_value(node, "style", source);
    if (!style) return result;

    // CSS declarations cascade in source order. Only the safe enumerated
    // text-align property is projected; exact style spelling stays in the CST.
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
    const HtmlSourceView& source) {
    std::vector<SourceRange> markers;
    if (!node.opening.empty()) markers.push_back(node.opening);
    if (node.closing) markers.push_back(*node.closing);
    push_leaf_range(id, source.physical_map(), node.range, node.content, markers);
}

// Register precisely the local source owned by one editable HTML child. The
// envelope accumulator can have discontiguous physical pieces after container
// prefixes; do not turn its bounding range into ownership for this child.
void register_html_owner_source(
    NodeId id,
    const HtmlSourceView& source,
    SourceRange local_source) {
    if (!local_source.valid_for(source.size())) {
        throw std::logic_error("HTML owner range is outside its leaf source");
    }
    LeafSourceAccumulator owner_source(physical_source());
    const auto& physical_map = source.physical_map();
    if (local_source.empty()) {
        const auto point = physical_map.physical_range(local_source.start, local_source.end);
        if (!point) throw std::logic_error("empty HTML owner has no physical boundary");
        owner_source.append(*point);
    } else {
        for (const auto& piece : physical_map.pieces()) {
            const auto piece_begin = piece.source_begin;
            const auto piece_end = piece_begin + piece.physical.size();
            const auto begin = (std::max)(local_source.start, piece_begin);
            const auto end = (std::min)(local_source.end, piece_end);
            if (begin >= end) continue;
            owner_source.append({
                piece.physical.begin + begin - piece_begin,
                piece.physical.begin + end - piece_begin});
        }
    }
    register_leaf_ownership(id, owner_source);
}

void attach_html_element(
    BlockNode& block,
    const HtmlCstNode& node,
    const HtmlSourceView& source) {
    auto& html = block.ensure_html_special();
    html.root_tag = node.tag_name;
    html.text_alignment = html_text_alignment(node, source);
    if (const auto opening = source.slice(node.opening.start, node.opening.end)) {
        html.opening_marker.assign(opening->begin(), opening->end());
    }
    if (node.closing) {
        if (const auto closing = source.slice(node.closing->start, node.closing->end)) {
            html.closing_marker.assign(closing->begin(), closing->end());
        }
    }
}

BlockNode make_html_inline_block(
    BlockKind kind,
    const HtmlCstNode& node,
    const HtmlSourceView& source,
    std::vector<HtmlContentSlot>& slots,
    std::uint8_t heading_level = 0) {
    const auto body = source.slice(node.content.start, node.content.end);
    if (!body) throw std::logic_error("invalid HTML inline content range");
    BlockNode block;
    block.id = next_node_id();
    block.kind = kind;
    block.inline_content.source.assign(body->begin(), body->end());
    block.inline_content.syntax_mode = InlineSyntaxMode::HtmlText;
    if (heading_level != 0) block.ensure_text_special().level = heading_level;
    attach_html_element(block, node, source);
    register_html_owner_source(block.id, source, node.content);
    push_html_range(block.id, node, source);
    slots.push_back({block.id, node.content});
    auto tail = completion_from_source(block.inline_content.source);
    if (kind == BlockKind::Heading && tail == Completion::Empty) tail = Completion::Other;
    record_completion(block.id, tail);
    return block;
}

BlockNode make_html_inline_run(
    SourceRange range,
    const HtmlSourceView& source,
    std::vector<HtmlContentSlot>& slots) {
    const auto body = source.slice(range.start, range.end);
    if (!body) throw std::logic_error("invalid HTML inline run range");
    BlockNode block;
    block.id = next_node_id();
    block.kind = BlockKind::Paragraph;
    block.inline_content.source.assign(body->begin(), body->end());
    block.inline_content.syntax_mode = InlineSyntaxMode::HtmlText;
    register_html_owner_source(block.id, source, range);
    push_leaf_range(block.id, source.physical_map(), range, range);
    slots.push_back({block.id, range});
    record_completion(block.id, completion_from_source(block.inline_content.source));
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
    const HtmlSourceView& source,
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
                BlockKind::TableCell, html_cell, source, slots));
        }
        if (row.children.empty()) continue;
        columns = (std::max)(columns, row.children.size());
        row.ensure_table_special().table_header_row = header;
        attach_html_element(row, *html_row, source);
        push_html_range(row.id, *html_row, source);
        record_completion(row.id, Completion::Other);
        table.children.push_back(std::move(row));
    }
    if (table.children.empty()) return std::nullopt;
    table.ensure_table_special().table_aligns.assign(columns, TableAlignment::None);
    attach_html_element(table, node, source);
    push_html_range(table.id, node, source);
    record_completion(table.id, Completion::Other);
    return table;
}

std::vector<BlockNode> make_html_children(
    std::span<const HtmlCstNode> children,
    const HtmlSourceView& source,
    std::vector<HtmlContentSlot>& slots) {
    std::vector<BlockNode> blocks;
    std::optional<SourceRange> inline_run;
    auto flush_inline = [&]() {
        if (!inline_run) return;
        const auto text = source.slice(inline_run->start, inline_run->end);
        if (!text) throw std::logic_error("invalid HTML inline child range");
        if (!html_whitespace_only(*text)) {
            blocks.push_back(make_html_inline_run(*inline_run, source, slots));
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
        if (auto block = make_html_semantic(child, source, slots)) {
            blocks.push_back(std::move(*block));
        }
    }
    flush_inline();
    return blocks;
}

std::optional<BlockNode> make_html_list(
    const HtmlCstNode& node,
    const HtmlSourceView& source,
    std::vector<HtmlContentSlot>& slots) {
    BlockNode list;
    list.id = next_node_id();
    list.kind = BlockKind::List;
    list.ensure_list_special().ordered = node.tag_name == "ol";
    if (auto start = html_attribute_value(node, "start", source)) {
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
        item.children = make_html_children(child.children, source, slots);
        if (item.children.empty()) {
            item.children.push_back(make_html_inline_run(
                {child.content.start, child.content.start}, source, slots));
        }
        attach_html_element(item, child, source);
        push_html_range(item.id, child, source);
        record_completion(item.id, sequence_completion(item.children));
        list.children.push_back(std::move(item));
    }
    if (list.children.empty()) return std::nullopt;
    attach_html_element(list, node, source);
    push_html_range(list.id, node, source);
    auto tail = sequence_completion(list.children);
    if (tail == Completion::Empty) tail = Completion::Other; // generated list marker
    record_completion(list.id, tail);
    return list;
}

std::optional<BlockNode> make_html_semantic(
    const HtmlCstNode& node,
    const HtmlSourceView& source,
    std::vector<HtmlContentSlot>& slots,
    bool allow_image_block = false) {
    if (node.kind != HtmlCstKind::Element) return std::nullopt;
    if (allow_image_block && node.tag_name == "img") {
        auto image = make_html_image_block_from_element(
            node,
            next_node_id(),
            true,
            [&](const HtmlCstNode& element, std::string_view name) {
                return html_attribute_value(element, name, source);
            },
            [&](std::string_view target, bool image_target) {
                return safe_link_target(target, image_target);
            });
        if (image) {
            attach_html_element(*image, node, source);
            return image;
        }
    }
    if (node.tag_name == "p" || node.tag_name == "summary"
        || node.tag_name == "dt" || node.tag_name == "dd") {
        return make_html_inline_block(BlockKind::Paragraph, node, source, slots);
    }
    if (node.tag_name.size() == 2 && node.tag_name[0] == 'h'
        && node.tag_name[1] >= '1' && node.tag_name[1] <= '6') {
        return make_html_inline_block(
            BlockKind::Heading, node, source, slots,
            static_cast<std::uint8_t>(node.tag_name[1] - '0'));
    }
    if (node.tag_name == "table") return make_html_table(node, source, slots);
    if (node.tag_name == "ul" || node.tag_name == "ol") {
        return make_html_list(node, source, slots);
    }
    if (node.tag_name == "blockquote") {
        BlockNode quote;
        quote.id = next_node_id();
        quote.kind = BlockKind::BlockQuote;
        quote.children = make_html_children(node.children, source, slots);
        if (quote.children.empty()) {
            quote.children.push_back(make_html_inline_run(
                {node.content.start, node.content.start}, source, slots));
        }
        attach_html_element(quote, node, source);
        push_html_range(quote.id, node, source);
        const auto tail = !quote.children.empty()
                && quote.children.back().kind == BlockKind::Paragraph
                && quote.children.back().inline_content.source.empty()
            ? Completion::Other : sequence_completion(quote.children);
        record_completion(quote.id, tail);
        return quote;
    }
    if (node.tag_name == "hr") {
        BlockNode rule;
        rule.id = next_node_id();
        rule.kind = BlockKind::ThematicBreak;
        attach_html_element(rule, node, source);
        push_html_range(rule.id, node, source);
        record_completion(rule.id, Completion::Other);
        return rule;
    }
    if (node.tag_name == "pre") {
        const auto raw = source.slice(node.range.start, node.range.end);
        if (!raw) throw std::logic_error("invalid HTML pre source range");
        BlockNode code;
        code.id = next_node_id();
        code.kind = BlockKind::CodeBlock;
        code.block_source = make_block_source(std::u32string(*raw), BlockSourceKind::HtmlCode);
        attach_html_element(code, node, source);
        register_html_owner_source(code.id, source, node.range);
        slots.push_back({code.id, node.range});
        push_html_range(code.id, node, source);
        record_completion(code.id, completion_from_source(code.block_source.source()));
        return code;
    }

    BlockNode container;
    container.id = next_node_id();
    container.kind = BlockKind::HtmlContainer;
    container.children = make_html_children(node.children, source, slots);
    if (container.children.empty()) {
        container.children.push_back(make_html_inline_run(node.content, source, slots));
    }
    attach_html_element(container, node, source);
    push_html_range(container.id, node, source);
    record_completion(container.id, sequence_completion(container.children));
    return container;
}

static std::u32string copy_leaf_line(
    const RootPhysicalSource& root,
    const LeafLine& line) {
    LeafLineText text(root, line);
    std::u32string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        result.push_back(text.at(index));
    }
    return result;
}

std::optional<BlockNode> try_parse_raw_html_block(const LeafLine& first) {
    if (!first.at_logical_line_start || !leaf_line_fully_matched(first))
        return std::nullopt;
    const auto first_text = copy_leaf_line(physical_source(), first);
    const auto opening = scan_markdown_html_block_start(
        first_text, 0, first_text.size());
    if (!opening) return std::nullopt;

    std::size_t accepted_lines = 0;
    for (std::size_t lookahead = 0; ; ++lookahead) {
        const auto* candidate = peek_leaf_line(lookahead);
        if (!candidate || !leaf_line_fully_matched(*candidate)) break;
        const auto line = copy_leaf_line(physical_source(), *candidate);
        if (lookahead != 0
            && (opening->kind == MarkdownHtmlBlockKind::BlockTag
                || opening->kind == MarkdownHtmlBlockKind::OtherTag)
            && html_whitespace_only(line)) {
            break; // Preserve html_block_end's blank-line boundary.
        }
        ++accepted_lines;
        if (markdown_html_block_ends_on_line(opening->kind, line, 0, line.size())) break;
    }
    if (accepted_lines == 0) return std::nullopt;

    LeafSourceAccumulator html_leaf(physical_source());
    PhysicalSourceRange final_ending{};
    for (std::size_t index = 0; index < accepted_lines; ++index) {
        const auto* line = peek_leaf_line();
        if (!line) throw std::logic_error("HTML leaf lookahead changed");
        if (index + 1 == accepted_lines) final_ending = line->ending;
        html_leaf.append_content(*line);
        if (index + 1 < accepted_lines) html_leaf.append_ending(*line);
        accept_leaf_line(*line);
    }

    const auto raw_size = html_leaf.size();
    const bool type7 = opening->kind == MarkdownHtmlBlockKind::OtherTag;
    bool maybe_standalone_img = false;
    if (type7) {
        const auto tag = scan_markdown_html_tag(
            first_text, opening->tag_start.start, first_text.size());
        maybe_standalone_img = tag && !tag->closing && tag->name == "img"
            && tag->range.end == first_text.size();
    }

    // Materialize once without the final physical line ending, then append
    // only that ending to the mapping accumulator. This makes consumed source
    // and content/owner source distinct before push_leaf_range indexes them.
    HtmlSourceView source(html_leaf);
    html_leaf.append(final_ending);
    const auto consumed_size = html_leaf.size();

    if (type7 && !maybe_standalone_img) {
        BlockNode paragraph;
        paragraph.id = next_node_id();
        paragraph.kind = BlockKind::Paragraph;
        paragraph.inline_content.source.assign(source.source());
        paragraph.inline_content.syntax_mode = InlineSyntaxMode::HtmlText;
        register_html_owner_source(paragraph.id, source, {0, raw_size});
        push_leaf_range(paragraph.id, html_leaf,
            {0, consumed_size}, {0, raw_size});
        record_completion(paragraph.id,
            completion_from_source(paragraph.inline_content.source));
        return paragraph;
    }

    const auto& raw = source.source();
    auto tree = parse_html_cst(raw); // One CST for this completed raw HTML leaf.
    if (type7) {
        const HtmlCstNode* image = nullptr;
        bool sole_image = true;
        for (const auto& node : tree.nodes) {
            if (node.kind == HtmlCstKind::Text) {
                const auto text = source.slice(node.range.start, node.range.end);
                if (text && html_whitespace_only(*text)) continue;
            }
            if (image || node.kind != HtmlCstKind::Element || node.tag_name != "img") {
                sole_image = false;
                break;
            }
            image = &node;
        }
        const bool complete_image = image && !tree.has_error
            && std::ranges::all_of(tree.nodes, [](const HtmlCstNode& node) {
                return node.status == HtmlParseStatus::Complete;
            });
        std::vector<HtmlContentSlot> slots;
        auto block = complete_image && sole_image
            ? make_html_semantic(*image, source, slots, true)
            : std::optional<BlockNode>{};
        if (!block || block->kind != BlockKind::ImageBlock) {
            BlockNode paragraph;
            paragraph.id = next_node_id();
            paragraph.kind = BlockKind::Paragraph;
            paragraph.inline_content.source.assign(source.source());
            paragraph.inline_content.syntax_mode = InlineSyntaxMode::HtmlText;
            register_html_owner_source(paragraph.id, source, {0, raw_size});
            push_leaf_range(paragraph.id, html_leaf,
                {0, consumed_size}, {0, raw_size});
            record_completion(paragraph.id,
                completion_from_source(paragraph.inline_content.source));
            return paragraph;
        }
        auto& html = block->ensure_html_special();
        html.source.assign(source.source());
        html.tree = std::move(tree);
        html.content_slots = std::move(slots);
        html.structure_shape = html_block_structure_shape(*block);
        html.root_tag = "img";
        // Register exactly once after the completed leaf boundary is known.
        register_html_owner_source(block->id, source, {0, raw_size});
        std::vector<SourceRange> markers;
        if (!image->opening.empty()) markers.push_back(image->opening);
        if (image->closing) markers.push_back(*image->closing);
        push_leaf_range(block->id, html_leaf,
            {0, consumed_size}, {0, raw_size}, markers);
        record_completion(block->id, completion_from_source(html.source));
        return block;
    }

    const HtmlCstNode* element = nullptr;
    bool single_element = true;
    for (const auto& node : tree.nodes) {
        if (node.kind == HtmlCstKind::Text) {
            const auto text = source.slice(node.range.start, node.range.end);
            if (text && html_whitespace_only(*text)) continue;
        }
        if (element || (node.kind != HtmlCstKind::Element && node.kind != HtmlCstKind::Comment)) {
            single_element = false;
            break;
        }
        element = &node;
    }
    const bool complete = !tree.has_error && std::ranges::all_of(tree.nodes,
        [](const HtmlCstNode& node) { return node.status == HtmlParseStatus::Complete; });
    const bool unsafe = std::ranges::any_of(tree.nodes,
        [](const auto& node) { return html_contains_unsafe(node); });
    if (!complete || unsafe) {
        BlockNode unsupported;
        unsupported.id = next_node_id();
        unsupported.kind = BlockKind::UnsupportedMarkup;
        unsupported.ensure_atomic_special().raw = cps_to_utf8(raw);
        unsupported.ensure_atomic_special().unsup_reason = unsafe
            ? UnsupportedMarkupReason::UnsafeHtml
            : UnsupportedMarkupReason::MalformedSyntax;
        push_leaf_range(unsupported.id, html_leaf,
            {0, raw_size}, {0, raw_size});
        record_completion(unsupported.id, completion_from_utf8(unsupported.atomic_special().raw));
        diagnostics.push_back(make_diagnostic(DiagnosticSeverity::Warning,
            unsafe ? "Unsafe HTML block is shown as text" : "Malformed HTML block is shown as text",
            std::nullopt, DIAG_UNSAFE_HTML));
        return unsupported;
    }
    if (single_element && element && element->kind == HtmlCstKind::Comment) {
        BlockNode comment;
        comment.id = next_node_id();
        comment.kind = BlockKind::HtmlContainer;
        auto& html = comment.ensure_html_special();
        html.source.assign(source.source());
        html.tree = std::move(tree);
        html.structure_shape = html_block_structure_shape(comment);
        push_leaf_range(comment.id, html_leaf,
            {0, raw_size}, {0, raw_size});
        record_completion(comment.id, completion_from_source(html.source));
        return comment;
    }

    std::vector<HtmlContentSlot> slots;
    std::optional<BlockNode> block;
    std::string root_tag;
    if (single_element && element) {
        root_tag = element->tag_name;
        block = make_html_semantic(*element, source, slots);
    } else {
        BlockNode container;
        container.id = next_node_id();
        container.kind = BlockKind::HtmlContainer;
        container.children = make_html_children(tree.nodes, source, slots);
        push_leaf_range(container.id, html_leaf,
            {0, raw_size}, {0, raw_size});
        block = std::move(container);
    }
    if (!block) return std::nullopt;
    if (block->kind != BlockKind::CodeBlock) {
        auto& html = block->ensure_html_special();
        html.source.assign(source.source());
        html.tree = std::move(tree);
        html.content_slots = std::move(slots);
        html.structure_shape = html_block_structure_shape(*block);
        html.root_tag = std::move(root_tag);
        record_completion(block->id, completion_from_source(html.source));
    } else {
        record_completion(block->id,
            completion_from_source(block->block_source.source()));
    }
    return block;
}

    // ---- table ----
    // Leaf-line table recognition never materializes a whole logical line.
    // Cell text is copied only for editable cell sources or owned syntax gaps.
    std::vector<std::pair<std::size_t, std::size_t>> table_cell_segments(
        parser_source_detail::LeafLineText const& line) const {
        std::vector<std::pair<std::size_t, std::size_t>> segments;
        auto first = std::size_t{0};
        while (first < line.size() && (line.at(first) == U' ' || line.at(first) == U'\t')) ++first;
        std::size_t start = first < line.size() && line.at(first) == U'|' ? first + 1 : 0;
        for (std::size_t index = start; index < line.size(); ++index) {
            if (line.at(index) == U'\\' && index + 1 < line.size()) { ++index; continue; }
            if (line.at(index) != U'|') continue;
            segments.push_back({start, index});
            start = index + 1;
        }
        bool nonspace = false;
        for (auto index = start; index < line.size(); ++index)
            if (line.at(index) != U' ' && line.at(index) != U'\t') { nonspace = true; break; }
        if (start < line.size() && (segments.empty() || nonspace)) segments.push_back({start, line.size()});
        return segments;
    }

    parser_source_detail::LeafSourceAccumulator table_subsource(
        parser_source_detail::LeafLine const& line, std::size_t begin, std::size_t end) {
        parser_source_detail::LeafSourceAccumulator result(physical_source());
        result.append_content_range(line, begin, end);
        return result;
    }

    std::u32string table_text(parser_source_detail::LeafLineText const& line,
            std::size_t begin, std::size_t end) const {
        std::u32string text;
        text.reserve(end - begin);
        for (auto index = begin; index < end; ++index) text.push_back(line.at(index));
        return text;
    }

    bool is_leaf_setext_underline(parser_source_detail::LeafLineText const& line) const {
        std::size_t index = 0;
        while (index < line.size() && line.at(index) == U' ' && index < 4) ++index;
        if (index > 3 || index == line.size() || line.at(index) != U'-') return false;
        const auto marker_start = index;
        while (index < line.size() && line.at(index) == U'-') ++index;
        if (index == marker_start) return false;
        while (index < line.size() && (line.at(index) == U' ' || line.at(index) == U'\t')) ++index;
        return index == line.size();
    }

    std::optional<std::vector<TableAlignment>> parse_table_separator(
        parser_source_detail::LeafLineText const& line) const {
        const auto segments = table_cell_segments(line);
        if (segments.empty()) return std::nullopt;
        std::vector<TableAlignment> alignments;
        for (const auto& [segment_begin, segment_end] : segments) {
            auto begin = segment_begin, end = segment_end;
            while (begin < end && is_table_marker_space_(line.at(begin))) ++begin;
            while (end > begin && is_table_marker_space_(line.at(end - 1))) --end;
            if (begin == end) return std::nullopt;
            const bool leading_colon = line.at(begin) == U':';
            const bool trailing_colon = line.at(end - 1) == U':';
            if (leading_colon) ++begin;
            if (trailing_colon) --end;
            if (begin >= end) return std::nullopt;
            for (auto index = begin; index < end; ++index) if (line.at(index) != U'-') return std::nullopt;
            alignments.push_back(leading_colon && trailing_colon ? TableAlignment::Center
                : leading_colon ? TableAlignment::Left
                : trailing_colon ? TableAlignment::Right : TableAlignment::None);
        }
        return alignments;
    }

    bool leaf_line_starts_gfm_table(parser_source_detail::LeafLine const& header_line) {
        if (!input->dialect.gfm.tables || !input->dialect.tables.gfm_pipe_tables) return false;
        using namespace parser_source_detail;
        const LeafLineText header(physical_source(), header_line);
        // Paragraph preflight can ask about a future line while the parser's
        // unaccepted front remains fixed. Resolve the separator relative to
        // the candidate header rather than assuming it is peek(0).
        const auto* front = peek_leaf_line();
        if (!front || header_line.content_cursor.line < front->content_cursor.line) return false;
        const auto candidate_offset = header_line.content_cursor.line - front->content_cursor.line;
        const auto* separator_line = peek_leaf_line(candidate_offset + 1);
        if (!separator_line) return false;
        const LeafLineText separator(physical_source(), *separator_line);
        bool has_pipe = false;
        for (std::size_t index = 0; index < header.size(); ++index) has_pipe |= header.at(index) == U'|';
        if (!has_pipe && is_leaf_setext_underline(separator)) return false;
        const auto alignments = parse_table_separator(separator);
        return alignments && alignments->size() == table_cell_segments(header).size();
    }



    // Physical-source table interruption preflight uses these overloads,
    // which still examines the root physical line while the frame engine is
    // being migrated. The table leaf itself uses LeafLineText above.
    std::vector<std::pair<std::size_t, std::size_t>> table_cell_segments(
        const std::u32string& line) const {
        std::vector<std::pair<std::size_t, std::size_t>> segments;
        auto first = line.find_first_not_of(U" \t");
        std::size_t start = first != std::u32string::npos && line[first] == U'|' ? first + 1 : 0;
        for (std::size_t index = start; index < line.size(); ++index) {
            if (line[index] == U'\\' && index + 1 < line.size()) { ++index; continue; }
            if (line[index] != U'|') continue;
            segments.push_back({start, index});
            start = index + 1;
        }
        if (start < line.size() && (segments.empty() || line.find_first_not_of(U" \t", start) != std::u32string::npos))
            segments.push_back({start, line.size()});
        return segments;
    }

    std::optional<std::vector<TableAlignment>> parse_table_separator(const std::u32string& line) const {
        const auto segments = table_cell_segments(line);
        if (segments.empty()) return std::nullopt;
        std::vector<TableAlignment> alignments;
        for (const auto& [segment_begin, segment_end] : segments) {
            auto begin = segment_begin, end = segment_end;
            while (begin < end && is_table_marker_space_(line[begin])) ++begin;
            while (end > begin && is_table_marker_space_(line[end - 1])) --end;
            if (begin == end) return std::nullopt;
            const bool leading_colon = line[begin] == U':';
            const bool trailing_colon = line[end - 1] == U':';
            if (leading_colon) ++begin;
            if (trailing_colon) --end;
            if (begin >= end) return std::nullopt;
            for (auto index = begin; index < end; ++index) if (line[index] != U'-') return std::nullopt;
            alignments.push_back(leading_colon && trailing_colon ? TableAlignment::Center
                : leading_colon ? TableAlignment::Left
                : trailing_colon ? TableAlignment::Right : TableAlignment::None);
        }
        return alignments;
    }


    std::optional<BlockNode> try_parse_table(parser_source_detail::LeafLine const& first_line) {
        // Recognize both rows before constructing editable cells. A rejected
        // table must not parse an ordinary paragraph's inline source twice.
        if (!leaf_line_starts_gfm_table(first_line)) return std::nullopt;
        using namespace parser_source_detail;
        const auto* separator_offer = peek_leaf_line(1);
        if (!separator_offer || !leaf_line_fully_matched(first_line)
            || !leaf_line_fully_matched(*separator_offer)) return std::nullopt;
        const LeafLineText header_text(physical_source(), first_line);
        const LeafLineText separator_text(physical_source(), *separator_offer);
        const auto header_segments = table_cell_segments(header_text);
        const auto alignments = parse_table_separator(separator_text);
        if (!alignments || alignments->size() != header_segments.size()) return std::nullopt;

        auto header_row = parse_table_row_leaf(first_line, header_segments.size());
        if (!header_row) return std::nullopt;
        LeafSourceAccumulator table_source(physical_source());
        table_source.append_line(first_line, true);
        std::vector<std::u32string> physical_line_endings;
        physical_line_endings.emplace_back(physical_source().text(first_line.ending));
        accept_leaf_line(first_line);

        const auto* current_separator = peek_leaf_line();
        if (!current_separator) throw std::logic_error("preflighted table separator disappeared");
        const LeafLineText separator_view(physical_source(), *current_separator);
        auto separator_source = table_text(separator_view, 0, separator_view.size());
        table_source.append_line(*current_separator, true);
        physical_line_endings.emplace_back(physical_source().text(current_separator->ending));
        accept_leaf_line(*current_separator);

        BlockVec rows;
        while (const auto* offered = peek_leaf_line()) {
            const LeafLine& row_line = *offered;
            const LeafLineText row_text(physical_source(), row_line);
            bool blank = true;
            for (std::size_t index = 0; index < row_text.size(); ++index)
                if (row_text.at(index) != U' ' && row_text.at(index) != U'\t') { blank = false; break; }
            const auto physical_start = row_line.content_cursor.offset;
            if (blank || !leaf_line_fully_matched(row_line)
                || line_starts_interrupting_block(physical_start, false)) break;
            auto row = parse_table_row_leaf(row_line, header_row->children.size());
            if (!row) break;
            table_source.append_line(row_line, true);
            physical_line_endings.emplace_back(physical_source().text(row_line.ending));
            accept_leaf_line(row_line);
            rows.push_back(std::move(*row));
        }
        NodeId id = next_node_id();
        const auto table_size = table_source.size();
        push_leaf_range(id, table_source, SourceRange{0, table_size}, SourceRange{0, table_size});
        BlockNode b; b.id = id; b.kind = BlockKind::Table;
        header_row->ensure_table_special().table_header_row = true;
        b.children.push_back(std::move(*header_row));
        b.children.insert(b.children.end(), std::make_move_iterator(rows.begin()), std::make_move_iterator(rows.end()));
        auto& table = b.ensure_table_special();
        table.table_aligns = *alignments;
        table.table_separator_source = std::move(separator_source);
        physical_line_endings.resize(b.children.size());
        table.table_internal_line_endings = std::move(physical_line_endings);
        for (const auto& row : b.children) {
            for (const auto& cell : row.children) record_completion(cell.id, completion_from_source(cell.inline_content.source));
            record_completion(row.id, Completion::Other);
        }
        record_completion(b.id, Completion::Other);
        return b;
    }

    std::optional<BlockNode> parse_table_row_leaf(
        parser_source_detail::LeafLine const& line, std::size_t maximum_cells) {
        using namespace parser_source_detail;
        const LeafLineText text(physical_source(), line);
        const auto segments = table_cell_segments(text);
        if (segments.empty()) return std::nullopt;
        BlockVec cells;
        cells.reserve((std::min)(segments.size(), maximum_cells));
        BlockTableSpecial::RowSyntax row_syntax;
        std::vector<std::pair<std::size_t, std::size_t>> bodies;
        for (const auto& segment : segments) {
            if (cells.size() >= maximum_cells) break;
            auto [content_start, content_end] = segment;
            while (content_start < content_end && (text.at(content_start) == U' ' || text.at(content_start) == U'\t')) ++content_start;
            while (content_end > content_start && (text.at(content_end - 1) == U' ' || text.at(content_end - 1) == U'\t')) --content_end;
            if (content_start == segment.second) {
                content_start = segment.first;
                if (content_start < segment.second && (text.at(content_start) == U' ' || text.at(content_start) == U'\t')) ++content_start;
                content_end = content_start;
            }
            auto cell_source = table_subsource(line, content_start, content_end);
            NodeId cell_id = next_node_id();
            BlockNode cell;
            cell.id = cell_id;
            cell.kind = BlockKind::TableCell;
            cell.inline_content.source = cell_source.materialize();
            cell.inline_content.syntax_mode = InlineSyntaxMode::TableCell;
            register_leaf_ownership(cell_id, cell_source);
            push_leaf_range(cell_id, cell_source, SourceRange{0, cell_source.size()}, SourceRange{0, cell_source.size()});
            row_syntax.cell_owners.push_back(cell_id);
            bodies.emplace_back(content_start, content_end);
            cells.push_back(std::move(cell));
        }
        row_syntax.gaps.reserve(bodies.size() + 1);
        std::size_t gap_start = 0;
        for (const auto& [body_start, body_end] : bodies) {
            row_syntax.gaps.push_back(table_text(text, gap_start, body_start));
            gap_start = body_end;
        }
        row_syntax.gaps.push_back(table_text(text, gap_start, text.size()));
        while (cells.size() < maximum_cells) {
            NodeId cell_id = next_node_id();
            BlockNode cell;
            cell.id = cell_id;
            cell.kind = BlockKind::TableCell;
            cell.inline_content.syntax_mode = InlineSyntaxMode::TableCell;
            LeafSourceAccumulator empty_source(physical_source());
            empty_source.append_content_range(line, text.size(), text.size());
            register_leaf_ownership(cell_id, empty_source);
            row_syntax.implicit_cell_owners.push_back(cell_id);
            push_leaf_range(cell_id, empty_source, SourceRange{0, 0}, SourceRange{0, 0});
            cells.push_back(std::move(cell));
        }
        LeafSourceAccumulator row_source(physical_source());
        row_source.append_line(line, true);
        NodeId row_id = next_node_id();
        push_leaf_range(row_id, row_source, SourceRange{0, row_source.size()}, SourceRange{0, text.size()});
        BlockNode row;
        row.id = row_id;
        row.kind = BlockKind::TableRow;
        row.children = std::move(cells);
        row.ensure_table_special().row_syntax = std::move(row_syntax);
        record_completion(row_id, Completion::Other);
        return row;
    }

    static bool is_table_marker_space_(char32_t ch) {
        return ch == U' ' || ch == U'\t' || ch == U'\v' || ch == U'\f' || ch == U'\r';
    }

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

    // ---- list / task ----


    struct ListOpening {
        bool ordered = false, task = false, checked = false;
        std::uint64_t number = 1;
        char32_t delimiter = U'.', bullet = U'-';
        std::size_t indent = 0, content_indent = 0;
        std::size_t marker_end = 0;
        parser_source_detail::PhysicalLineCursor content;
        parser_source_detail::PhysicalSourceRange spelling;
        std::optional<parser_source_detail::PhysicalSourceRange> retained_padding;
        std::size_t retained_padding_column = 0, retained_residual_columns = 0;
    };
    static std::optional<ListOpening> consume_list_opening(
        parser_source_detail::RootPhysicalSource const& source,
        parser_source_detail::PhysicalLineCursor start, bool task_lists) {
        auto cursor = start;
        const auto leading = source.indentation(cursor, 3);
        if (leading.columns > 3) return std::nullopt;
        source.consume_indentation(cursor, leading);
        const auto line_end = source.line(cursor.line).content_end;
        if (cursor.offset >= line_end) return std::nullopt;
        ListOpening marker;
        marker.indent = leading.columns;
        if (source.peek(cursor) == U'-' || source.peek(cursor) == U'*' || source.peek(cursor) == U'+') {
            marker.bullet = source.peek(cursor);
            source.advance(cursor);
        } else if (is_ascii_digit_(source.peek(cursor))) {
            marker.ordered = true; marker.number = 0;
            std::size_t digits = 0;
            while (is_ascii_digit_(source.peek(cursor))) {
                if (++digits > 9) return std::nullopt;
                marker.number = marker.number * 10 + static_cast<std::uint64_t>(source.peek(cursor) - U'0');
                source.advance(cursor);
            }
            if (digits > 9 || cursor.offset >= line_end
                || (source.peek(cursor) != U'.' && source.peek(cursor) != U')')) return std::nullopt;
            marker.delimiter = source.peek(cursor);
            source.advance(cursor);
        } else return std::nullopt;
        marker.marker_end = cursor.offset;
        const auto marker_width = cursor.offset - leading.end;
        const auto marker_column = cursor.physical_column;
        while (source.peek(cursor) == U' ' || source.peek(cursor) == U'\t') source.advance(cursor);
        const auto whitespace_end = cursor.offset;
        const auto content_column = cursor.physical_column;
        const bool has_content = cursor.offset < line_end;
        const auto padding_columns = content_column - marker_column;
        if (has_content && padding_columns == 0) return std::nullopt;
        const bool overpadded = has_content && padding_columns > 4;
        const auto effective_padding = has_content && !overpadded ? padding_columns : std::size_t{1};
        marker.content_indent = marker.indent + marker_width + effective_padding;
        if (overpadded) {
            marker.retained_padding = parser_source_detail::PhysicalSourceRange{marker.marker_end+1, whitespace_end};
            auto padding_cursor = start;
            source.consume_indentation(padding_cursor, leading);
            while (padding_cursor.offset < marker.marker_end+1) source.advance(padding_cursor);
            marker.retained_padding_column = padding_cursor.physical_column;
            const auto child_column = start.physical_column + marker.content_indent;
            marker.retained_residual_columns = padding_cursor.physical_column > child_column
                ? padding_cursor.physical_column - child_column : 0;
        }
        if (task_lists && cursor.offset+2 < line_end) {
            const auto text = source.source();
            const auto at = cursor.offset;
            if (text[at] == U'[' && (text[at+1] == U' ' || text[at+1] == U'\t' || text[at+1] == U'x' || text[at+1] == U'X')
                && text[at+2] == U']' && (at+3 == line_end || text[at+3] == U' ' || text[at+3] == U'\t')) {
                marker.task = true; marker.checked = text[at+1] == U'x' || text[at+1] == U'X';
                source.advance(cursor); source.advance(cursor); source.advance(cursor);
                if (source.peek(cursor) == U' ' || source.peek(cursor) == U'\t') source.advance(cursor);
            }
        }
        // Preserve the established item context: checkbox spelling is owned
        // by the marker but does not increase continuation indentation.
        cursor.physical_column = content_column;
        cursor.residual_columns = 0;
        marker.content = cursor;
        marker.spelling = {start.offset, marker.task ? cursor.offset
            : overpadded ? marker.marker_end+1 : has_content ? whitespace_end : line_end};
        return marker;
    }

    // One physical-line opening step for the container-frame engine. Grammar
    // remains in this Parser; RootPhysicalSource only supplies source/cursors.
    struct QuoteOpening {
        parser_source_detail::PhysicalLineCursor content;
        parser_source_detail::PhysicalSourceRange marker;
    };
    static std::optional<QuoteOpening> consume_quote_opening(
        parser_source_detail::RootPhysicalSource const& source,
        parser_source_detail::PhysicalLineCursor cursor) {
        const auto opening_start = cursor.offset;
        const auto indent = source.indentation(cursor, 3);
        const auto line_end = source.line(cursor.line).content_end;
        if (indent.columns > 3 || indent.end >= line_end || source.source()[indent.end] != U'>')
            return std::nullopt;
        source.consume_indentation(cursor, indent);
        source.advance(cursor); // '>'
        if (source.peek(cursor) == U' ') source.advance(cursor);
        else if (source.peek(cursor) == U'\t') {
            const auto before = cursor.physical_column;
            source.advance(cursor);
            cursor.residual_columns = cursor.physical_column - before - 1;
        }
        return QuoteOpening{cursor, {opening_start, cursor.offset}};
    }

    // Parse-session frames own only structural state and completed direct
    // children. All source cursors refer to the one stable root Parser buffer.
    struct DocumentFrameState {};
    struct QuoteFrameState { bool callout = false; bool last_line_empty = false; };
    struct ListFrameState { ListOpening first; std::size_t sibling_indent_limit = 0; };
    struct ItemFrameState {
        ListOpening opening;
        bool first_line_has_content = false, previous_blank = false;
        std::optional<std::size_t> trailing_blank_start;
    };
    struct FootnoteFrameState { std::u32string label; };
    using ContainerState = std::variant<DocumentFrameState, QuoteFrameState,
        ListFrameState, ItemFrameState, FootnoteFrameState>;
    struct ContainerFrame {
        std::size_t parent = (std::numeric_limits<std::size_t>::max)();
        BlockNode node;
        ContainerState state;
        parser_source_detail::PhysicalSourceRange source;
        parser_source_detail::PhysicalSourceRange content;
        std::vector<parser_source_detail::PendingContainerPrefix> prefixes;
        Completion children_completion = Completion::Empty;
        std::optional<std::size_t> preceding_child_end;
        std::vector<parser_source_detail::PhysicalSourceRange> syntax_markers;
        bool block_dispatch_started=false;
    };
    std::optional<parser_source_detail::RootPhysicalSource> root_physical_source;
    std::vector<ContainerFrame> container_frames;
    std::vector<std::size_t> active_container_frames;
    std::size_t engine_physical_line = 0;
    std::deque<parser_source_detail::LeafLine> leaf_line_cache;
    parser_source_detail::SourceOwnershipIndex physical_ownership;
    parser_source_detail::ContainerPrefixExclusions prefix_exclusions;
    std::unordered_map<std::uint64_t, std::vector<parser_source_detail::PendingContainerPrefix>> pending_container_prefixes;

    parser_source_detail::RootPhysicalSource const& physical_source() {
        if (!root_physical_source) root_physical_source.emplace(cps);
        return *root_physical_source;
    }
    // Peeking stages each prefix with its frame handle. It never mutates the
    // open frames, source ranges, prefix provenance or the global source cursor.
    parser_source_detail::LeafLine offer_container_line(std::size_t physical_line) {
        using namespace parser_source_detail;
        auto const& source=physical_source();
        const auto& line=source.line(physical_line);
        auto context=line_context(line.begin);
        auto cursor=source.begin_line(physical_line,context.physical_column,context.residual_columns,is_lazy_line(line.begin));
        LeafLine offered;
        offered.active_container_frames=active_container_frames.size();
        offered.matched_container_frames=0;
        offered.ending={line.content_end,line.ending_end};
        for (const auto handle : active_container_frames) {
            auto const& frame=container_frames[handle];
            if (std::holds_alternative<DocumentFrameState>(frame.state)
                    || std::holds_alternative<ListFrameState>(frame.state)) {
                ++offered.matched_container_frames;
                continue;
            }
            const auto before=cursor;
            std::optional<PendingContainerPrefix> prefix;
            if (std::holds_alternative<QuoteFrameState>(frame.state)) {
                const auto opening=consume_quote_opening(source,cursor);
                if (!opening) { offered.lazy_candidate=true; break; }
                cursor=opening->content;
                prefix=PendingContainerPrefix{physical_line,{opening->marker},cursor.offset,false,ContainerPrefixRole::Literal};
            } else {
                const bool item=std::holds_alternative<ItemFrameState>(frame.state);
                if (!item) {
                    // Footnote continuation grammar removes one literal tab
                    // or four spaces, independently of inherited tab columns.
                    // Footnote bodies begin a fresh indentation context.
                    auto at=cursor.offset;
                    bool blank=true;
                    for (auto i=at;i<line.content_end;++i)
                        if (source.source()[i]!=U' ' && source.source()[i]!=U'\t') {blank=false;break;}
                    if (blank) at=line.content_end;
                    else if (at<line.content_end && source.source()[at]==U'\t') ++at;
                    else {
                        std::size_t spaces=0;
                        while (at<line.content_end && source.source()[at]==U' ' && spaces<4) {++at;++spaces;}
                        if (spaces<4) {offered.lazy_candidate=false;break;}
                    }
                    cursor.offset=at; cursor.physical_column=0; cursor.residual_columns=0;
                    offered.staged_prefixes.push_back({handle,{
                        physical_line,{{before.offset,at}},at,false,ContainerPrefixRole::Literal}});
                    ++offered.matched_container_frames;
                    continue;
                }
                const auto required=std::get<ItemFrameState>(frame.state).opening.content_indent;
                const auto indent=source.indentation(cursor,required);
                if (indent.columns<required) {
                    auto nonblank=indent.end;
                    while (nonblank<line.content_end && (source.source()[nonblank]==U' ' || source.source()[nonblank]==U'\t')) ++nonblank;
                    if (nonblank<line.content_end) {
                        offered.lazy_candidate=item;
                        // Existing-list siblings have no paragraph-start
                        // restriction on their number or empty content.
                        const auto sibling=consume_list_opening(source,before,input->dialect.gfm.task_lists);
                        if (sibling && sibling->indent<required) offered.lazy_candidate=false;
                        break;
                    }
                }
                source.consume_indentation(cursor,indent);
                // Keep real whitespace after the consumed indentation in the
                // leaf source, including a partial tab's residual context.
                if (item) prefix=PendingContainerPrefix{
                    physical_line,{{before.offset,cursor.offset}},cursor.offset,false,ContainerPrefixRole::Literal};
            }
            if (prefix) offered.staged_prefixes.push_back({handle,std::move(*prefix)});
            ++offered.matched_container_frames;
        }
        if (offered.lazy_candidate) {
            // Staged proposals only: real match count remains unchanged so
            // new-block dispatch closes containers rather than using laziness.
            for (std::size_t i=offered.matched_container_frames;i<active_container_frames.size();++i) {
                auto const& state=container_frames[active_container_frames[i]].state;
                if (std::holds_alternative<FootnoteFrameState>(state)) {
                    offered.lazy_candidate=false;
                    break;
                }
            }
            if (offered.lazy_candidate) {
                const auto lazy_begin=cursor.offset;
                offered.lazy_origin_cursor=cursor;
                source.consume_indentation(cursor,source.indentation(cursor));
                bool first_missing=true;
                for (std::size_t i=offered.matched_container_frames;i<active_container_frames.size();++i) {
                    const auto handle=active_container_frames[i];
                    auto const& state=container_frames[handle].state;
                    if (std::holds_alternative<ListFrameState>(state)
                        || std::holds_alternative<DocumentFrameState>(state)) continue;
                    std::vector<PhysicalSourceRange> spelling;
                    if (first_missing && lazy_begin!=cursor.offset) spelling.push_back({lazy_begin,cursor.offset});
                    first_missing=false;
                    offered.staged_prefixes.push_back({handle,{
                        physical_line,std::move(spelling),cursor.offset,true,ContainerPrefixRole::Literal}});
                }
            }
        }
        offered.content_cursor=cursor;
        offered.content_spans.push_back({cursor.offset,line.content_end});
        return offered;
    }


    void append_completed_block(ContainerFrame& parent,BlockNode block,std::size_t consumed_end) {
        parent.block_dispatch_started=true;
        auto* range=source_range_for(block.id);
        if (!range) throw std::logic_error("completed block has no physical source range");
        if (!parent.node.children.empty() && parent.preceding_child_end) {
            const auto gap=parser_source_detail::PhysicalSourceRange{*parent.preceding_child_end,range->source_range.start};
            std::u32string spelling;
            std::size_t offset=0;
            for (const auto piece : prefix_exclusions.retained(gap)) {
                spelling.append(physical_source().text(piece));
                physical_ownership.add_separator(block.id,piece,offset);
                offset+=piece.size();
            }
            if (std::holds_alternative<ItemFrameState>(parent.state)
                && parent.parent!=(std::numeric_limits<std::size_t>::max)()) {
                auto& list=container_frames[parent.parent];
                if (std::holds_alternative<ListFrameState>(list.state)) {
                    bool whitespace=true;
                    std::size_t endings=0;
                    for (std::size_t i=0;i<spelling.size();++i) {
                        const auto ch=spelling[i];
                        if (ch==U'\r' || ch==U'\n') {
                            ++endings;
                            if (ch==U'\r' && i+1<spelling.size() && spelling[i+1]==U'\n') ++i;
                        } else if (ch!=U' ' && ch!=U'\t') whitespace=false;
                    }
                    const bool prior_owns_ending=completion(parent.node.children.back().id)==Completion::LineEnding;
                    if (whitespace && endings>=(prior_owns_ending ? 1u : 2u))
                        list.node.ensure_list_special().tight=false;
                }
            }
            const bool header_to_first_body=parent.node.kind==BlockKind::Callout
                && parent.node.children.size()==1
                && parent.node.children.front().kind==BlockKind::CalloutTitle;
            // The Callout header already emits this boundary. An explicit empty
            // sibling gap would suppress separators after later title splits.
            if (!header_to_first_body || !spelling.empty())
                block.separator_before=std::move(spelling);
            range->separator_range=PhysicalRange{gap.begin,gap.end};
        }
        const auto separator=block.separator_before ? completion_from_source(*block.separator_before) : Completion::Empty;
        const auto fact=completion(block.id);
        parent.children_completion=append_completion(parent.children_completion,separator,fact);
        auto ending=consumed_end;
        if (ending>range->source_range.start && fact!=Completion::LineEnding && is_line_ending_character(cps[ending-1])) {
            --ending;
            if (ending>range->source_range.start && cps[ending]==U'\n' && cps[ending-1]==U'\r') --ending;
        }
        parent.preceding_child_end=ending;
        parent.node.children.push_back(std::move(block));
    }


    // A completed node enters the existing tree once. Producers supply their
    // own completion fact; this operation contains no formatting-kind switch.
    void complete_container_frame(Completion produced) {
        if (active_container_frames.empty()) throw std::logic_error("no active block frame");
        const auto handle=active_container_frames.back();
        active_container_frames.pop_back();
        auto& frame=container_frames[handle];
        const auto id=frame.node.id;
        record_completion(id,produced);
        if (frame.node.kind!=BlockKind::Document) {
            physical_ownership.add_syntax_range(id,frame.source);
            source_ranges.emplace_back(id,
                PhysicalRange{frame.source.begin,frame.source.end},PhysicalRange{frame.content.begin,frame.content.end});
            for (auto const span : frame.syntax_markers)
                if (!span.empty()) source_ranges.back().marker_ranges.push_back({span.begin,span.end});
            for (auto const& prefix : frame.prefixes) for (const auto span : prefix.spelling)
                if (!span.empty()) source_ranges.back().marker_ranges.push_back({span.begin,span.end});
        }
        if (!frame.prefixes.empty()) pending_container_prefixes.emplace(id.v,std::move(frame.prefixes));
        if (!active_container_frames.empty()) {
            auto& parent=container_frames[active_container_frames.back()];
            append_completed_block(parent,std::move(frame.node),frame.source.end);
        }
    }

    // Item fragments leave their terminal physical ending to the container.
    // Decide before constructing any owner CST; consumed syntax ranges stay full.
    std::size_t completed_leaf_source_end(
        const LeafSourceAccumulator& source, std::size_t requested_end) {
        if (requested_end>source.size()) throw std::out_of_range("leaf source end");
        if (requested_end==0) return requested_end;
        const auto last=source.physical_range(requested_end-1,requested_end);
        if (!last || last->end!=pos) return requested_end;
        auto const& root=physical_source();
        const auto ch=root.source()[last->begin];
        if (!is_line_ending_character(ch)) return requested_end;
        std::size_t ending_length=1;
        if (ch==U'\n' && requested_end>1) {
            const auto prior=source.physical_range(requested_end-2,requested_end-1);
            if (prior && prior->end==last->begin && root.source()[prior->begin]==U'\r') ending_length=2;
        }
        std::optional<std::size_t> item_position;
        for (auto index=active_container_frames.size();index>0;--index) {
            if (std::holds_alternative<ItemFrameState>(container_frames[active_container_frames[index-1]].state)) {
                item_position=index-1;
                break;
            }
        }
        if (!item_position) return requested_end;
        for (std::size_t ahead=0;;++ahead) {
            const auto* next=peek_leaf_line(ahead);
            if (!next || next->matched_container_frames<=*item_position)
                return requested_end-ending_length;
            if (!leaf_line_blank(root,*next)) return requested_end;
        }
    }

    BlockSourceDocument make_leaf_block_source(
        parser_source_detail::CodeMathLeafSource& source, BlockSourceKind kind,
        std::optional<std::size_t> requested_end=std::nullopt) {
        const auto owner_end=completed_leaf_source_end(source.view(),requested_end.value_or(source.size()));
        return parser_source_detail::finish_code_math_source(source,kind,owner_end);
    }

    // Load-time finalization only: block/source discovery and definition
    // registration finish before the sole inline CST parse for each owner.
    void finalize_inline_owners(BlockNode& root) {
        walk_blocks(root,[&](BlockNode& block) {
            auto* document=editable_inline_document(block);
            if (!document) return;
            reparse_inline_document(*document,inline_parse_context());
            if (block.kind==BlockKind::Heading) {
                block.ensure_text_special().slug=generate_slug(
                    cps_to_utf8(trim_cps_(inline_visible_text(*document))),{});
            }
            if (block.kind==BlockKind::Paragraph) promote_image_paragraph(block);
        });
    }

// Staged logical lines share the frame engine's root source and acceptance path.

// Current unconsumed staged line (0), or non-mutating subsequent lookahead.
// nullptr is EOF. Returned addresses survive further peek calls (deque cache).
// Accept/open/close may invalidate cached offers; callers retain needed spans
// or scalar facts before accepting. A peek never allocates NodeIds or records
// ranges, definitions, source ownership, prefix provenance or diagnostics.
const LeafLine* peek_leaf_line(std::size_t lookahead = 0);

// Accept only peek(0): commit each staged prefix once by internal frame handle,
// update active frame physical endpoints, and advance one physical input line.
// The caller must not accept a later lookahead before the current line.
void accept_leaf_line(const LeafLine& line);
// Existing math grammar may stop mid-line. Commit only this logical prefix;
// leave the same-line suffix at peek(0), without consuming the physical EOL.
// Its at_logical_line_start becomes false; line-start-only grammar cannot run.
void accept_leaf_prefix(const LeafLine& line, std::size_t content_end);

bool leaf_line_fully_matched(const LeafLine& line) const {
    return line.active_container_frames == line.matched_container_frames;
}
// Paragraph grammar tests blank, interrupting
// blocks and setext rules; nonparagraph leaves cannot silently accept laziness.
bool leaf_line_lazy_candidate(const LeafLine& line) const { return line.lazy_candidate; }

// Stable root buffer and line table: physical_source() already has its one
// implementation in this Parser. No frame owns or moves that buffer.
// RootPhysicalSource const& physical_source();

// Materialize the one actual leaf's authoritative source, preserving syntax
// mode; this load path intentionally leaves CST construction until definitions
// and the complete block/source tree exist. No early/repeated inline parse.
InlineDocument make_inline_document(const LeafSourceAccumulator& source,
    InlineSyntaxMode mode = InlineSyntaxMode::Markdown);

// Register each accepted source owner's physical spans once. Empty owners must
// carry a physical boundary in their accumulator. No child-to-parent remap.
void register_leaf_ownership(NodeId owner, const LeafSourceAccumulator& source);
// Consumed source ranges can include a terminal EOL which the owner does not
// own (fence/math close). Clip ownership only; the range accumulator is shared.
void register_leaf_ownership(NodeId owner, const LeafSourceAccumulator& source,
    std::size_t owner_source_end);

// Local ranges count characters in this leaf's final authoritative source.
// Begin maps downstream and end upstream across stripped container prefixes;
// root PhysicalRange records are transient recognition data, not selection.
void push_leaf_range(NodeId owner, const LeafSourceAccumulator& source,
    SourceRange local_source, SourceRange local_content,
    std::span<const SourceRange> local_markers = {});

// Existing common APIs remain the only allocation/fact/context hooks:
// NodeId next_node_id();
// void record_completion(NodeId, Completion);
// Completion completion(NodeId) const;
// static Completion completion_from_source(std::u32string_view);
// static Completion append_completion(Completion, Completion, Completion);
// input->dialect / input->revision / diagnostics / global link_definitions.

    // This runs once after all source owners and the final unified tree exist.
    // No frame stores pointers into movable node vectors or parent offset maps.
    void bind_completed_prefixes(BlockNode& root) {
        physical_ownership.seal();
        walk_blocks(root,[&](BlockNode& block) {
            const auto found=pending_container_prefixes.find(block.id.v);
            if (found!=pending_container_prefixes.end())
                block.ensure_container_source()=physical_ownership.bind(physical_source(),found->second);
        });
        pending_container_prefixes.clear();
    }

// Structural transitions for the block frame engine.
// These methods do not accept source lines. They move the current staged offer
// while opening frames; accepting one leaf later commits its prefixes once.
std::size_t push_container_frame(BlockNode node, ContainerState state,
    parser_source_detail::PhysicalSourceRange source,
    parser_source_detail::PhysicalSourceRange content) {
    const auto parent=active_container_frames.empty()
        ? (std::numeric_limits<std::size_t>::max)() : active_container_frames.back();
    const auto handle=container_frames.size();
    if (parent!=(std::numeric_limits<std::size_t>::max)())
        container_frames[parent].block_dispatch_started=true;
    container_frames.push_back({parent,std::move(node),std::move(state),source,content});
    active_container_frames.push_back(handle);
    return handle;
}


// Keep just the accepted current offer, transferring its prefix storage rather
// than copying it at each nested opener. Future offers depended on the previous
// active frame chain and must be discarded when that chain changes.
LeafLine take_current_offer() {
    if (leaf_line_cache.empty()) throw std::logic_error("opening without leaf offer");
    auto current=std::move(leaf_line_cache.front());
    leaf_line_cache.clear();
    return current;
}

void restore_current_offer(LeafLine current) {
    current.active_container_frames=active_container_frames.size();
    current.matched_container_frames=active_container_frames.size();
    current.lazy_candidate=false;
    leaf_line_cache.push_back(std::move(current));
}

void replace_offer_content(LeafLine& line,
    parser_source_detail::PhysicalLineCursor cursor,
    std::optional<parser_source_detail::PhysicalSourceRange> retained={}
) {
    line.content_cursor=cursor;
    line.content_spans.clear();
    if (retained && !retained->empty()) {
        line.content_cursor.offset=retained->begin;
        line.content_spans.push_back(*retained);
    }
    const auto end=physical_source().line(cursor.line).content_end;
    line.content_spans.push_back({cursor.offset,end});
}

bool open_quote_frame() {
    const auto* offered=peek_leaf_line();
    if (!offered || !offered->at_logical_line_start || !leaf_line_fully_matched(*offered)) return false;
    const auto opening=consume_quote_opening(physical_source(),offered->content_cursor);
    if (!opening) return false;
    auto current=take_current_offer();
    BlockNode quote;
    quote.id=next_node_id(); quote.kind=BlockKind::BlockQuote;
    const auto handle=push_container_frame(std::move(quote),QuoteFrameState{},
        {opening->marker.begin,opening->marker.end},
        {opening->content.offset,opening->content.offset});
    current.staged_prefixes.push_back({handle,{
        current.content_cursor.line,{opening->marker},opening->content.offset,false,ContainerPrefixRole::Literal}});
    replace_offer_content(current,opening->content);
    restore_current_offer(std::move(current));
    return true;
}


static bool same_list_kind(ListOpening const& left,ListOpening const& right) {
    return left.ordered==right.ordered
        && (left.ordered ? left.delimiter==right.delimiter : left.bullet==right.bullet);
}

bool open_list_item_frame() {
    const auto* offered=peek_leaf_line();
    if (!offered || !offered->at_logical_line_start || !leaf_line_fully_matched(*offered)) return false;
    const auto opening=consume_list_opening(physical_source(),offered->content_cursor,input->dialect.gfm.task_lists);
    if (!opening) return false;
    auto current=take_current_offer();
    const auto start=current.content_cursor.offset;
    bool reuse=false;
    if (!active_container_frames.empty()) {
        auto& parent=container_frames[active_container_frames.back()];
        if (auto* list=std::get_if<ListFrameState>(&parent.state)) {
            if (!same_list_kind(list->first,*opening)
                || opening->indent>=list->sibling_indent_limit) {
                // The engine must close an incompatible list and retry the
                // unchanged physical line in its parent before dispatching.
                leaf_line_cache.push_back(std::move(current));
                return false;
            }
            reuse=true;
            list->sibling_indent_limit=opening->content_indent;
        }
    }
    if (!reuse) {
        BlockNode list;
        list.id=next_node_id();
        list.kind=opening->task ? BlockKind::TaskList : BlockKind::List;
        auto& properties=list.ensure_list_special();
        properties.ordered=opening->ordered; properties.start=opening->number;
        properties.delimiter=opening->delimiter; properties.tight=true;
        push_container_frame(std::move(list),ListFrameState{*opening,opening->content_indent},
            {start,start},{opening->content.offset,opening->content.offset});
    }
    BlockNode item;
    item.id=next_node_id(); item.kind=opening->task ? BlockKind::TaskListItem : BlockKind::ListItem;
    item.ensure_item_special().checked=opening->checked;
    item.ensure_item_special().marker=std::u32string(physical_source().text(opening->spelling));
    const auto line_end=physical_source().line(current.content_cursor.line).content_end;
    const auto handle=push_container_frame(std::move(item),ItemFrameState{
        *opening,opening->retained_padding.has_value()||opening->content.offset<line_end,false,{}},
        {start,start},{opening->content.offset,opening->content.offset});
    // The opening marker already lives in ItemSpecial::marker. Its record
    // locates that marker without emitting its spelling a second time.
    current.staged_prefixes.push_back({handle,{
        current.content_cursor.line,{},opening->content.offset,false,ContainerPrefixRole::ItemOpening}});
    prefix_exclusions.add(opening->spelling);
    container_frames[handle].syntax_markers.push_back(opening->spelling);
    auto cursor=opening->content;
    if (opening->retained_padding) {
        cursor.physical_column=opening->retained_padding_column;
        cursor.residual_columns=opening->retained_residual_columns;
    }
    replace_offer_content(current,cursor,opening->retained_padding);
    restore_current_offer(std::move(current));
    collapse_empty_item_opening(handle);
    return true;
}


bool open_footnote_frame() {
    const auto* offered=peek_leaf_line();
    if (!offered || !offered->at_logical_line_start || !leaf_line_fully_matched(*offered)) return false;
    const parser_source_detail::LeafLineText text(physical_source(),*offered);
    if (text.size()<4 || text.at(0)!=U'[' || text.at(1)!=U'^') return false;
    std::size_t end=2;
    while (end<text.size() && text.at(end)!=U']') ++end;
    if (end+1>=text.size() || text.at(end+1)!=U':') return false;
    const auto label_end=end;
    end+=2;
    while (end<text.size() && text.at(end)==U' ') ++end;
    const auto physical_end=text.physical(end);
    if (!physical_end) throw std::logic_error("footnote marker boundary");
    std::u32string label;
    for (std::size_t i=2;i<label_end;++i) label.push_back(text.at(i));
    auto current=take_current_offer();
    const auto start=current.content_cursor.offset;
    BlockNode note;
    note.id=next_node_id(); note.kind=BlockKind::FootnoteDefinition;
    note.ensure_container_special().footnote_label=cps_to_utf8(label);
    note.ensure_text_special().opening_marker=std::u32string(physical_source().text({start,*physical_end}));
    const auto handle=push_container_frame(std::move(note),FootnoteFrameState{std::move(label)},
        {start,start},{*physical_end,*physical_end});
    container_frames[handle].syntax_markers.push_back({start,*physical_end});
    prefix_exclusions.add({start,*physical_end});
    auto cursor=current.content_cursor;
    while (cursor.offset<*physical_end) physical_source().advance(cursor);
    cursor.physical_column=0; cursor.residual_columns=0;
    replace_offer_content(current,cursor);
    restore_current_offer(std::move(current));
    return true;
}


void append_empty_frame_body(ContainerFrame& frame,std::size_t physical_point) {
    LeafSourceAccumulator source(physical_source());
    source.append({physical_point,physical_point});
    BlockNode empty;
    empty.id=next_node_id(); empty.kind=BlockKind::Paragraph;
    empty.inline_content=make_inline_document(source);
    push_leaf_range(empty.id,source,{0,0},{0,0});
    register_leaf_ownership(empty.id,source);
    record_completion(empty.id,Completion::Empty);
    append_completed_block(frame,std::move(empty),physical_point);
}


void close_innermost_frame() {
    if (active_container_frames.size()<2) throw std::logic_error("closing root frame");
    const auto handle=active_container_frames.back();
    auto& frame=container_frames[handle];
    const auto* quote_state=std::get_if<QuoteFrameState>(&frame.state);
    const bool title_only=quote_state && quote_state->callout && frame.node.children.size()==1
        && frame.node.children.front().kind==BlockKind::CalloutTitle;
    if ((frame.node.children.empty() || title_only) && !std::holds_alternative<ListFrameState>(frame.state)) {
        append_empty_frame_body(frame,title_only ? frame.source.end : frame.content.begin);
        // This synthetic editing owner contributes no physical body line. The
        // document ending or following sibling owns the header's final ending.
        if (quote_state && quote_state->callout) frame.node.children.back().separator_before=U"";
    }
    auto tail=frame.children_completion;
    if (auto* quote=std::get_if<QuoteFrameState>(&frame.state)) {
        // A quoted blank boundary has a source-editable empty owner unless an
        // unclosed fence actually owns that accepted blank ending.
        if (quote->last_line_empty && !frame.node.children.empty()) {
            auto& last=frame.node.children.back();
            const bool fence_owns_blank=last.kind==BlockKind::CodeBlock
                && last.block_source.tree().kind==BlockSourceKind::FencedCode
                && !last.block_source.tree().complete_closing;
            if (!fence_owns_blank && !(last.kind==BlockKind::Paragraph && last.inline_content.source.empty()))
                append_empty_frame_body(frame,frame.content.end);
        }
        if (quote->callout || (!frame.node.children.empty()
            && frame.node.children.back().kind==BlockKind::Paragraph
            && frame.node.children.back().inline_content.source.empty())) tail=Completion::Other;
    } else if (std::holds_alternative<ListFrameState>(frame.state) && tail==Completion::Empty) {
        tail=Completion::Other;
    } else if (std::holds_alternative<FootnoteFrameState>(frame.state)
        && frame.children_completion==Completion::Empty) tail=Completion::Other;
    if (tail==Completion::LineEnding && !frame.node.children.empty()) {
        const auto* terminal=source_range_for(frame.node.children.back().id);
        if (terminal) {
            for (auto const& prefix : frame.prefixes) {
                if (prefix.role==ContainerPrefixRole::ItemOpening
                    || prefix.content_physical_offset!=terminal->source_range.end) continue;
                if (std::ranges::any_of(prefix.spelling,[](auto range){return !range.empty();})) {
                    // The final source newline is followed by a real stored
                    // container prefix. Its emitted text owns the tail fact.
                    tail=Completion::Other;
                    break;
                }
            }
        }
    }
    leaf_line_cache.clear();
    complete_container_frame(tail);
}


// Called immediately after creating an Item frame and restoring its opening
// offer. An empty marker line directly followed by indented content does not
// introduce an editable blank Paragraph: its EOL + next-line indentation live
// in the existing ItemOpening prefix anchored at the actual first body owner.
void collapse_empty_item_opening(std::size_t item_handle) {
    auto const* first=peek_leaf_line();
    auto const& item=std::get<ItemFrameState>(container_frames[item_handle].state);
    if (!first || item.first_line_has_content) return;
    auto const* next=peek_leaf_line(1);
    if (!next || !leaf_line_fully_matched(*next) || leaf_line_blank(physical_source(),*next)) return;
    const auto ending=first->ending;
    auto& opening=leaf_line_cache.front().staged_prefixes;
    std::erase_if(opening,[&](auto const& record){return record.frame_handle==item_handle;});
    accept_leaf_line(*first);
    auto& continuation=leaf_line_cache.front();
    auto prefix=std::ranges::find(continuation.staged_prefixes,item_handle,
        &parser_source_detail::StagedContainerPrefix::frame_handle);
    if (prefix==continuation.staged_prefixes.end()) throw std::logic_error("collapsed item prefix missing");
    prefix->prefix.role=ContainerPrefixRole::ItemOpening;
    if (!ending.empty()) prefix->prefix.spelling.insert(prefix->prefix.spelling.begin(),ending);
}

// Frontmatter, thematic-break, TOC, and callout header producers.
std::optional<BlockNode> try_parse_frontmatter(LeafLine const& first) {
    if (!first.at_logical_line_start || !leaf_line_fully_matched(first)) return std::nullopt;
    auto const& frame=container_frames[active_container_frames.back()];
    if (frame.block_dispatch_started) return std::nullopt;
    // Reject a non-opener directly from this staged line. Containers may open
    // many frames on one line; only a real leaf candidate may materialize it.
    const parser_source_detail::LeafLineText prefix(physical_source(),first);
    std::size_t at=0;
    while (at<prefix.size() && (prefix.at(at)==U' ' || prefix.at(at)==U'\t'
        || prefix.at(at)==U'\r' || prefix.at(at)==U'\n')) ++at;
    if (prefix.size()-at<3 || prefix.at(at)!=U'-'
        || prefix.at(at+1)!=U'-' || prefix.at(at+2)!=U'-') return std::nullopt;
    const auto opening=trim_utf8(cps_to_utf8(copy_leaf_line_text(physical_source(),first)));
    FrontmatterFormat format=FrontmatterFormat::Yaml;
    if (opening=="---") { if (!input->dialect.frontmatter.yaml) return std::nullopt; }
    else if (opening=="---toml" || opening=="--- toml") {
        if (!input->dialect.frontmatter.toml) return std::nullopt;
        format=FrontmatterFormat::Toml;
    } else if (opening=="---json" || opening=="--- json") {
        if (!input->dialect.frontmatter.json) return std::nullopt;
        format=FrontmatterFormat::Json;
    } else return std::nullopt;
    std::size_t count=1;
    for (;;++count) {
        auto const* next=peek_leaf_line(count);
        if (!next || !leaf_line_fully_matched(*next)) return std::nullopt;
        if (trim_utf8(cps_to_utf8(copy_leaf_line_text(physical_source(),*next)))=="---") break;
    }
    LeafSourceAccumulator raw(physical_source());
    std::size_t opening_end=0,closing_start=0;
    for (std::size_t i=0;i<=count;++i) {
        auto const* line=peek_leaf_line();
        if (i==count) closing_start=raw.size();
        raw.append_line(*line,true);
        if (i==0) opening_end=raw.size();
        accept_leaf_line(*line);
    }
    BlockNode block; block.id=next_node_id(); block.kind=BlockKind::Frontmatter;
    block.ensure_atomic_special().fmt=format;
    const auto source=raw.materialize();
    block.ensure_atomic_special().raw=cps_to_utf8(std::u32string_view(source).substr(opening_end,closing_start-opening_end));
    auto closing_end=source.size();
    if (closing_end && source[closing_end-1]==U'\n') {
        --closing_end;
        if (closing_end && source[closing_end-1]==U'\r') --closing_end;
    } else if (closing_end && source[closing_end-1]==U'\r') --closing_end;
    block.ensure_text_special().opening_marker=source.substr(0,opening_end);
    block.ensure_text_special().closing_marker=source.substr(closing_start,closing_end-closing_start);
    push_leaf_range(block.id,raw,{0,raw.size()},{0,raw.size()});
    record_completion(block.id,Completion::Other);
    return block;
}


std::optional<BlockNode> try_parse_thematic_break(LeafLine const& first) {
    if (!first.at_logical_line_start || !leaf_line_fully_matched(first)) return std::nullopt;
    const auto text=copy_leaf_line_text(physical_source(),first);
    if (!BlockLineRecognizer(text).line_is_thematic_break(0)) return std::nullopt;
    LeafSourceAccumulator raw(physical_source()); raw.append_content(first);
    BlockNode block; block.id=next_node_id(); block.kind=BlockKind::ThematicBreak;
    block.ensure_text_special().opening_marker=text;
    push_leaf_range(block.id,raw,{0,raw.size()},{0,raw.size()});
    accept_leaf_line(first);
    record_completion(block.id,Completion::Other);
    return block;
}


std::optional<BlockNode> try_parse_toc(LeafLine const& first) {
    if (!first.at_logical_line_start || !leaf_line_fully_matched(first)) return std::nullopt;
    const parser_source_detail::LeafLineText text(physical_source(),first);
    auto matches=[&](std::u32string_view marker) {
        if (text.size()<marker.size()) return false;
        for (std::size_t i=0;i<marker.size();++i) if (text.at(i)!=marker[i]) return false;
        return true;
    };
    const auto length=matches(U"[TOC]") ? 5u : matches(U"[[toc]]") ? 7u : 0u;
    if (length==0) return std::nullopt;
    LeafSourceAccumulator raw(physical_source()); raw.append_content_range(first,0,length);
    if (length==text.size()) raw.append_ending(first);
    BlockNode block; block.id=next_node_id(); block.kind=BlockKind::Toc;
    block.ensure_atomic_special().toc_marker=length==5 ? TocMarkerKind::BracketToc : TocMarkerKind::WikiToc;
    push_leaf_range(block.id,raw,{0,raw.size()},{0,raw.size()});
    if (length==text.size()) accept_leaf_line(first); else accept_leaf_prefix(first,length);
    record_completion(block.id,Completion::Other);
    return block;
}


void recognize_callout_header() {
    auto const handle=active_container_frames.back();
    auto& frame=container_frames[handle];
    if (!input->dialect.callouts || !std::holds_alternative<QuoteFrameState>(frame.state)) return;
    auto const* first=peek_leaf_line();
    if (!first) return;
    const parser_source_detail::LeafLineText text(physical_source(),*first);
    std::size_t marker_start=0;
    while (marker_start<text.size() && (text.at(marker_start)==U' ' || text.at(marker_start)==U'\t')) ++marker_start;
    if (marker_start+1>=text.size() || text.at(marker_start)!=U'[' || text.at(marker_start+1)!=U'!') return;
    std::size_t end=marker_start+2;
    std::u32string kind;
    while (end<text.size() && text.at(end)!=U']') kind.push_back(text.at(end++));
    if (end==text.size()) return;
    const auto normalized=normalize_callout_kind(cps_to_utf8(kind));
    if (!normalized) return;
    auto title_start=end+1;
    if (title_start<text.size() && (text.at(title_start)==U' ' || text.at(title_start)==U'\t')) ++title_start;
    const auto title_physical=text.physical(title_start);
    if (!title_physical) throw std::logic_error("callout title source boundary");
    frame.node.kind=BlockKind::Callout;
    frame.node.ensure_container_special().callout_kind=*normalized;
    frame.node.ensure_text_special().opening_marker=std::u32string(physical_source().text({frame.source.begin,*title_physical}));
    frame.syntax_markers.push_back({frame.source.begin,*title_physical});
    std::get<QuoteFrameState>(frame.state).callout=true;
    if (title_start<text.size()) {
        LeafSourceAccumulator title_source(physical_source());
        title_source.append_content_range(*first,title_start,text.size());
        BlockNode title; title.id=next_node_id(); title.kind=BlockKind::CalloutTitle;
        title.inline_content=make_inline_document(title_source);
        register_leaf_ownership(title.id,title_source);
        push_leaf_range(title.id,title_source,{0,title_source.size()},{0,title_source.size()});
        record_completion(title.id,completion_from_source(title.inline_content.source));
        // Header/title serialization is owned by the existing Callout producer;
        // a body separator starts after the header's physical ending.
        frame.node.children.push_back(std::move(title));
    }
    // The callout opening marker already owns this quote's header prefix.
    // Keep any enclosing-container staged prefix records unchanged.
    auto& staged=leaf_line_cache.front().staged_prefixes;
    for (auto const& prefix : staged) if (prefix.frame_handle==handle)
        for (auto const range : prefix.prefix.spelling) prefix_exclusions.add(range);
    std::erase_if(staged,[&](auto const& prefix){return prefix.frame_handle==handle;});
    const auto header_end=first->ending.begin;
    accept_leaf_line(*first);
    // The first body's separator owns the complete physical header ending.
    // Do not split CRLF between the title's inline source and generated syntax.
    frame.preceding_child_end=header_end;
}


// Blank runs are examined before accepting any line. A trailing item/footnote
// run is owned by its parent; an explicit quote prefix keeps the line in that
// quote. No suffix body or ancestor offset map is materialized.
void parse_frame_blank_lines() {
    auto const* first=peek_leaf_line();
    if (!first || !leaf_line_blank(physical_source(),*first)) return;
    std::size_t blanks=0;
    while (auto const* line=peek_leaf_line(blanks)) {
        if (!leaf_line_fully_matched(*line) || !leaf_line_blank(physical_source(),*line)) break;
        ++blanks;
    }
    auto const* following=peek_leaf_line(blanks);
    const auto depth=active_container_frames.size();
    auto& frame=container_frames[active_container_frames.back()];
    const bool body_container=std::holds_alternative<ItemFrameState>(frame.state)
        || std::holds_alternative<FootnoteFrameState>(frame.state);
    const bool unaccepted_opening=body_container && frame.source.end==frame.source.begin;
    // An opening marker is real consumed syntax even when its body is empty.
    // Accept it before considering later blank lines for parent ownership;
    // otherwise closing/re-offering would repeatedly reopen this same line.
    if (unaccepted_opening) blanks=1;
    const auto* item=std::get_if<ItemFrameState>(&frame.state);
    const bool empty_item_body=item && !item->first_line_has_content
        && std::ranges::all_of(frame.node.children,[](auto const& child) {
            return child.kind==BlockKind::Paragraph && child.inline_content.source.empty();
        });
    if (body_container && !unaccepted_opening
        && (empty_item_body || !following || following->matched_container_frames<depth)) {
        close_innermost_frame();
        if (std::holds_alternative<ListFrameState>(container_frames[active_container_frames.back()].state)) {
            // Re-offer in the list's parent-prefix context after closing the
            // item. This makes a sibling's real marker visible to the same
            // list-opening recognizer, without allocating an alternative tree.
            std::size_t count=0;
            while (auto const* line=peek_leaf_line(count)) {
                if (!leaf_line_fully_matched(*line) || !leaf_line_blank(physical_source(),*line)) break;
                ++count;
            }
            auto const* next=peek_leaf_line(count);
            auto const& list=std::get<ListFrameState>(container_frames[active_container_frames.back()].state);
            const auto marker=next && leaf_line_fully_matched(*next)
                ? consume_list_opening(physical_source(),next->content_cursor,input->dialect.gfm.task_lists)
                : std::optional<ListOpening>{};
            const bool thematic=next && BlockLineRecognizer(copy_leaf_line_text(physical_source(),*next)).line_is_thematic_break(0);
            if (!thematic && marker && same_list_kind(list.first,*marker) && marker->indent<list.sibling_indent_limit) {
                container_frames[active_container_frames.back()].node.ensure_list_special().tight=false;
                for (std::size_t i=0;i<count;++i) accept_leaf_line(*peek_leaf_line());
            } else close_innermost_frame();
        }
        return; // Retry the unaccepted current line with its final owner chain.
    }
    frame.block_dispatch_started=true;
    const bool following_content=following && leaf_line_fully_matched(*following);
    const bool skip_separator=following_content && !frame.node.children.empty()
        && frame.preceding_child_end
        && !(frame.node.children.back().kind==BlockKind::Paragraph
            && frame.node.children.back().inline_content.source.empty());
    for (std::size_t i=0;i<blanks;++i) {
        auto const* line=peek_leaf_line();
        if (i==0 && skip_separator) { accept_leaf_line(*line); continue; }
        LeafSourceAccumulator body(physical_source()); body.append_content(*line);
        LeafSourceAccumulator consumed(physical_source()); consumed.append_line(*line,true);
        BlockNode paragraph; paragraph.id=next_node_id(); paragraph.kind=BlockKind::Paragraph;
        paragraph.inline_content=make_inline_document(body);
        register_leaf_ownership(paragraph.id,body);
        push_leaf_range(paragraph.id,consumed,{0,consumed.size()},{0,body.size()});
        const auto fact=completion_from_source(paragraph.inline_content.source);
        record_completion(paragraph.id,fact);
        accept_leaf_line(*line);
        append_completed_block(container_frames[active_container_frames.back()],std::move(paragraph),pos);
    }
}


// Heading and paragraph leaves retain source spans; CST finalization follows
// complete block and reference-definition discovery.

static std::u32string copy_leaf_line_text(
    parser_source_detail::RootPhysicalSource const& source, LeafLine const& line) {
    const LeafLineText view(source, line);
    std::u32string text;
    text.reserve(view.size());
    for (std::size_t i = 0; i < view.size(); ++i) text.push_back(view.at(i));
    return text;
}

static bool leaf_line_blank(
    parser_source_detail::RootPhysicalSource const& source, LeafLine const& line) {
    const LeafLineText text(source, line);
    return std::ranges::all_of(std::views::iota(std::size_t{0}, text.size()),
        [&](std::size_t i) { return text.at(i) == U' ' || text.at(i) == U'\t'; });
}

std::u32string_view leaf_recognition_line(
    LeafLine const& line,
    parser_source_detail::PhysicalLineCursor& cursor,
    std::u32string& owned_text) {
    using namespace parser_source_detail;
    auto const& root = physical_source();
    if (line.lazy_candidate && line.lazy_origin_cursor) {
        cursor = *line.lazy_origin_cursor;
        return root.text({cursor.offset, root.line(cursor.line).content_end});
    }
    cursor = line.content_cursor;
    owned_text = copy_leaf_line_text(root, line);
    return owned_text;
}

struct LeafAtxShape {
    std::uint8_t level = 0;
    std::size_t content_start = 0;
    std::size_t content_end = 0;
};

std::optional<LeafAtxShape> atx_shape(LeafLine const& line) {
    if (!line.at_logical_line_start) return std::nullopt;
    auto const& root=physical_source();
    const auto probe=line.lazy_candidate && line.lazy_origin_cursor
        ? *line.lazy_origin_cursor : line.content_cursor;
    const auto prefix=root.indentation(probe,3);
    if (prefix.columns>3 || prefix.end>=root.line(probe.line).content_end
        || root.source()[prefix.end]!=U'#') return std::nullopt;
    std::u32string owned_text;
    PhysicalLineCursor cursor;
    const auto text = leaf_recognition_line(line, cursor, owned_text);
    const auto indent = scan_block_indentation(text, 0, 3,
        cursor.physical_column, cursor.residual_columns);
    if (indent.columns > 3 || indent.end >= text.size() || text[indent.end] != U'#')
        return std::nullopt;
    auto text_offset = indent.end;
    std::size_t level = 0;
    while (text_offset < text.size() && text[text_offset] == U'#') { ++text_offset; ++level; }
    if (level > 6 || (text_offset < text.size() && text[text_offset] != U' ' && text[text_offset] != U'\t'))
        return std::nullopt;
    while (text_offset < text.size() && (text[text_offset] == U' ' || text[text_offset] == U'\t')) ++text_offset;

    std::size_t trimmed_end = text.size();
    while (trimmed_end > text_offset && (text[trimmed_end - 1] == U' ' || text[trimmed_end - 1] == U'\t'))
        --trimmed_end;
    std::size_t closing_start = trimmed_end;
    while (closing_start > text_offset && text[closing_start - 1] == U'#') --closing_start;
    std::size_t content_end = text.size();
    if (closing_start < trimmed_end
        && (closing_start == text_offset || text[closing_start - 1] == U' ' || text[closing_start - 1] == U'\t')) {
        content_end = closing_start;
        while (content_end > text_offset && (text[content_end - 1] == U' ' || text[content_end - 1] == U'\t'))
            --content_end;
    }
    return LeafAtxShape{static_cast<std::uint8_t>(level), text_offset, content_end};
}

std::optional<BlockNode> try_parse_atx_heading(LeafLine const& first) {
    using namespace parser_source_detail;
    if (!leaf_line_fully_matched(first)) return std::nullopt;
    const auto shape = atx_shape(first);
    if (!shape) return std::nullopt;

    const LeafLineText text(physical_source(), first);
    LeafSourceAccumulator range_source(physical_source());
    LeafSourceAccumulator inline_source(physical_source());
    range_source.append_content(first);
    range_source.append_ending(first);
    inline_source.append_content_range(first, shape->content_start, shape->content_end);

    BlockNode heading;
    heading.id = next_node_id();
    heading.kind = BlockKind::Heading;
    auto& metadata = heading.ensure_text_special();
    metadata.level = shape->level;
    metadata.opening_marker.reserve(shape->content_start);
    for (std::size_t i = 0; i < shape->content_start; ++i) metadata.opening_marker.push_back(text.at(i));
    if (shape->content_end < text.size()) {
        metadata.closing_marker.reserve(text.size() - shape->content_end);
        for (std::size_t i = shape->content_end; i < text.size(); ++i)
            metadata.closing_marker.push_back(text.at(i));
    }
    heading.inline_content = make_inline_document(inline_source);
    const auto& tail = !metadata.closing_marker.empty()
        ? metadata.closing_marker
        : !heading.inline_content.source.empty()
            ? heading.inline_content.source : metadata.opening_marker;

    register_leaf_ownership(heading.id, inline_source);
    std::array<SourceRange, 2> marker_storage{};
    std::size_t marker_count = 0;
    marker_storage[marker_count++] = {0, shape->content_start};
    if (shape->content_end < text.size())
        marker_storage[marker_count++] = {shape->content_end, text.size()};
    push_leaf_range(heading.id, range_source,
        {0, range_source.size()}, {shape->content_start, shape->content_end},
        std::span<const SourceRange>(marker_storage.data(), marker_count));
    accept_leaf_line(first);
    record_completion(heading.id, completion_from_source(tail));
    return heading;
}

std::optional<BlockLineRecognizer::SetextUnderline> leaf_setext_underline(
    LeafLine const& line) {
    if (!line.at_logical_line_start) return std::nullopt;
    std::u32string owned_text;
    PhysicalLineCursor cursor;
    const auto text = leaf_recognition_line(line, cursor, owned_text);
    const auto indentation = scan_block_indentation(text, 0, 3,
        cursor.physical_column, cursor.residual_columns);
    if (indentation.columns > 3
        || (indentation.columns == 3 && indentation.end < text.size()
            && (text[indentation.end] == U' ' || text[indentation.end] == U'\t')))
        return std::nullopt;
    const BlockLineRecognizer recognizer(text);
    return recognizer.setext_underline_at(0);
}

bool leaf_line_interrupts_paragraph(LeafLine const& line) {
    using namespace parser_source_detail;
    if (!line.at_logical_line_start) return false;
    std::u32string owned_text;
    PhysicalLineCursor cursor;
    const auto text = leaf_recognition_line(line, cursor, owned_text);
    const auto indentation = scan_block_indentation(text, 0, 3,
        cursor.physical_column, cursor.residual_columns);
    const auto text_offset = indentation.end;
    if (indentation.columns > 3
        || (indentation.columns == 3 && text_offset < text.size()
            && (text[text_offset] == U' ' || text[text_offset] == U'\t'))) return false;
    const BlockLineRecognizer recognizer(text);
    if (leaf_setext_underline(line).has_value()
        || recognizer.line_is_thematic_break(0)
        || recognizer.line_starts_block_html(text_offset)
        || (text_offset < text.size() && text[text_offset] == U'>')
        || recognizer.line_starts_fenced_code(text_offset)
        || atx_shape(line).has_value()) return true;
    if (input->dialect.math.block_dollar
        && text_offset + 1 < text.size() && text[text_offset] == U'$' && text[text_offset + 1] == U'$') return true;
    if (input->dialect.math.block_bracket
        && text_offset + 1 < text.size() && text[text_offset] == U'\\' && text[text_offset + 1] == U'[') return true;
    if (input->dialect.gfm.tables && leaf_line_starts_gfm_table(line)) return true;
    std::size_t definition_line_count = 0;
    if (link_definition_at(line, definition_line_count)) return true;
    const auto opening = consume_list_opening(physical_source(), cursor,
        input->dialect.gfm.task_lists);
    if (opening && opening->content.offset < physical_source().line(line.content_cursor.line).content_end
        && (!opening->ordered || opening->number == 1)) return true;
    return false;
}

bool leaf_lazy_line_can_continue_paragraph(LeafLine const& line) {
    using namespace parser_source_detail;
    if (!leaf_line_lazy_candidate(line) || !line.at_logical_line_start
        || leaf_line_blank(physical_source(), line)) return false;

    // A missing quote/item prefix only makes a line a candidate. Match the
    // rule that a lazy line must still be paragraph
    // content; this prevents e.g. `> a\n# h` from swallowing an ATX heading.
    // An unprefixed '=' setext-looking line remains literal
    // when it is already in a lazy continuation.
    std::u32string owned_text;
    PhysicalLineCursor cursor;
    const auto text = leaf_recognition_line(line, cursor, owned_text);
    std::size_t first_nonspace = 0;
    while (first_nonspace < text.size()
        && (text[first_nonspace] == U' ' || text[first_nonspace] == U'\t')) ++first_nonspace;
    const auto underline = leaf_setext_underline(line);
    if (underline && first_nonspace < text.size() && text[first_nonspace] == U'=') return true;
    if (leaf_line_interrupts_paragraph(line)) return false;

    // These existing nonparagraph openers are rejected by
    // paragraph content even though they are not all block
    // interruptors in the CommonMark paragraph continuation table.
    const auto indentation = scan_block_indentation(text, 0,
        (std::numeric_limits<std::size_t>::max)(),
        cursor.physical_column, cursor.residual_columns);
    const auto text_offset = indentation.end;
    if (indentation.columns >= 4) return true;
    if (input->dialect.callouts && text_offset + 1 < text.size()
        && text[text_offset] == U'[' && text[text_offset + 1] == U'!') {
        const auto close = text.find(U']', text_offset + 2);
        if (close != std::u32string::npos
            && normalize_callout_kind(cps_to_utf8(std::u32string_view(text).substr(
                text_offset + 2, close - text_offset - 2)))) return false;
    }
    if (input->dialect.footnotes && text_offset + 3 < text.size()
        && text[text_offset] == U'[' && text[text_offset + 1] == U'^') {
        const auto close = text.find(U']', text_offset + 2);
        if (close != std::u32string::npos && close + 1 < text.size()
            && text[close + 1] == U':') return false;
    }
    if ((input->dialect.toc.bracket_toc && text.size() - text_offset >= 5
            && std::u32string_view(text).substr(text_offset, 5) == U"[TOC]")
        || (input->dialect.toc.wiki_toc && text.size() - text_offset >= 7
            && std::u32string_view(text).substr(text_offset, 7) == U"[[toc]]")) return false;
    return true;
}

struct AcceptedParagraphLine {
    LeafLine line;
    std::size_t local_start = 0;
};

std::optional<BlockNode> try_parse_paragraph(LeafLine const& first) {
    using namespace parser_source_detail;
    if (leaf_line_blank(physical_source(), first)) return std::nullopt;
    // Laziness can extend an already-open paragraph, but cannot start a new
    // leaf. The dispatcher reaches this method with an unopened boundary.
    if (!leaf_line_fully_matched(first)) return std::nullopt;

    LeafSourceAccumulator range_source(physical_source());
    LeafSourceAccumulator body_source(physical_source());
    std::vector<AcceptedParagraphLine> accepted;
    std::optional<LeafLine> setext_line;

    for (;;) {
        const auto* current = peek_leaf_line();
        if (!current) break;
        if (!accepted.empty() && !leaf_line_fully_matched(*current)
            && !leaf_line_lazy_candidate(*current)) break;
        if (!accepted.empty() && leaf_line_blank(physical_source(), *current)) break;

        const LeafLineText current_text(physical_source(), *current);
        const auto current_offset = range_source.size();
        accepted.push_back({*current, current_offset});
        range_source.append_content(*current);
        body_source.append_content(*current);
        range_source.append_ending(*current);

        const auto* next = peek_leaf_line(1);
        const bool next_lazy = next && leaf_line_lazy_candidate(*next);
        const bool next_eligible = next && !leaf_line_blank(physical_source(), *next)
            && (leaf_line_fully_matched(*next)
                || leaf_lazy_line_can_continue_paragraph(*next));
        const bool next_is_lazy = next && leaf_line_lazy_candidate(*next);
        const auto underline = next_eligible && !next_is_lazy
            && leaf_line_fully_matched(*next) ? leaf_setext_underline(*next) : std::nullopt;
        if (underline) {
            const LeafLine underline_line = *next;
            range_source.append_content(underline_line);
            range_source.append_ending(underline_line);
            setext_line = underline_line;
            accept_leaf_line(*current);
            const auto* accepted_underline = peek_leaf_line();
            if (!accepted_underline) throw std::logic_error("setext line disappeared");
            accept_leaf_line(*accepted_underline);
            break;
        }

        // A missing container prefix is only a candidate for paragraph
        // laziness. Apply blank, setext, and block-interruption rules before
        // allowing that candidate to continue this already-open paragraph.
        // Matched lines follow the normal interruption table. A lazy line
        // has already passed the paragraph-content preflight above,
        // including the unprefixed '=' continuation exception.
        const bool next_interrupts = next_eligible && !next_lazy
            && leaf_line_interrupts_paragraph(*next);
        const bool continue_paragraph = next_eligible && !next_interrupts;
        if (continue_paragraph) body_source.append_ending(*current);
        else {
            const bool hard_break = current->ending.size() != 0
                && ((current_text.size() >= 2
                    && current_text.at(current_text.size() - 1) == U' '
                    && current_text.at(current_text.size() - 2) == U' ')
                    || (current_text.size() != 0
                        && current_text.at(current_text.size() - 1) == U'\\'));
            if (hard_break) body_source.append_ending(*current);
        }
        accept_leaf_line(*current);
        if (!continue_paragraph) break;
    }

    if (accepted.empty()) return std::nullopt;
    const auto id = next_node_id();
    if (setext_line) {
        auto const& first_line = accepted.front().line;
        auto const& last_line = accepted.back().line;
        const auto first_text = copy_leaf_line_text(physical_source(), first_line);
        const auto last_text = copy_leaf_line_text(physical_source(), last_line);
        const auto leading = scan_block_indentation(first_text, 0, 4,
            first_line.content_cursor.physical_column, first_line.content_cursor.residual_columns);
        const auto leading_chars = leading.columns <= 3 ? leading.end : std::size_t{0};
        auto trimmed_end = last_text.size();
        while (trimmed_end > 0 && (last_text[trimmed_end - 1] == U' ' || last_text[trimmed_end - 1] == U'\t'))
            --trimmed_end;

        LeafSourceAccumulator heading_source(physical_source());
        for (std::size_t i = 0; i < accepted.size(); ++i) {
            const auto& entry = accepted[i];
            const auto text = copy_leaf_line_text(physical_source(), entry.line);
            const auto begin = i == 0 ? leading_chars : 0;
            const auto end = i + 1 == accepted.size() ? trimmed_end : text.size();
            heading_source.append_content_range(entry.line, begin, end);
            if (i + 1 != accepted.size()) heading_source.append_ending(entry.line);
        }
        const auto underline_line = *setext_line;
        const auto underline_text = copy_leaf_line_text(physical_source(), underline_line);

        BlockNode heading;
        heading.id = id;
        heading.kind = BlockKind::Heading;
        auto& metadata = heading.ensure_text_special();
        const auto underline = leaf_setext_underline(underline_line);
        if (!underline) throw std::logic_error("accepted setext marker changed");
        metadata.level = underline->level;
        if (leading_chars != 0) {
            const auto text = copy_leaf_line_text(physical_source(), first_line);
            metadata.opening_marker = text.substr(0, leading_chars);
        }
        std::u32string closing;
        closing.append(last_text.substr(trimmed_end));
        if (!last_line.ending.empty())
            closing.append(physical_source().text(last_line.ending));
        closing.append(underline_text);
        metadata.closing_marker = std::move(closing);
        heading.inline_content = make_inline_document(heading_source);
        const auto& tail = !metadata.closing_marker.empty()
            ? metadata.closing_marker : heading.inline_content.source;

        register_leaf_ownership(id, heading_source);
        const auto source_end = range_source.size();
        const auto body_start = leading_chars;
        const auto body_end = accepted.back().local_start + trimmed_end;
        const auto marker_end = source_end - underline_line.ending.size();
        std::vector<SourceRange> markers;
        if (body_start != 0) markers.push_back({0, body_start});
        markers.push_back({body_end, marker_end});
        push_leaf_range(id, range_source, {0, source_end}, {body_start, body_end}, markers);
        record_completion(id, completion_from_source(tail));
        return heading;
    }

    BlockNode paragraph;
    paragraph.id = id;
    paragraph.kind = BlockKind::Paragraph;
    paragraph.inline_content = make_inline_document(body_source);
    register_leaf_ownership(id, body_source);
    push_leaf_range(id, range_source, {0, range_source.size()}, {0, body_source.size()});
    record_completion(id, completion_from_source(paragraph.inline_content.source));
    return paragraph;
}

// Reference definition recognition over staged logical lines.
// The recognizer uses the existing destination/title grammar once, over staged
// de-prefixed lines. It neither prescans the root source nor accepts lines on
// a failed recognition.

struct LinkDefinitionLeafCursor {
    Parser* parser = nullptr;
    std::size_t line_index = 0;
    std::size_t offset = 0;

    const LeafLine* line() const {
        const auto* offered = parser->peek_leaf_line(line_index);
        return offered && parser->leaf_line_fully_matched(*offered) ? offered : nullptr;
    }
    std::size_t line_size() const {
        const auto* offered = line();
        if (!offered) return 0;
        return parser_source_detail::LeafLineText(parser->physical_source(), *offered).size();
    }
    char32_t peek() const {
        const auto* offered = line();
        if (!offered) return 0;
        parser_source_detail::LeafLineText text(parser->physical_source(), *offered);
        if (offset < text.size()) return text.at(offset);
        return offered->ending.empty() ? 0 : U'\n';
    }
    // Returns the number of de-prefixed source code points consumed. CRLF is
    // one grammar newline but counts as its physical two code points for the
    // existing 999-character label limit.
    std::size_t advance() {
        const auto* offered = line();
        if (!offered) return 0;
        parser_source_detail::LeafLineText text(parser->physical_source(), *offered);
        if (offset < text.size()) { ++offset; return 1; }
        if (offered->ending.empty()) return 0;
        const auto width = offered->ending.size();
        ++line_index;
        offset = 0;
        return width;
    }
    bool eof() const {
        const auto* offered = line();
        if (!offered) return true;
        const auto size = LeafLineText(parser->physical_source(), *offered).size();
        return offset >= size && offered->ending.empty();
    }
};

std::optional<LinkDefinition> link_definition_at(
    const LeafLine& first, std::size_t& accepted_line_count) {
    using namespace parser_source_detail;
    accepted_line_count = 0;
    if (!leaf_line_fully_matched(first)) return std::nullopt;

    LinkDefinitionLeafCursor cursor{this, 0, 0};
    const auto source_start = first.content_cursor.offset;
    std::size_t indentation = 0;
    while (cursor.peek() == U' ' && indentation < 4) {
        cursor.advance();
        ++indentation;
    }
    if (indentation > 3 || cursor.peek() != U'[') return std::nullopt;
    cursor.advance();
    std::u32string raw_label;
    bool escaped = false;
    std::size_t label_length = 0;
    while (!cursor.eof()) {
        const auto ch = cursor.peek();
        if (ch == U'\n') {
            if (label_length >= 999) return std::nullopt;
            label_length += cursor.advance();
            raw_label.push_back(U'\n');
            escaped = false;
            continue;
        }
        if (!escaped && ch == U'[') return std::nullopt;
        if (!escaped && ch == U']') break;
        if (label_length >= 999) return std::nullopt;
        raw_label.push_back(ch);
        label_length += cursor.advance();
        if (!escaped && ch == U'\\') escaped = true;
        else escaped = false;
    }
    if (cursor.eof() || raw_label.empty()) return std::nullopt;
    auto after_label = cursor;
    after_label.advance();
    if (after_label.peek() != U':') return std::nullopt;
    const auto label = normalize_link_label(raw_label);
    if (label.empty() || label_length > 999) return std::nullopt;
    cursor = after_label;
    cursor.advance();

    auto skip_space = [&](bool allow_one_line_break) {
        bool saw_line_break = false;
        bool saw_any = false;
        while (!cursor.eof()) {
            const auto ch = cursor.peek();
            if (ch == U' ' || ch == U'\t') {
                cursor.advance();
                saw_any = true;
            } else if (ch == U'\n') {
                if (!allow_one_line_break || saw_line_break) break;
                saw_line_break = true;
                saw_any = true;
                cursor.advance();
            } else break;
        }
        return saw_any;
    };
    skip_space(true);

    std::u32string href;
    bool angle_destination = false;
    if (cursor.peek() == U'<') {
        angle_destination = true;
        cursor.advance();
        bool destination_escape = false;
        while (!cursor.eof() && cursor.peek() != U'\n') {
            const auto ch = cursor.peek();
            if (!destination_escape && ch == U'>') break;
            if (!destination_escape && ch == U'<') return std::nullopt;
            href.push_back(ch);
            cursor.advance();
            if (!destination_escape && ch == U'\\') destination_escape = true;
            else destination_escape = false;
        }
        if (cursor.peek() != U'>') return std::nullopt;
        cursor.advance();
    } else {
        std::size_t paren_depth = 0;
        bool destination_escape = false;
        while (!cursor.eof() && cursor.peek() != U'\n'
            && cursor.peek() != U' ' && cursor.peek() != U'\t') {
            const auto ch = cursor.peek();
            if (!destination_escape && (ch == U'<' || ch == U'>' || ch == U'"')) return std::nullopt;
            if (!destination_escape && ch == U'(') {
                if (++paren_depth > 32) return std::nullopt;
            } else if (!destination_escape && ch == U')') {
                if (paren_depth == 0) return std::nullopt;
                --paren_depth;
            }
            href.push_back(ch);
            cursor.advance();
            if (!destination_escape && ch == U'\\') destination_escape = true;
            else destination_escape = false;
        }
        if (paren_depth != 0) return std::nullopt;
    }
    if (href.empty() && !angle_destination) return std::nullopt;

    const auto destination_end = cursor;
    const auto had_title_separator = skip_space(true);
    const bool title_on_next_line = cursor.line_index != destination_end.line_index;
    std::optional<std::string> title;
    if (cursor.peek() == U'"' || cursor.peek() == U'\'' || cursor.peek() == U'(') {
        if (!had_title_separator) return std::nullopt;
        const auto opening = cursor.peek();
        cursor.advance();
        const auto closing = opening == U'(' ? U')' : opening;
        std::u32string value;
        bool title_escape = false;
        bool closed = false;
        bool invalid_blank_line = false;
        while (!cursor.eof()) {
            const auto ch = cursor.peek();
            if (!title_escape && ch == closing) { cursor.advance(); closed = true; break; }
            if (!title_escape && opening == U'(' && ch == U'(') return std::nullopt;
            if (ch == U'\n') {
                auto next_line = cursor;
                next_line.advance();
                while (next_line.peek() == U' ' || next_line.peek() == U'\t') next_line.advance();
                if (next_line.peek() == U'\n') {
                    invalid_blank_line = true;
                    break;
                }
                cursor.advance();
                value.push_back(U'\n');
                title_escape = false;
                continue;
            }
            if (!title_escape && ch == U'\\') title_escape = true;
            else title_escape = false;
            value.push_back(ch);
            cursor.advance();
        }
        if (invalid_blank_line) return std::nullopt;
        if (closed) {
            skip_space(false);
            if (cursor.eof() || cursor.peek() == U'\n') {
                title = cps_to_utf8(value);
            } else if (title_on_next_line) {
                cursor = destination_end;
                while (cursor.peek() == U' ' || cursor.peek() == U'\t') cursor.advance();
            } else return std::nullopt;
        } else if (title_on_next_line) {
            cursor = destination_end;
            while (cursor.peek() == U' ' || cursor.peek() == U'\t') cursor.advance();
        } else return std::nullopt;
    } else {
        // A destination cannot be followed by non-whitespace on its last line.
        cursor = destination_end;
        while (cursor.peek() == U' ' || cursor.peek() == U'\t') cursor.advance();
    }
    if (!cursor.eof() && cursor.peek() != U'\n') return std::nullopt;

    // A cursor at EOF immediately after consuming an ending names one line
    // beyond the final accepted physical line. Otherwise its current staged
    // line is the final definition line. This does not accept any line.
    accepted_line_count = cursor.line() ? cursor.line_index + 1 : cursor.line_index;
    const auto end_line = peek_leaf_line(accepted_line_count - 1);
    if (!end_line) { accepted_line_count = 0; return std::nullopt; }
    const auto end = end_line->ending.end;

    LinkDefinition definition;
    definition.label = label;
    definition.href = cps_to_utf8(decode_link_component(href));
    if (title) title = cps_to_utf8(decode_link_component(utf8_to_cps(*title)));
    definition.title = std::move(title);
    definition.source_start = source_start;
    definition.source_end = end;
    return definition;
}

std::optional<BlockNode> try_parse_link_definition(const LeafLine& first) {
    using namespace parser_source_detail;
    std::size_t line_count = 0;
    auto definition = link_definition_at(first, line_count);
    if (!definition) return std::nullopt;

    LeafSourceAccumulator raw(physical_source());
    for (std::size_t index = 0; index < line_count; ++index) {
        const auto* line = peek_leaf_line(0);
        if (!line) throw std::logic_error("accepted link definition line disappeared");
        raw.append_line(*line, true);
        accept_leaf_line(*line);
    }

    const auto source = raw.materialize();
    const auto physical_begin = raw.physical_offset(0).value_or(definition->source_start);
    const auto physical_end = raw.physical_offset(raw.size()).value_or(definition->source_end);
    definition->source_start = physical_begin;
    definition->source_end = physical_end;
    link_definitions.try_emplace(definition->label, *definition);

    BlockNode block;
    block.id = next_node_id();
    block.kind = BlockKind::LinkDefinition;
    block.ensure_atomic_special().raw = cps_to_utf8(source);
    block.ensure_atomic_special().reference_definition=std::make_shared<const LinkDefinitionData>(
        LinkDefinitionData{definition->label,InlineLinkDef{definition->href,definition->title}});
    push_leaf_range(block.id, raw, SourceRange{0, raw.size()}, SourceRange{0, raw.size()});
    record_completion(block.id, completion_from_source(source));
    return block;
}

// Code and math leaves share source ownership and block grammar dispatch.

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


std::optional<BlockNode> try_parse_indented_code_block(const LeafLine& first) {
    using namespace parser_source_detail;
    const auto& root = physical_source();
    auto indentation = [&](const LeafLine& line) {
        auto cursor = line.content_cursor;
        if (!line.content_spans.empty()
            && line.content_spans.front().begin < cursor.offset
            && line.content_spans.front().end == cursor.offset)
            cursor.offset=line.content_spans.front().begin;
        return root.indentation(cursor,4);
    };
    auto logical_offset_at = [&](const LeafLine& line, std::size_t physical) {
        LeafLineText view(root, line);
        for (std::size_t offset = 0; offset <= view.size(); ++offset) {
            const auto mapped = view.physical(offset);
            if (mapped && *mapped == physical) return offset;
        }
        return view.size();
    };
    auto is_blank = [&](const LeafLine& line) {
        LeafLineText view(root, line);
        for (std::size_t i = 0; i < view.size(); ++i)
            if (view.at(i) != U' ' && view.at(i) != U'\t') return false;
        return true;
    };

    if (!leaf_line_fully_matched(first) || indentation(first).columns < 4)
        return std::nullopt;

    CodeMathLeafSource raw(root);
    std::vector<SourceRange> markers;
    std::size_t content_start = 0;
    std::size_t content_end = 0;
    bool first_code_line = true;
    const LeafLine* line = &first;
    while (line) {
        if (!leaf_line_fully_matched(*line)) break;
        const auto indent = indentation(*line);
        const bool blank = is_blank(*line);
        if (indent.columns < 4) {
            if (!blank) break;
            // The existing grammar retains this blank run only when the next
            // logical line is another indented code line. Peek is non-mutating.
            std::size_t blanks = 0;
            const LeafLine* continuation = nullptr;
            while (const auto* candidate = peek_leaf_line(blanks)) {
                if (!leaf_line_fully_matched(*candidate)) break;
                if (!is_blank(*candidate)) {
                    if (indentation(*candidate).columns >= 4) continuation = candidate;
                    break;
                }
                ++blanks;
            }
            if (!continuation) break;
            for (std::size_t index = 0; index < blanks; ++index) {
                // Preflight used lookahead without moving the cursor. Once
                // accepting, always fetch the new current offer at index 0.
                line = peek_leaf_line(0);
                if (!line) break;
                const auto part = raw.append_indented(*line, true);
                markers.push_back({part.source_begin, part.source_end - line->ending.size()});
                accept_leaf_line(*line);
            }
            line = peek_leaf_line(0);
            if (!line) break;
            continue;
        }

        const auto logical_indent_end = logical_offset_at(*line, indent.end);
        const auto part = raw.append_indented(*line, true);
        markers.push_back({part.source_begin, part.source_begin + logical_indent_end});
        if (first_code_line) {
            content_start = part.source_begin + logical_indent_end;
            first_code_line = false;
        }
        content_end = part.source_end - line->ending.size();
        accept_leaf_line(*line);
        line = peek_leaf_line(0);
    }

    const auto id = next_node_id();
    BlockNode block;
    block.id = id;
    block.kind = BlockKind::CodeBlock;
    block.ensure_atomic_special().code_indented = true;
    block.block_source = make_leaf_block_source(raw, BlockSourceKind::IndentedCode);
    trim_indented_code_blank_edges(block.block_source);
    register_leaf_ownership(id, raw.view());
    push_leaf_range(id, raw.view(), {0, raw.size()}, {content_start, content_end}, markers);
    record_completion(id, completion_from_source(block.block_source.source()));
    return block;
}

std::optional<BlockNode> try_parse_code_block(const LeafLine& first) {
    using namespace parser_source_detail;
    const auto& root = physical_source();
    if (!leaf_line_fully_matched(first)) return std::nullopt;
    LeafLineText opening(root, first);
    std::size_t opening_indent = 0;
    while (opening_indent < 4 && opening_indent < opening.size()
        && opening.at(opening_indent) == U' ') ++opening_indent;
    if (opening_indent > 3 || opening_indent >= opening.size()) return std::nullopt;
    const auto marker = opening.at(opening_indent);
    if (marker != U'`' && marker != U'~') return std::nullopt;
    std::size_t fence_count = 0;
    while (opening_indent + fence_count < opening.size()
        && opening.at(opening_indent + fence_count) == marker) ++fence_count;
    if (fence_count < 3) return std::nullopt;
    const auto info_start = opening_indent + fence_count;
    if (marker == U'`') {
        for (auto i = info_start; i < opening.size(); ++i)
            if (opening.at(i) == U'`') return std::nullopt;
    }
    std::u32string info;
    info.reserve(opening.size() - info_start);
    for (auto i = info_start; i < opening.size(); ++i) info.push_back(opening.at(i));
    auto info_utf8 = cps_to_utf8(info);
    std::optional<std::string> language;
    const auto trimmed = trim_utf8(info_utf8);
    if (!trimmed.empty()) language = trimmed;

    CodeMathLeafSource raw(root);
    std::vector<SourceRange> markers;
    const auto opening_part = raw.append(first, true);
    markers.push_back({opening_part.source_begin, opening_part.source_end});
    const auto content_start = raw.size();
    accept_leaf_line(first);

    std::size_t content_end = raw.size();
    std::optional<SourceRange> closing_marker;
    while (const auto* line = peek_leaf_line()) {
        if (!leaf_line_fully_matched(*line)) break;
        LeafLineText view(root, *line);
        std::size_t cursor = 0;
        std::size_t closing_indent = 0;
        while (cursor < view.size() && view.at(cursor) == U' ' && closing_indent < 4) {
            ++cursor;
            ++closing_indent;
        }
        const auto marker_start = cursor;
        while (cursor < view.size() && view.at(cursor) == marker) ++cursor;
        const auto close_count = cursor - marker_start;
        while (cursor < view.size() && (view.at(cursor) == U' ' || view.at(cursor) == U'\t')) ++cursor;
        if (closing_indent <= 3 && close_count >= fence_count && cursor == view.size()) {
            content_end = raw.size();
            const auto close_part = raw.append(*line, false);
            closing_marker = SourceRange{close_part.source_begin, close_part.source_end};
            const auto block_source_end = raw.size();
            raw.append_ending(*line);
            accept_leaf_line(*line);

            const auto id = next_node_id();
            BlockNode block;
            block.id = id;
            const bool fenced_math = language && *language == "math"
                && input->dialect.math.fenced_math;
            if (fenced_math) {
                block.kind = BlockKind::MathBlock;
                block.ensure_atomic_special().math_delim = MathDelimiter::FencedMath;
                block.block_source = make_leaf_block_source(raw, BlockSourceKind::FencedMath, block_source_end);
            } else {
                block.kind = BlockKind::CodeBlock;
                block.block_source = make_leaf_block_source(raw, BlockSourceKind::FencedCode, block_source_end);
                project_fenced_code_indentation(block.block_source, opening_indent);
            }
            if (closing_marker) markers.push_back(*closing_marker);
            register_leaf_ownership(id, raw.view(), block_source_end);
            push_leaf_range(id, raw.view(), {0, raw.size()}, {content_start, content_end}, markers);
            record_completion(id, completion_from_source(block.block_source.source()));
            return block;
        }

        // Keep the established EOF/open-fence behavior: every non-closing
        // physical line and its original ending belongs to the code source.
        const auto body_part = raw.append(*line, true);
        (void)body_part;
        content_end = raw.size();
        accept_leaf_line(*line);
    }

    const auto id = next_node_id();
    BlockNode block;
    block.id = id;
    block.kind = BlockKind::CodeBlock;
    block.block_source = make_leaf_block_source(raw, BlockSourceKind::FencedCode);
    project_fenced_code_indentation(block.block_source, opening_indent);
    register_leaf_ownership(id, raw.view());
    push_leaf_range(id, raw.view(), {0, raw.size()}, {content_start, content_end}, markers);
    record_completion(id, completion_from_source(block.block_source.source()));
    return block;
}

std::optional<BlockNode> try_parse_math_block(const LeafLine& first) {
    using namespace parser_source_detail;
    const auto& root = physical_source();
    if (!leaf_line_fully_matched(first)) return std::nullopt;
    LeafLineText opening(root, first);
    const bool dollar = input->dialect.math.block_dollar && opening.size() >= 2
        && opening.at(0) == U'$' && opening.at(1) == U'$';
    const bool bracket = input->dialect.math.block_bracket && opening.size() >= 2
        && opening.at(0) == U'\\' && opening.at(1) == U'[';
    if (!dollar && !bracket) return std::nullopt;

    CodeMathLeafSource raw(root);
    std::vector<SourceRange> markers;
    const auto open_part = raw.append_content_range(first, 0, 2);
    std::size_t content_start = raw.size();
    if (opening.size() == 2) {
        raw.append_ending(first);
        content_start = raw.size();
        accept_leaf_line(first);
    } else {
        // Continue on the same physical line after the opening delimiter.
        accept_leaf_prefix(first, 2);
    }
    markers.push_back({open_part.source_begin, content_start});

    std::optional<SourceRange> closing_marker;
    std::size_t content_end = raw.size();
    std::size_t block_source_end = raw.size();
    bool closed = false;
    while (const auto* line = peek_leaf_line()) {
        if (!leaf_line_fully_matched(*line)) break;
        LeafLineText view(root, *line);
        std::size_t close = view.size();
        for (std::size_t i = 0; i + 1 < view.size(); ++i) {
            if (dollar ? (view.at(i) == U'$' && view.at(i + 1) == U'$')
                       : (view.at(i) == U'\\' && view.at(i + 1) == U']')) {
                close = i;
                break;
            }
        }
        if (close == view.size()) {
            raw.append(*line, true);
            accept_leaf_line(*line);
            continue;
        }

        const auto close_end = close + 2;
        const auto body_part = raw.append_content_range(*line, 0, close);
        content_end = body_part.source_end;
        const auto marker_part = raw.append_content_range(*line, close, close_end);
        closing_marker = SourceRange{marker_part.source_begin, marker_part.source_end};
        block_source_end = raw.size();
        if (close_end == view.size()) {
            raw.append_ending(*line);
            accept_leaf_line(*line);
        } else {
            // Preserve the old parser's same-line suffix for the next block
            // recognizer. The frame engine retains it at peek(0).
            accept_leaf_prefix(*line, close_end);
        }
        closed = true;
        break;
    }

    if (closed) markers.push_back(*closing_marker);
    else {
        diagnostics.push_back(make_diagnostic(DiagnosticSeverity::Warning,
            dollar ? "Unclosed math block delimiter $$" : "Unclosed math block delimiter \\[",
            std::nullopt, DIAG_UNCLOSED_MATH_DOLLAR));
        content_end = raw.size();
        block_source_end = raw.size();
    }

    const auto id = next_node_id();
    BlockNode block;
    block.id = id;
    block.kind = BlockKind::MathBlock;
    block.ensure_atomic_special().math_delim = dollar
        ? MathDelimiter::BlockDollar : MathDelimiter::BlockBracket;
    block.block_source = make_leaf_block_source(raw,
        dollar ? BlockSourceKind::DollarMath : BlockSourceKind::BracketMath,
        block_source_end);
    register_leaf_ownership(id, raw.view(), block_source_end);
    push_leaf_range(id, raw.view(), {0, raw.size()}, {content_start, content_end}, markers);
    record_completion(id, completion_from_source(block.block_source.source()));
    return block;
}

    // ---- blockquote / callout ----



    // ---- thematic break ----


    // ---- TOC ----



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

// Staged-line acceptance and leaf source ownership.
inline const Parser::LeafLine* Parser::peek_leaf_line(std::size_t lookahead) {
    auto const& root=physical_source();
    while (leaf_line_cache.size()<=lookahead) {
        const auto line=engine_physical_line+leaf_line_cache.size();
        if (line>=root.lines().size()) return nullptr;
        auto const& physical=root.line(line);
        if (physical.begin==root.source().size() && physical.content_end==physical.begin) return nullptr;
        leaf_line_cache.push_back(offer_container_line(line));
    }
    return &leaf_line_cache[lookahead];
}

inline void Parser::accept_leaf_line(LeafLine const& offered) {
    if (leaf_line_cache.empty() || &offered!=&leaf_line_cache.front())
        throw std::logic_error("accept requires the current staged leaf line");
    auto& current=leaf_line_cache.front();
    auto const& root=physical_source();
    auto const physical_line=current.content_cursor.line;
    const auto shared_prefix_anchor=current.content_cursor.offset;
    for (auto& staged : current.staged_prefixes) {
        auto& frame=container_frames[staged.frame_handle];
        if (auto* quote=std::get_if<QuoteFrameState>(&frame.state)) {
            const auto content=staged.prefix.content_physical_offset;
            const auto end=root.line(physical_line).content_end;
            quote->last_line_empty=std::ranges::all_of(root.text({content,end}),[](char32_t ch){return ch==U' '||ch==U'\t';});
        }
        staged.prefix.binding_physical_offset=shared_prefix_anchor;
        for (const auto range : staged.prefix.spelling) prefix_exclusions.add(range);
        if (std::holds_alternative<FootnoteFrameState>(frame.state)) {
            for (const auto range : staged.prefix.spelling) frame.syntax_markers.push_back(range);
        } else frame.prefixes.push_back(std::move(staged.prefix));
    }
    const auto content_end=root.line(physical_line).content_end;
    const auto source_end=current.ending.end;
    for (const auto handle : active_container_frames) {
        auto& frame=container_frames[handle];
        frame.source.end=(std::max)(frame.source.end,source_end);
        frame.content.end=(std::max)(frame.content.end,content_end);
    }
    pos=source_end;
    ++engine_physical_line;
    leaf_line_cache.pop_front(); // Preserve non-mutating future lookahead offers.
}

inline void Parser::accept_leaf_prefix(LeafLine const& offered,std::size_t content_end) {
    using namespace parser_source_detail;
    if (leaf_line_cache.empty() || &offered!=&leaf_line_cache.front())
        throw std::logic_error("partial accept requires current staged leaf line");
    auto& current=leaf_line_cache.front();
    auto const& root=physical_source();
    const auto shared_prefix_anchor=current.content_cursor.offset;
    LeafLineText text(root,current);
    if (content_end==0 || content_end>text.size()) throw std::out_of_range("partial leaf prefix");
    const auto next_physical=text.physical(content_end);
    if (!next_physical) throw std::logic_error("partial leaf prefix has no physical boundary");
    auto cursor=current.content_cursor;
    for (std::size_t index=0;index<content_end;++index) {
        const auto ch=text.at(index);
        cursor.physical_column+=ch==U'\t' ? 4-cursor.physical_column%4 : 1;
    }
    cursor.offset=*next_physical; cursor.residual_columns=0;
    std::vector<PhysicalSourceRange> remaining;
    auto consumed=content_end;
    for (auto span : current.content_spans) {
        if (consumed>=span.size()) { consumed-=span.size(); continue; }
        span.begin+=consumed; consumed=0;
        remaining.push_back(span);
    }
    for (auto& staged : current.staged_prefixes) {
        staged.prefix.binding_physical_offset=shared_prefix_anchor;
        for (const auto range : staged.prefix.spelling) prefix_exclusions.add(range);
        auto& frame=container_frames[staged.frame_handle];
        if (std::holds_alternative<FootnoteFrameState>(frame.state)) {
            for (const auto range : staged.prefix.spelling) frame.syntax_markers.push_back(range);
        } else frame.prefixes.push_back(std::move(staged.prefix));
    }
    current.staged_prefixes.clear();
    current.content_cursor=cursor;
    current.content_spans=std::move(remaining);
    current.at_logical_line_start=false;
    current.active_container_frames=active_container_frames.size();
    current.matched_container_frames=active_container_frames.size();
    current.lazy_candidate=false;
    pos=*next_physical;
    for (const auto handle : active_container_frames) {
        auto& frame=container_frames[handle];
        frame.source.end=(std::max)(frame.source.end,*next_physical);
        frame.content.end=(std::max)(frame.content.end,*next_physical);
    }
}

inline InlineDocument Parser::make_inline_document(
    LeafSourceAccumulator const& source,InlineSyntaxMode mode) {
    InlineDocument document;
    document.source=source.materialize();
    document.source.resize(completed_leaf_source_end(source,source.size()));
    document.syntax_mode=mode;
    return document;
}

inline void Parser::register_leaf_ownership(NodeId owner,LeafSourceAccumulator const& source) {
    register_leaf_ownership(owner,source,completed_leaf_source_end(source,source.size()));
}
inline void Parser::register_leaf_ownership(
    NodeId owner,LeafSourceAccumulator const& source,std::size_t owner_source_end) {
    owner_source_end=completed_leaf_source_end(source,owner_source_end);
    if (owner_source_end==0) {
        const auto point=source.physical_offset(0);
        if (!point) throw std::logic_error("empty source owner missing physical boundary");
        physical_ownership.add_source(owner,{{*point,*point},0});
        return;
    }
    for (auto piece : source.pieces()) {
        if (piece.source_begin>=owner_source_end) break;
        piece.physical.end=piece.physical.begin+(std::min)(piece.physical.size(),owner_source_end-piece.source_begin);
        physical_ownership.add_source(owner,piece);
    }
}

inline void Parser::push_leaf_range(NodeId owner,LeafSourceAccumulator const& source,
    SourceRange local_source,SourceRange local_content,std::span<const SourceRange> local_markers) {
    const auto physical_source_range=source.physical_range(local_source.start,local_source.end);
    const auto physical_content_range=source.physical_range(local_content.start,local_content.end);
    if (!physical_source_range || !physical_content_range) throw std::logic_error("leaf range not in source view");
    ParserSourceRange result(owner,
        {physical_source_range->begin,physical_source_range->end},
        {physical_content_range->begin,physical_content_range->end});
    for (const auto marker : local_markers) {
        const auto mapped=source.physical_range(marker.start,marker.end);
        if (!mapped) throw std::logic_error("leaf marker not in source view");
        result.marker_ranges.push_back({mapped->begin,mapped->end});
    }
    source_ranges.push_back(std::move(result));
    physical_ownership.add_syntax_range(owner,*physical_source_range);
}

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
        doc.metadata=metadata_from_text(p.cps);
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
    MarkdownDialect dialect = default_dialect(),
    std::function<std::optional<InlineLinkDef>(const std::string&)> references_before={},
    std::function<std::optional<InlineLinkDef>(const std::string&)> references_after={}) {
    ParseInput input{0, cps_to_utf8(source), std::move(dialect)};
    input.references_before=std::move(references_before);
    input.references_after=std::move(references_after);
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
