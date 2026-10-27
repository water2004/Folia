// folia.markdown.inline_document — per-content-node inline source + lossless CST.
//
// An editable content node (Paragraph, Heading, TableCell, and the
// paragraph/heading bodies inside list items, task items, block quotes,
// callouts, footnotes) owns an `InlineDocument`: the character-exact Markdown
// source of its inline content plus a lossless editable CST over that source.
//
// Editing mutates `source` (via TextEdit) and re-parses the CST of *this node
// only*; the tree is structural/analysis state, never a second coordinate
// system. Saving emits `source` verbatim.
export module folia.markdown.inline_document;
import std;
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.inline_cst;
import folia.markdown.text_edit;
import folia.markdown.utf;
import folia.markdown.html_entities;

export namespace folia {

// Context the lossless inline parser needs: dialect, an id allocator seed,
// and (optionally) a link-definition resolver for reference links. This is a
// concrete type so InlineDocument can hold/forward it without a circular import.
struct InlineLinkDef {
    std::string href;
    std::optional<std::string> title;
    bool operator==(const InlineLinkDef&) const=default;
};

enum class InlineSyntaxMode : std::uint8_t {
    Markdown,
    TableCell,
    HtmlText,
};

inline bool inline_syntax_allows_markdown(InlineSyntaxMode mode) {
    return mode != InlineSyntaxMode::HtmlText;
}

struct InlineParseContext {
    MarkdownDialect dialect{};
    InlineSyntaxMode syntax_mode = InlineSyntaxMode::Markdown;
    NodeId next_id{1};
    std::function<NodeId()> allocate_id;
    std::function<std::optional<InlineLinkDef>(const std::string&)> resolve_link_label;
};

struct InlineDocument {
    std::u32string source;
    InlineCstTree tree;
    InlineSyntaxMode syntax_mode = InlineSyntaxMode::Markdown;

    std::u32string serialize() const { return source; }
};

enum class InlineBlockWhitespacePolicy : std::uint8_t {
    None,
    Paragraph,
    Heading,
};

// Return only source ranges whose outermost raw whitespace is omitted by the
// block's CommonMark semantic projection. The ranges remain in source and CST;
// callers use them as source-mapped structural markers rather than deleting
// or normalizing editable text.
inline std::vector<SourceRange> inline_block_semantic_whitespace_ranges(
    const InlineDocument& document,
    InlineBlockWhitespacePolicy policy) {
    std::vector<SourceRange> ranges;
    if (document.syntax_mode != InlineSyntaxMode::Markdown || document.source.empty()
        || document.tree.nodes.empty()) return ranges;

    // A parser-owned editable blank physical line may contain its original
    // spaces or tabs as source.  They have no CommonMark paragraph content,
    // but remain in the CST and are exposed through one hidden editable
    // marker spanning the exact source range.
    if (policy == InlineBlockWhitespacePolicy::Paragraph
        && std::ranges::all_of(document.source, [](char32_t ch) {
            return ch == U' ' || ch == U'\t';
        })) {
        ranges.push_back({0, document.source.size()});
        return ranges;
    }

    const auto is_raw_text_leaf = [](const InlineCstNode& node) {
        return node.children.empty()
            && (node.kind == InlineCstKind::Text || node.kind == InlineCstKind::Whitespace);
    };
    if (policy == InlineBlockWhitespacePolicy::Paragraph) {
        auto offset = std::size_t{0};
        auto spaces = std::size_t{0};
        for (const auto& node : document.tree.nodes) {
            if (!is_raw_text_leaf(node) || node.range.start != offset) break;
            const auto leaf_start = offset;
            while (offset < node.range.end && document.source[offset] == U' ' && spaces < 4) {
                ++offset;
                ++spaces;
            }
            if (offset > leaf_start) ranges.push_back({leaf_start, offset});
            if (spaces == 4) {
                ranges.clear();
                return ranges;
            }
            if (offset < node.range.end || spaces == 0) break;
        }
        // A four-space run begins an indented code block at a paragraph
        // boundary, so don't hide any prefix if it appears in an editable
        // paragraph node produced by a surrounding/container parser.
        if (spaces == 0) ranges.clear();
        return ranges;
    }
    if (policy == InlineBlockWhitespacePolicy::Heading) {
        auto offset = document.source.size();
        std::vector<SourceRange> reversed_ranges;
        for (auto node = document.tree.nodes.rbegin(); node != document.tree.nodes.rend(); ++node) {
            if (!is_raw_text_leaf(*node) || node->range.end != offset) break;
            const auto leaf_end = offset;
            while (offset > node->range.start
                && (document.source[offset - 1] == U' ' || document.source[offset - 1] == U'\t')) {
                --offset;
            }
            if (offset < leaf_end) reversed_ranges.push_back({offset, leaf_end});
            if (offset > node->range.start) break;
            offset = node->range.start;
        }
        ranges.assign(reversed_ranges.rbegin(), reversed_ranges.rend());
    }
    return ranges;
}

// ---- queries over a CST (used by renderer / hit-test / format-state) ----

// Find the deepest node whose range covers `offset`. Returns nullptr if none.
// Descends into container children so the returned node is the most specific
// covering node (leaf or innermost container).
inline const InlineCstNode* node_at_offset(const InlineCstTree& tree, std::size_t offset) {
    const InlineCstNode* deepest = nullptr;
    std::function<const InlineCstNode*(const std::vector<InlineCstNode>&)> search =
        [&](const std::vector<InlineCstNode>& nodes) -> const InlineCstNode* {
        for (const auto& node : nodes) {
            if (!node.range.covers(offset)) continue;
            deepest = &node;
            search(node.children);
            return deepest;
        }
        return nullptr;
    };
    search(tree.nodes);
    return deepest;
}

// Is the offset inside a marker (opening or closing) of a delimited node?
inline bool offset_in_marker(const InlineCstNode& node, std::size_t offset) {
    const auto& delim = node.delimiter_ranges();
    if (delim.opening.contains(offset)) return true;
    if (delim.closing && delim.closing->contains(offset)) return true;
    return false;
}

// Does the node's content range cover the offset (half-open; the boundary at
// content.start counts as "in content", content.end counts as just past)?
inline bool offset_in_content(const InlineCstNode& node, std::size_t offset) {
    const auto& content = node.delimiter_ranges().content;
    return offset >= content.start && offset <= content.end;
}

inline std::u32string inline_source_slice(const InlineDocument& document, SourceRange range) {
    if (!range.valid_for(document.source.size())) return {};
    return document.source.substr(range.start, range.length());
}

// Replace source NUL only in a derived string that will be displayed. The
// source and its offsets remain unchanged, and each scalar keeps one position.
inline bool replace_inline_display_nuls(std::u32string& display_text) {
    if (std::ranges::find(display_text, U'\0') == display_text.end()) return false;
    std::ranges::replace(display_text, U'\0', U'\uFFFD');
    return true;
}

inline std::u32string decode_inline_entity(std::u32string_view raw) {
    if (auto decoded = decode_html_character_reference(raw)) return std::move(*decoded);
    return std::u32string(raw);
}

// CommonMark's code-span text projection: line endings become spaces, then
// one outer ASCII space is removed only when both ends are spaces and the
// result contains a non-space. The source and its CST range remain untouched.
inline std::u32string normalize_inline_code_span(
    std::u32string_view raw,
    InlineSyntaxMode syntax_mode = InlineSyntaxMode::Markdown) {
    std::u32string normalized;
    normalized.reserve(raw.size());
    for (std::size_t index = 0; index < raw.size(); ++index) {
        const auto ch = raw[index];
        if (ch == U'\r') {
            if (index + 1 < raw.size() && raw[index + 1] == U'\n') ++index;
            normalized.push_back(U' ');
        } else if (ch == U'\n') {
            normalized.push_back(U' ');
        } else {
            normalized.push_back(ch);
        }
    }
    if (syntax_mode == InlineSyntaxMode::TableCell) {
        std::u32string table_text;
        table_text.reserve(normalized.size());
        for (std::size_t index = 0; index < normalized.size(); ++index) {
            if (normalized[index] == U'\\' && index + 1 < normalized.size()
                && normalized[index + 1] == U'|') ++index;
            table_text.push_back(normalized[index]);
        }
        normalized = std::move(table_text);
    }
    if (normalized.size() >= 2 && normalized.front() == U' ' && normalized.back() == U' '
        && std::ranges::any_of(normalized, [](char32_t ch) { return ch != U' '; })) {
        normalized.erase(normalized.begin());
        normalized.pop_back();
    }
    return normalized;
}

inline void append_inline_visible_text(
    const InlineDocument& document,
    const InlineCstNodes& nodes,
    std::u32string& output) {
    for (const auto& node : nodes) {
        using K = InlineCstKind;
        switch (node.kind) {
            case K::Emphasis:
            case K::Strong:
            case K::Strikethrough:
            case K::Link:
                append_inline_visible_text(document, node.children, output);
                break;
            case K::HtmlElement:
                if (node.semantics().html_tag == "img") {
                    output += utf8_to_cps(node.semantics().alt);
                } else if (node.semantics().html_tag == "br") {
                    output.push_back(U'\n');
                } else {
                    append_inline_visible_text(document, node.children, output);
                }
                break;
            case K::HtmlComment:
                break;
            case K::Image:
                output += utf8_to_cps(node.semantics().alt);
                break;
            case K::WikiLink:
                output += utf8_to_cps(node.semantics().alias.value_or(node.semantics().target));
                break;
            case K::FootnoteRef:
                output += U"[^" + utf8_to_cps(node.semantics().label) + U"]";
                break;
            case K::CodeSpan:
                output += normalize_inline_code_span(
                    inline_source_slice(document, node.delimiter_ranges().content), document.syntax_mode);
                break;
            case K::InlineMath:
            case K::Autolink:
                output += inline_source_slice(document, node.delimiter_ranges().content);
                break;
            case K::Escape: {
                const auto raw = inline_source_slice(document, node.range);
                if (!raw.empty()) output.push_back(raw.back());
                break;
            }
            case K::Entity:
                output += decode_inline_entity(inline_source_slice(document, node.range));
                break;
            case K::SoftBreak:
            case K::HardBreak:
                output.push_back(U'\n');
                break;
            default:
                output += inline_source_slice(document, node.range);
                break;
        }
    }
}

inline std::u32string inline_visible_text(const InlineDocument& document) {
    std::u32string output;
    append_inline_visible_text(document, document.tree.nodes, output);
    replace_inline_display_nuls(output);
    return output;
}

inline bool inline_contains_kind(const InlineCstNodes& nodes, InlineCstKind kind) {
    for (const auto& node : nodes) {
        if (node.kind == kind || inline_contains_kind(node.children, kind)) return true;
    }
    return false;
}

inline bool inline_contains_kind(const InlineDocument& document, InlineCstKind kind) {
    return inline_contains_kind(document.tree.nodes, kind);
}

} // namespace folia
