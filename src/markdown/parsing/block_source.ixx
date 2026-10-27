// folia.markdown.block_source — lossless, block-local source documents for raw
// code and display-math blocks. The source is authoritative; the tree is a
// projection used for rendering, syntax metadata, and semantic Enter rules.
export module folia.markdown.block_source;
import std;
import folia.markdown.text_edit;
import folia.markdown.utf;
import folia.markdown.html_entities;

export namespace folia {

enum class BlockSourceKind {
    FencedCode,
    IndentedCode,
    HtmlCode,
    DollarMath,
    BracketMath,
    FencedMath,
};

struct BlockIndentation {
    std::size_t end = 0;
    std::size_t columns = 0;
    std::size_t physical_column = 0;
    std::size_t residual_columns = 0;
};

// Visual column state at a physical line start in a nested parser input.
// source_offset is still a character offset; residual_columns represents
// columns left when a container consumed a tab across its boundary.
struct BlockLineIndentation {
    std::size_t source_offset = 0;
    std::size_t physical_column = 0;
    std::size_t residual_columns = 0;

    bool operator==(const BlockLineIndentation&) const = default;
};

struct GeneratedCodeIndentation {
    std::size_t content_offset = 0;
    std::size_t source_offset = 0;
    std::size_t columns = 0;

    bool operator==(const GeneratedCodeIndentation&) const = default;
};

inline std::vector<BlockLineIndentation> transform_block_line_indentation(
    std::span<const BlockLineIndentation> contexts,
    const TextEdit& edit,
    std::u32string_view old_source) {
    if (!edit.range.valid_for(old_source.size())) {
        throw std::out_of_range("TextEdit range is outside block-line indentation source");
    }
    std::u32string new_source(old_source);
    new_source.replace(edit.range.start, edit.range.length(), edit.replacement);

    BlockLineIndentation inherited{edit.range.start, 0, 0};
    for (const auto& context : contexts) {
        if (context.source_offset > edit.range.start) break;
        inherited = context;
    }
    inherited.source_offset = edit.range.start;

    std::vector<BlockLineIndentation> result;
    result.reserve(contexts.size() + edit.replacement.size() / 16 + 2);
    const auto delta = static_cast<std::ptrdiff_t>(edit.replacement.size())
        - static_cast<std::ptrdiff_t>(edit.range.length());
    auto map_offset = [&](std::size_t offset) {
        if (offset < edit.range.start) return offset;
        if (offset >= edit.range.end) {
            return static_cast<std::size_t>(static_cast<std::ptrdiff_t>(offset) + delta);
        }
        return edit.range.start;
    };
    auto is_line_start = [&](std::size_t offset) {
        return offset == 0 || (offset <= new_source.size()
            && (new_source[offset - 1] == U'\r' || new_source[offset - 1] == U'\n')
            && !(offset < new_source.size() && new_source[offset - 1] == U'\r'
                && new_source[offset] == U'\n'));
    };
    for (auto context : contexts) {
        context.source_offset = map_offset(context.source_offset);
        if (context.source_offset <= new_source.size() && is_line_start(context.source_offset)) {
            result.push_back(context);
        }
    }

    // New physical lines inherit the enclosing-container column state of the
    // edited line. CRLF is one boundary; CR and LF also work independently.
    for (std::size_t cursor = 0; cursor < edit.replacement.size();) {
        if (edit.replacement[cursor] != U'\r' && edit.replacement[cursor] != U'\n') {
            ++cursor;
            continue;
        }
        if (edit.replacement[cursor] == U'\r' && cursor + 1 < edit.replacement.size()
            && edit.replacement[cursor + 1] == U'\n') cursor += 2;
        else ++cursor;
        result.push_back({edit.range.start + cursor, inherited.physical_column, inherited.residual_columns});
    }

    std::ranges::stable_sort(result, {}, &BlockLineIndentation::source_offset);
    std::vector<BlockLineIndentation> unique;
    unique.reserve(result.size());
    for (const auto& context : result) {
        if (!unique.empty() && unique.back().source_offset == context.source_offset) {
            // Prefer a surviving original line context over an inferred one.
            continue;
        }
        unique.push_back(context);
    }
    return unique;
}

// Source offsets remain character offsets; structural indentation uses
// four-column tab stops, including a tab preceded by ordinary spaces.
inline BlockIndentation scan_block_indentation(
    std::u32string_view source, std::size_t start,
    std::size_t maximum_columns = (std::numeric_limits<std::size_t>::max)(),
    std::size_t physical_column = 0,
    std::size_t residual_columns = 0) {
    BlockIndentation result{start, residual_columns, physical_column, residual_columns};
    while (result.end < source.size() && result.columns < maximum_columns) {
        if (source[result.end] == U' ') {
            ++result.columns;
            ++result.physical_column;
        } else if (source[result.end] == U'\t') {
            const auto next = result.physical_column + (4 - result.physical_column % 4);
            result.columns += next - result.physical_column;
            result.physical_column = next;
        }
        else break;
        ++result.end;
    }
    result.residual_columns = result.columns > maximum_columns
        ? result.columns - maximum_columns : 0;
    return result;
}

enum class BlockSourceTokenKind {
    OpeningMarker,
    InfoString,
    Indentation,
    Content,
    LineBreak,
    ClosingMarker,
    Error,
};

struct BlockSourceToken {
    BlockSourceTokenKind kind = BlockSourceTokenKind::Error;
    SourceRange source_range;

    bool operator==(const BlockSourceToken&) const = default;
};

struct BlockSourceTree {
    BlockSourceKind kind = BlockSourceKind::FencedCode;
    std::vector<BlockSourceToken> tokens;
    std::optional<SourceRange> info_range;
    // The first non-whitespace token in a fenced code info string.  The full
    // info_range remains lossless source; this narrower range lets the UI edit
    // the syntax language without rewriting auxiliary info-string text.
    std::optional<SourceRange> language_range;
    std::vector<SourceRange> content_ranges;
    std::vector<GeneratedCodeIndentation> generated_indentation;
    std::vector<std::size_t> content_to_source;
    std::u32string content;
    std::optional<std::string> language;
    bool complete_opening = false;
    bool complete_closing = false;
};

struct BlockSourceStorage {
    std::u32string source;
    BlockSourceTree tree;
    std::vector<BlockLineIndentation> indentation_contexts;
};

struct BlockSourceDocument {
    std::unique_ptr<BlockSourceStorage> storage;

    BlockSourceDocument() = default;
    BlockSourceDocument(std::u32string source, BlockSourceTree tree)
        : storage(std::make_unique<BlockSourceStorage>(std::move(source), std::move(tree))) {}
    BlockSourceDocument(BlockSourceDocument const& other)
        : storage(other.storage ? std::make_unique<BlockSourceStorage>(*other.storage) : nullptr) {}
    BlockSourceDocument& operator=(BlockSourceDocument const& other) {
        if (this != &other) storage = other.storage ? std::make_unique<BlockSourceStorage>(*other.storage) : nullptr;
        return *this;
    }
    BlockSourceDocument(BlockSourceDocument&&) noexcept = default;
    BlockSourceDocument& operator=(BlockSourceDocument&&) noexcept = default;

    std::u32string const& source() const {
        static const std::u32string empty;
        return storage ? storage->source : empty;
    }
    std::u32string& source() {
        return ensure_storage().source;
    }
    BlockSourceTree const& tree() const {
        static const BlockSourceTree empty;
        return storage ? storage->tree : empty;
    }
    BlockSourceTree& tree() {
        return ensure_storage().tree;
    }
    bool empty() const { return !storage; }

private:
    BlockSourceStorage& ensure_storage() {
        if (!storage) storage = std::make_unique<BlockSourceStorage>();
        return *storage;
    }
};

inline std::u32string block_source_line_ending(
    const BlockSourceDocument& document) {
    const auto& source = document.source();
    for (const auto& token : document.tree().tokens) {
        if (token.kind != BlockSourceTokenKind::LineBreak) continue;
        if (token.source_range.end <= source.size()) {
            return source.substr(
                token.source_range.start,
                token.source_range.length());
        }
    }
    return U"\n";
}

namespace block_source_detail {

inline bool horizontal_space(char32_t value) {
    return value == U' ' || value == U'\t';
}

inline std::size_t line_end(std::u32string_view source, std::size_t start) {
    while (start < source.size() && source[start] != U'\r' && source[start] != U'\n') ++start;
    return start;
}

inline std::size_t line_ending_length(std::u32string_view source, std::size_t at) {
    if (at >= source.size()) return 0;
    if (source[at] == U'\r') {
        return at + 1 < source.size() && source[at + 1] == U'\n' ? 2u : 1u;
    }
    return source[at] == U'\n' ? 1u : 0u;
}

inline std::optional<SourceRange> line_ending_before(
    std::u32string_view source,
    std::size_t at) {
    if (at == 0 || at > source.size()) return std::nullopt;
    if (source[at - 1] == U'\n') {
        const auto start = at >= 2 && source[at - 2] == U'\r' ? at - 2 : at - 1;
        return SourceRange{start, at};
    }
    if (source[at - 1] == U'\r') return SourceRange{at - 1, at};
    return std::nullopt;
}

inline void push_token(BlockSourceTree& tree, BlockSourceTokenKind kind, std::size_t start, std::size_t end) {
    if (start < end) tree.tokens.push_back({kind, {start, end}});
}

inline std::u32string trim_horizontal(std::u32string_view value) {
    std::size_t start = 0;
    std::size_t end = value.size();
    while (start < end && horizontal_space(value[start])) ++start;
    while (end > start && horizontal_space(value[end - 1])) --end;
    return std::u32string(value.substr(start, end - start));
}

inline bool ascii_punctuation(char32_t value) {
    return (value >= U'!' && value <= U'/')
        || (value >= U':' && value <= U'@')
        || (value >= U'[' && value <= U'`')
        || (value >= U'{' && value <= U'~');
}

inline std::u32string decode_info_word(std::u32string_view raw) {
    std::u32string decoded;
    for (std::size_t cursor = 0; cursor < raw.size();) {
        if (raw[cursor] == U'&') {
            auto semicolon = cursor + 1;
            while (semicolon < raw.size() && semicolon - cursor <= 35
                && raw[semicolon] != U';' && !horizontal_space(raw[semicolon])) {
                ++semicolon;
            }
            if (semicolon < raw.size() && raw[semicolon] == U';') {
                if (auto value = decode_html_character_reference(raw.substr(cursor, semicolon - cursor + 1))) {
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

inline std::optional<SourceRange> closing_fence(
    std::u32string_view source,
    std::size_t content_start,
    char32_t marker,
    std::size_t minimum) {
    auto line_start = content_start;
    while (line_start <= source.size()) {
        const auto end = line_end(source, line_start);
        auto cursor = line_start;
        std::size_t indent = 0;
        while (cursor < end && source[cursor] == U' ' && indent < 4) {
            ++cursor;
            ++indent;
        }
        if (indent <= 3) {
            const auto marker_start = cursor;
            while (cursor < end && source[cursor] == marker) ++cursor;
            const auto count = cursor - marker_start;
            while (cursor < end && horizontal_space(source[cursor])) ++cursor;
            if (count >= minimum && cursor == end) return SourceRange{line_start, end};
        }
        const auto ending_length = line_ending_length(source, end);
        if (ending_length == 0) break;
        line_start = end + ending_length;
    }
    return std::nullopt;
}

inline void identity_content_map(BlockSourceTree& tree, SourceRange range) {
    tree.content_ranges.clear();
    if (!range.empty()) tree.content_ranges.push_back(range);
    tree.content.clear();
    tree.content_to_source.reserve(range.length() + 1);
    for (auto offset = range.start; offset <= range.end; ++offset) {
        tree.content_to_source.push_back(offset);
    }
}

inline BlockSourceTree parse_fenced(std::u32string_view source, BlockSourceKind kind) {
    BlockSourceTree tree;
    tree.kind = kind;
    const auto first_end = line_end(source, 0);
    auto cursor = std::size_t{0};
    std::size_t indent = 0;
    while (cursor < first_end && source[cursor] == U' ' && indent < 4) {
        ++cursor;
        ++indent;
    }
    const auto marker_start = cursor;
    const auto marker = cursor < first_end ? source[cursor] : U'\0';
    while (cursor < first_end && source[cursor] == marker) ++cursor;
    const auto marker_count = cursor - marker_start;
    const auto valid_marker = indent <= 3
        && (marker == U'`' || marker == U'~')
        && marker_count >= 3;
    if (!valid_marker) {
        push_token(tree, BlockSourceTokenKind::Error, 0, source.size());
        identity_content_map(tree, {0, source.size()});
        tree.content = std::u32string(source);
        return tree;
    }

    const auto opening_line_ending_length = line_ending_length(source, first_end);
    tree.complete_opening = opening_line_ending_length != 0;
    push_token(tree, BlockSourceTokenKind::OpeningMarker, 0, cursor);
    tree.info_range = SourceRange{cursor, first_end};
    push_token(tree, BlockSourceTokenKind::InfoString, cursor, first_end);
    auto language_start = cursor;
    while (language_start < first_end && horizontal_space(source[language_start])) {
        ++language_start;
    }
    auto language_end = language_start;
    while (language_end < first_end && !horizontal_space(source[language_end])) {
        ++language_end;
    }
    if (language_start < language_end) {
        tree.language_range = SourceRange{language_start, language_end};
        tree.language = cps_to_utf8(decode_info_word(source.substr(
            language_start,
            language_end - language_start)));
    }
    auto content_start = first_end;
    if (opening_line_ending_length != 0) {
        push_token(
            tree,
            BlockSourceTokenKind::LineBreak,
            first_end,
            first_end + opening_line_ending_length);
        content_start = first_end + opening_line_ending_length;
    }

    const auto closing = closing_fence(source, content_start, marker, marker_count);
    // For an empty block the opening line ending ends exactly at the closing
    // marker.  It is already represented by the opening LineBreak token and
    // must not also be mistaken for a separator belonging to the content.
    const auto closing_separator = closing && closing->start > content_start
        ? line_ending_before(source, closing->start)
        : std::nullopt;
    const auto content_end = closing_separator
        ? closing_separator->start
        : closing ? closing->start : source.size();
    push_token(tree, BlockSourceTokenKind::Content, content_start, content_end);
    identity_content_map(tree, {content_start, content_end});
    tree.content = std::u32string(source.substr(content_start, content_end - content_start));
    if (closing) {
        tree.complete_closing = true;
        if (closing_separator) {
            push_token(
                tree,
                BlockSourceTokenKind::LineBreak,
                closing_separator->start,
                closing_separator->end);
        }
        push_token(tree, BlockSourceTokenKind::ClosingMarker, closing->start, closing->end);
        if (closing->end < source.size()) {
            push_token(tree, BlockSourceTokenKind::Error, closing->end, source.size());
        }
    }
    return tree;
}

inline BlockSourceTree parse_math(std::u32string_view source, BlockSourceKind kind) {
    BlockSourceTree tree;
    tree.kind = kind;
    const auto opening = kind == BlockSourceKind::DollarMath
        ? std::u32string_view{U"$$"}
        : std::u32string_view{U"\\["};
    const auto closing_text = kind == BlockSourceKind::DollarMath
        ? std::u32string_view{U"$$"}
        : std::u32string_view{U"\\]"};
    auto cursor = std::size_t{0};
    std::size_t indent = 0;
    while (cursor < source.size() && source[cursor] == U' ' && indent < 4) {
        ++cursor;
        ++indent;
    }
    const auto has_marker = indent <= 3
        && source.substr(cursor, opening.size()) == opening;
    if (!has_marker) {
        push_token(tree, BlockSourceTokenKind::Error, 0, source.size());
        identity_content_map(tree, {0, source.size()});
        tree.content = std::u32string(source);
        return tree;
    }
    cursor += opening.size();
    tree.complete_opening = true;
    push_token(tree, BlockSourceTokenKind::OpeningMarker, 0, cursor);
    auto content_start = cursor;
    const auto opening_line_ending_length = line_ending_length(source, cursor);
    if (opening_line_ending_length != 0) {
        push_token(
            tree,
            BlockSourceTokenKind::LineBreak,
            cursor,
            cursor + opening_line_ending_length);
        content_start = cursor + opening_line_ending_length;
    }

    std::optional<SourceRange> closing;
    const auto closing_start = source.find(closing_text, content_start);
    if (closing_start != std::u32string_view::npos) {
        closing = SourceRange{closing_start, closing_start + closing_text.size()};
    }
    const auto closing_separator = closing && closing->start > content_start
        ? line_ending_before(source, closing->start)
        : std::nullopt;
    const auto content_end = closing_separator
        ? closing_separator->start
        : closing ? closing->start : source.size();
    push_token(tree, BlockSourceTokenKind::Content, content_start, content_end);
    identity_content_map(tree, {content_start, content_end});
    tree.content = std::u32string(source.substr(content_start, content_end - content_start));
    if (closing) {
        tree.complete_closing = true;
        if (closing_separator) {
            push_token(
                tree,
                BlockSourceTokenKind::LineBreak,
                closing_separator->start,
                closing_separator->end);
        }
        push_token(tree, BlockSourceTokenKind::ClosingMarker, closing->start, closing->end);
        if (closing->end < source.size()) {
            push_token(tree, BlockSourceTokenKind::Error, closing->end, source.size());
        }
    }
    return tree;
}

inline BlockSourceTree parse_indented(
    std::u32string_view source,
    std::span<const BlockLineIndentation> contexts = {}) {
    BlockSourceTree tree;
    tree.kind = BlockSourceKind::IndentedCode;
    tree.complete_opening = true;
    tree.complete_closing = true;
    auto line_start = std::size_t{0};
    auto content_boundary = std::size_t{0};
    while (line_start < source.size()) {
        const auto end = line_end(source, line_start);
        const auto context = std::ranges::find(contexts, line_start, &BlockLineIndentation::source_offset);
        const auto physical_column = context == contexts.end() ? 0 : context->physical_column;
        const auto residual_columns = context == contexts.end() ? 0 : context->residual_columns;
        const auto indent = scan_block_indentation(source, line_start, 4, physical_column, residual_columns);
        auto content_start = indent.end;
        if (indent.columns >= 4 || content_start == end) {
            push_token(tree, BlockSourceTokenKind::Indentation, line_start, content_start);
        } else {
            content_start = line_start;
        }
        if (content_start < end) {
            if (indent.columns > 4) {
                tree.generated_indentation.push_back({
                    tree.content.size(), content_start, indent.columns - 4});
            }
            tree.content_ranges.push_back({content_start, end});
            push_token(tree, BlockSourceTokenKind::Content, content_start, end);
            for (auto offset = content_start; offset < end; ++offset) {
                tree.content.push_back(source[offset]);
                tree.content_to_source.push_back(offset);
            }
        }
        const auto ending_length = line_ending_length(source, end);
        if (ending_length != 0) {
            push_token(tree, BlockSourceTokenKind::LineBreak, end, end + ending_length);
            if (end + ending_length < source.size()) {
                tree.content.push_back(U'\n');
                tree.content_to_source.push_back(end);
                content_boundary = end + ending_length;
            } else {
                // The last physical ending terminates the indented code line;
                // it is source structure, not an additional empty code line.
                content_boundary = end;
            }
            line_start = end + ending_length;
        } else {
            content_boundary = end;
            line_start = end;
        }
    }
    tree.content_to_source.push_back(content_boundary);
    return tree;
}

inline BlockSourceTree parse_html_code(std::u32string_view source) {
    BlockSourceTree tree;
    tree.kind = BlockSourceKind::HtmlCode;
    auto opening_end = source.find(U'>');
    auto closing_start = source.rfind(U"</pre");
    if (opening_end == std::u32string_view::npos
        || closing_start == std::u32string_view::npos
        || closing_start <= opening_end) {
        push_token(tree, BlockSourceTokenKind::Error, 0, source.size());
        identity_content_map(tree, {0, source.size()});
        tree.content = std::u32string(source);
        return tree;
    }
    ++opening_end;
    auto content_start = opening_end;
    auto content_end = closing_start;
    if (source.substr(content_start, 5) == U"<code") {
        const auto code_open_end = source.find(U'>', content_start);
        const auto code_close = source.rfind(U"</code", content_end);
        if (code_open_end != std::u32string_view::npos
            && code_close != std::u32string_view::npos
            && code_close > code_open_end) {
            content_start = code_open_end + 1;
            content_end = code_close;
        }
    }
    tree.complete_opening = true;
    tree.complete_closing = true;
    push_token(tree, BlockSourceTokenKind::OpeningMarker, 0, content_start);
    push_token(tree, BlockSourceTokenKind::Content, content_start, content_end);
    push_token(tree, BlockSourceTokenKind::ClosingMarker, content_end, source.size());
    if (content_start < content_end) tree.content_ranges.push_back({content_start, content_end});

    for (auto cursor = content_start; cursor < content_end;) {
        if (source[cursor] == U'<') {
            const auto tag_end = source.find(U'>', cursor + 1);
            if (tag_end != std::u32string_view::npos && tag_end < content_end) {
                auto tag = source.substr(cursor + 1, tag_end - cursor - 1);
                if (tag == U"br" || tag == U"br/" || tag == U"/p" || tag == U"/div") {
                    tree.content.push_back(U'\n');
                    tree.content_to_source.push_back(cursor);
                }
                cursor = tag_end + 1;
                continue;
            }
        }
        if (source[cursor] == U'&') {
            const auto semicolon = source.find(U';', cursor + 1);
            if (semicolon != std::u32string_view::npos && semicolon < content_end
                && semicolon - cursor <= 10) {
                const auto entity = source.substr(cursor + 1, semicolon - cursor - 1);
                std::optional<char32_t> decoded;
                if (entity == U"amp") decoded = U'&';
                else if (entity == U"lt") decoded = U'<';
                else if (entity == U"gt") decoded = U'>';
                else if (entity == U"quot") decoded = U'"';
                else if (entity == U"apos" || entity == U"#39") decoded = U'\'';
                else if (entity == U"nbsp") decoded = U' ';
                if (decoded) {
                    tree.content.push_back(*decoded);
                    tree.content_to_source.push_back(cursor);
                    cursor = semicolon + 1;
                    continue;
                }
            }
        }
        tree.content.push_back(source[cursor]);
        tree.content_to_source.push_back(cursor);
        ++cursor;
    }
    tree.content_to_source.push_back(content_end);
    return tree;
}

} // namespace block_source_detail

inline BlockSourceTree parse_block_source(
    std::u32string_view source,
    BlockSourceKind kind,
    std::span<const BlockLineIndentation> contexts = {}) {
    switch (kind) {
        case BlockSourceKind::FencedCode:
        case BlockSourceKind::FencedMath:
            return block_source_detail::parse_fenced(source, kind);
        case BlockSourceKind::IndentedCode:
            return block_source_detail::parse_indented(source, contexts);
        case BlockSourceKind::HtmlCode:
            return block_source_detail::parse_html_code(source);
        case BlockSourceKind::DollarMath:
        case BlockSourceKind::BracketMath:
            return block_source_detail::parse_math(source, kind);
    }
    return {};
}

inline BlockSourceDocument make_block_source(
    std::u32string source,
    BlockSourceKind kind,
    std::vector<BlockLineIndentation> contexts = {}) {
    BlockSourceDocument document;
    document.source() = std::move(source);
    document.storage->indentation_contexts = std::move(contexts);
    document.tree() = parse_block_source(document.source(), kind, document.storage->indentation_contexts);
    return document;
}

inline void reparse_block_source(BlockSourceDocument& document) {
    auto& tree = document.tree();
    tree = parse_block_source(
        document.source(), tree.kind, document.storage->indentation_contexts);
}

inline bool block_source_tokens_partition(const BlockSourceDocument& document) {
    std::size_t cursor = 0;
    for (const auto& token : document.tree().tokens) {
        if (token.source_range.start != cursor
            || !token.source_range.valid_for(document.source().size())) return false;
        cursor = token.source_range.end;
    }
    return cursor == document.source().size();
}

inline std::u32string flatten_block_source_tokens(const BlockSourceDocument& document) {
    std::u32string result;
    for (const auto& token : document.tree().tokens) {
        result.append(
            document.source(),
            token.source_range.start,
            token.source_range.length());
    }
    return result;
}

inline std::u32string block_source_content(const BlockSourceDocument& document) {
    return document.tree().content;
}

// Semantic code text includes visual columns left after a tab-crossing code
// indentation boundary. The authoritative content and source map stay raw.
inline std::u32string block_source_code_projection(const BlockSourceDocument& document) {
    auto result = document.tree().content;
    const auto& generated = document.tree().generated_indentation;
    for (auto iterator = generated.rbegin(); iterator != generated.rend(); ++iterator) {
        result.insert(iterator->content_offset, iterator->columns, U' ');
    }
    return result;
}

inline std::size_t block_source_offset_for_content(
    const BlockSourceDocument& document,
    std::size_t content_offset) {
    if (document.tree().content_to_source.empty()) return 0;
    const auto index = (std::min)(content_offset, document.tree().content_to_source.size() - 1);
    return document.tree().content_to_source[index];
}

// Rendered editing keeps the original source coordinate.  These helpers only
// describe which source boundaries are reachable through the projected block
// content; they do not introduce a second coordinate system.
inline SourceRange block_source_editable_range(
    const BlockSourceDocument& document) {
    const auto& mapping = document.tree().content_to_source;
    if (mapping.empty()) return {0, 0};
    return {mapping.front(), mapping.back()};
}

inline std::size_t block_source_clamp_editable_offset(
    const BlockSourceDocument& document,
    std::size_t source_offset) {
    const auto& mapping = document.tree().content_to_source;
    if (mapping.empty()) return 0;
    if (source_offset <= mapping.front()) return mapping.front();
    if (source_offset >= mapping.back()) return mapping.back();
    const auto next = std::ranges::lower_bound(mapping, source_offset);
    if (next == mapping.end()) return mapping.back();
    if (*next == source_offset || next == mapping.begin()) return *next;
    const auto previous = *std::prev(next);
    return source_offset - previous <= *next - source_offset ? previous : *next;
}

inline std::optional<SourceRange> block_source_backward_delete_range(
    const BlockSourceDocument& document,
    std::size_t source_offset) {
    const auto& mapping = document.tree().content_to_source;
    if (mapping.size() < 2) return std::nullopt;
    const auto offset = block_source_clamp_editable_offset(document, source_offset);
    const auto current = std::ranges::lower_bound(mapping, offset);
    if (current == mapping.end() || *current != offset || current == mapping.begin()) {
        return std::nullopt;
    }
    return SourceRange{*std::prev(current), *current};
}

inline std::optional<SourceRange> block_source_forward_delete_range(
    const BlockSourceDocument& document,
    std::size_t source_offset) {
    const auto& mapping = document.tree().content_to_source;
    if (mapping.size() < 2) return std::nullopt;
    const auto offset = block_source_clamp_editable_offset(document, source_offset);
    const auto current = std::ranges::lower_bound(mapping, offset);
    if (current == mapping.end() || *current != offset || std::next(current) == mapping.end()) {
        return std::nullopt;
    }
    return SourceRange{*current, *std::next(current)};
}

inline std::optional<SourceRange> block_source_trailing_line_break(
    const BlockSourceDocument& document) {
    const auto range = block_source_editable_range(document);
    if (range.empty() || range.end > document.source().size()) return std::nullopt;
    if (range.end >= 2
        && document.source()[range.end - 2] == U'\r'
        && document.source()[range.end - 1] == U'\n') {
        return SourceRange{range.end - 2, range.end};
    }
    if (document.source()[range.end - 1] == U'\r'
        || document.source()[range.end - 1] == U'\n') {
        return SourceRange{range.end - 1, range.end};
    }
    return std::nullopt;
}

} // namespace folia
