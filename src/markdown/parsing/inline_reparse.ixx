// Incremental inline analysis uses the existing lexer and grammar. A lexical
// island is reusable only when both its grammar and its surrounding boundaries
// are unchanged; otherwise the caller expands analysis to the owner scope.
export module folia.markdown.inline_reparse;
import std;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.inline_cst;
import folia.markdown.text_edit;
import folia.markdown.instrumentation;

export namespace folia {

struct InlineReparseWindow {
    SourceRange before;
    SourceRange after;
    std::vector<std::size_t> path;
    std::size_t first_token = 0;
    std::size_t last_token = 0;
    std::ptrdiff_t delta = 0;
    std::vector<InlineToken> tokens;
};

namespace inline_reparse_detail {
inline bool literal_token(InlineToken token) {
    return token.kind() == TokenKind::Text || token.kind() == TokenKind::Whitespace;
}

// These CST wrappers derive presentation from their kind, not from their
// descendant text. Links, images, HTML and extensions can derive attributes or
// semantics from that text, so their dependency scope includes the wrapper.
inline bool independent_descendants(InlineCstNode const& node) {
    using K = InlineCstKind;
    return node.status == ParseStatus::Complete && node.delimited
        && node.delimiter_ranges().closing
        && (node.kind == K::Emphasis || node.kind == K::Strong || node.kind == K::Strikethrough);
}

inline std::vector<std::size_t> literal_path(InlineDocument const& document, SourceRange range) {
    std::vector<std::size_t> path;
    auto nodes = &document.tree.nodes;
    while (!nodes->empty()) {
        auto found = std::ranges::upper_bound(*nodes, range.start, {},
            [](auto const& node) { return node.range.end; });
        if (found == nodes->end()) {
            if (range.start != document.source.size()) return {};
            found = nodes->end() - 1;
        }
        if (found->status != ParseStatus::Complete || range.start < found->range.start
            || range.end > found->range.end) return {};
        path.push_back(static_cast<std::size_t>(found - nodes->begin()));
        if (found->kind == InlineCstKind::Text && found->children.empty()) return path;
        if (!independent_descendants(*found)) return {};
        nodes = &found->children;
    }
    return {};
}

inline std::size_t shifted(std::size_t offset, std::ptrdiff_t delta) {
    return static_cast<std::size_t>(static_cast<std::ptrdiff_t>(offset) + delta);
}

inline void shift_subtree(InlineCstNode& root, std::size_t boundary, std::ptrdiff_t delta,
    std::vector<InlineCstNode*>& pending) {
    if (!delta) return;
    pending.push_back(&root);
    auto shift = [&](SourceRange& range) {
        if (range.start < boundary) return; // Unset optional delimiter ranges stay unset.
        range = {shifted(range.start, delta), shifted(range.end, delta)};
    };
    while (!pending.empty()) {
        auto& node = *pending.back(); pending.pop_back();
        shift(node.range);
        if (node.delimited) {
            node.delimited = std::make_shared<DelimitedRanges>(*node.delimited);
            auto& ranges = *node.delimited;
            shift(ranges.full); shift(ranges.opening); shift(ranges.content);
            if (ranges.closing) shift(*ranges.closing);
        }
        for (auto& child : node.children) pending.push_back(&child);
    }
}
}

// A read-only plan. Token classification, bare autolinks, entities and dialect
// behavior all come from the canonical parser, rather than an editor scanner.
inline std::optional<InlineReparseWindow> plan_inline_reparse(
    InlineDocument const& document, TextEdit const& edit, InlineParseContext const& context) {
    if (!edit.range.valid_for(document.source.size()) || document.tree.nodes.empty()) return {};
    auto path = inline_reparse_detail::literal_path(document, edit.range);
    if (path.empty()) return {};
    auto nodes = &document.tree.nodes;
    InlineCstNode const* leaf = nullptr;
    for (auto index : path) { leaf = &(*nodes)[index]; nodes = &leaf->children; }
    auto const& literal = *leaf;
    // Preserve literal characters adjacent to other grammar nodes. Their
    // unchanged flanking classes guarantee that a remote delimiter cannot
    // change interpretation because of this edit.
    if ((edit.range.start == literal.range.start && literal.range.start != 0)
        || (edit.range.end == literal.range.end && literal.range.end != document.source.size())) return {};

    auto const& tokens = document.tree.tokens;
    auto first = std::ranges::upper_bound(tokens, literal.range.start, {}, &InlineToken::end);
    auto last = std::ranges::lower_bound(tokens, literal.range.end, {}, &InlineToken::end);
    if (first == tokens.end() || last == tokens.end() || last->end() != literal.range.end) return {};
    ++last;
    auto first_start = first == tokens.begin() ? 0 : (first - 1)->end();
    if (first_start != literal.range.start || !std::all_of(first, last, inline_reparse_detail::literal_token)) return {};

    auto begin = std::upper_bound(first, last, edit.range.start,
        [](std::size_t offset, InlineToken token) { return offset < token.end(); });
    if (begin == last) --begin;
    if (begin != first) --begin;
    auto end = std::lower_bound(begin, last, edit.range.end,
        [](InlineToken token, std::size_t offset) { return token.end() < offset; });
    if (end != last) ++end;
    if (end != last) ++end;
    auto window_start = begin == tokens.begin() ? 0 : (begin - 1)->end();
    auto window_end = (end - 1)->end();
    auto text = document.source.substr(window_start, window_end - window_start);
    text.replace(edit.range.start - window_start, edit.range.length(), edit.replacement);
    if (text.empty()) return {};
    auto local = context;
    local.syntax_mode = document.syntax_mode;
    local.allocate_id = {};
    local.report_progress = {};
    inline_parser_detail::Parser parser{text, local};
    auto parsed = parser.parse_all();
    record_inline_scanned_characters(text.size());
    if (parsed.nodes.size() != 1 || parsed.nodes.front().kind != InlineCstKind::Text
        || parsed.nodes.front().status != ParseStatus::Complete
        || !std::ranges::all_of(parsed.tokens, inline_reparse_detail::literal_token)) return {};
    InlineReparseWindow result{
        {window_start, window_end}, {window_start, window_start + text.size()},
        std::move(path), static_cast<std::size_t>(begin - tokens.begin()),
        static_cast<std::size_t>(end - tokens.begin()), text_edit_delta(edit), std::move(parsed.tokens)};
    for (auto& token : result.tokens) token = {token.kind(), window_start + token.end()};
    return result;
}

// Source has already been edited by the editor. Reuse all structural identities;
// only the token window, ancestor bounds and subsequent source ranges change.
inline void apply_inline_reparse(InlineDocument& document, InlineReparseWindow window) {
    auto& tree = document.tree;
    auto nodes = &tree.nodes;
    std::vector<InlineCstNode*> pending;
    for (auto index : window.path) {
        auto& node = (*nodes)[index];
        auto boundary = node.range.end;
        node.range.end = inline_reparse_detail::shifted(boundary, window.delta);
        for (auto sibling = index + 1; sibling < nodes->size(); ++sibling)
            inline_reparse_detail::shift_subtree((*nodes)[sibling], boundary, window.delta, pending);
        if (node.delimited && window.delta) {
            // The ancestor's opening delimiter is unchanged. The content and
            // full ranges grow, and its closing delimiter moves with the edit.
            node.delimited = std::make_shared<DelimitedRanges>(*node.delimited);
            auto& ranges = *node.delimited;
            ranges.full.end = inline_reparse_detail::shifted(ranges.full.end, window.delta);
            ranges.content.end = inline_reparse_detail::shifted(ranges.content.end, window.delta);
            if (ranges.closing) {
                ranges.closing->start = inline_reparse_detail::shifted(ranges.closing->start, window.delta);
                ranges.closing->end = inline_reparse_detail::shifted(ranges.closing->end, window.delta);
            }
        }
        nodes = &node.children;
    }
    auto& tokens = tree.tokens;
    auto insert = tokens.erase(tokens.begin() + window.first_token, tokens.begin() + window.last_token);
    tokens.insert(insert, window.tokens.begin(), window.tokens.end());
    if (window.delta) {
        for (auto index = window.first_token + window.tokens.size(); index < tokens.size(); ++index)
            tokens[index] = {tokens[index].kind(), inline_reparse_detail::shifted(tokens[index].end(), window.delta)};
    }
    record_inline_reparse();
}

}
