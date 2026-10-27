// folia.core.inline_source_edit — the sole mutation path for inline content.
export module folia.core.inline_source_edit;
import std;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.text_edit;

export namespace folia {

namespace inline_source_edit_detail {

inline std::vector<NodeId> node_identities(const InlineCstNodes& roots) {
    std::vector<NodeId> result;
    std::vector<const InlineCstNode*> pending;
    for (auto i=roots.size(); i>0; --i) pending.push_back(&roots[i-1]);
    while (!pending.empty()) {
        const auto* node=pending.back(); pending.pop_back();
        result.push_back(node->id);
        for (auto i=node->children.size(); i>0; --i) pending.push_back(&node->children[i-1]);
    }
    return result;
}

inline bool restore_identities(InlineCstNodes& roots, std::span<const NodeId> identities) {
    std::vector<InlineCstNode*> nodes;
    std::vector<InlineCstNode*> pending;
    for (auto i=roots.size(); i>0; --i) pending.push_back(&roots[i-1]);
    while (!pending.empty()) {
        auto* node=pending.back(); pending.pop_back();
        nodes.push_back(node);
        for (auto i=node->children.size(); i>0; --i) pending.push_back(&node->children[i-1]);
    }
    if (nodes.size()!=identities.size()) return false;
    for (std::size_t i=0; i<nodes.size(); ++i) nodes[i]->id=identities[i];
    return true;
}

inline std::optional<SourceRange> adjacent_hard_break(
    const InlineCstNodes& nodes,
    std::size_t offset,
    bool backward) {
    for (const auto& node : nodes) {
        if (auto nested = adjacent_hard_break(node.children, offset, backward)) return nested;
        const auto semantic_break = node.kind == InlineCstKind::HardBreak
            || (node.kind == InlineCstKind::HtmlElement
                && node.semantics().html_tag == "br");
        if (!semantic_break) continue;
        const bool adjacent = backward
            ? node.range.start < offset && offset <= node.range.end
            : node.range.start <= offset && offset < node.range.end;
        if (adjacent) return node.range;
    }
    return std::nullopt;
}

inline std::optional<SourceRange> adjacent_explicit_break_lexeme(
    std::u32string_view source,
    std::size_t offset,
    bool backward) {
    constexpr std::u32string_view marker = U"<br>";
    const auto first = offset > marker.size() ? offset - marker.size() : 0u;
    const auto last = (std::min)(offset, source.size() >= marker.size()
        ? source.size() - marker.size() : 0u);
    for (auto start = first; start <= last && source.size() >= marker.size(); ++start) {
        const auto end = start + marker.size();
        const bool adjacent = backward
            ? start < offset && offset <= end
            : start <= offset && offset < end;
        if (adjacent && source.substr(start, marker.size()) == marker) {
            return SourceRange{start, end};
        }
    }
    return std::nullopt;
}

inline std::optional<SourceRange> translated_untouched_range(
    SourceRange range,
    const TextEdit& edit) {
    if (range.end <= edit.range.start) return range;
    if (range.start >= edit.range.end) {
        const auto delta = text_edit_delta(edit);
        const auto start = static_cast<std::ptrdiff_t>(range.start) + delta;
        const auto end = static_cast<std::ptrdiff_t>(range.end) + delta;
        if (start < 0 || end < start) return std::nullopt;
        return SourceRange{static_cast<std::size_t>(start), static_cast<std::size_t>(end)};
    }
    return std::nullopt;
}

inline bool same_source_slice(
    std::u32string_view before,
    SourceRange before_range,
    std::u32string_view after,
    SourceRange after_range) {
    return before_range.valid_for(before.size())
        && after_range.valid_for(after.size())
        && before.substr(before_range.start, before_range.length())
            == after.substr(after_range.start, after_range.length());
}

inline void reconcile_nodes(
    const InlineCstNodes& before_nodes,
    InlineCstNodes& after_nodes,
    std::u32string_view before_source,
    std::u32string_view after_source,
    const TextEdit& edit) {
    std::vector<bool> claimed(after_nodes.size(), false);
    for (const auto& before : before_nodes) {
        const auto expected = translated_untouched_range(before.range, edit);
        if (!expected) continue;
        for (std::size_t index = 0; index < after_nodes.size(); ++index) {
            auto& after = after_nodes[index];
            if (claimed[index] || after.kind != before.kind || after.status != before.status
                || after.range != *expected
                || !same_source_slice(before_source, before.range, after_source, after.range)) {
                continue;
            }
            claimed[index] = true;
            after.id = before.id;
            reconcile_nodes(before.children, after.children, before_source, after_source, edit);
            break;
        }
    }
}

} // namespace inline_source_edit_detail

// Complete paired formatting markers are semantic editing units. Incomplete
// input remains ordinary source; recognition always comes from the CST.
inline const InlineCstNode* inline_format_marker_at(
    const InlineCstNodes& nodes, std::size_t character) {
    for (const auto& node : nodes) {
        if (character < node.range.start || character >= node.range.end) continue;
        if (auto child = inline_format_marker_at(node.children, character)) return child;
        switch (node.kind) {
            case InlineCstKind::Emphasis: case InlineCstKind::Strong:
            case InlineCstKind::Strikethrough: case InlineCstKind::CodeSpan:
            case InlineCstKind::InlineMath: break;
            default: continue;
        }
        const auto& delimiters = node.delimiter_ranges();
        if (node.status != ParseStatus::Complete || !delimiters.closing) continue;
        for (auto range : {delimiters.opening, *delimiters.closing})
            if (character >= range.start && character < range.end) return &node;
    }
    return nullptr;
}

inline std::size_t inline_format_marker_boundary(
    const InlineDocument& document, std::size_t offset, bool forward) {
    if (auto node = inline_format_marker_at(document.tree.nodes, offset)) {
        const auto& delimiters = node->delimiter_ranges();
        for (auto range : {delimiters.opening, *delimiters.closing})
            if (offset > range.start && offset < range.end)
                return forward ? range.end : range.start;
    }
    return offset;
}

inline std::optional<SourceRange> inline_backward_delete_range(
    const InlineDocument& document,
    std::size_t offset,
    bool explicit_break_lexemes_are_semantic = false) {
    offset = (std::min)(offset, document.source.size());
    if (offset == 0) return std::nullopt;
    if (auto hard_break = inline_source_edit_detail::adjacent_hard_break(
            document.tree.nodes, offset, true)) {
        return hard_break;
    }
    if (explicit_break_lexemes_are_semantic) {
        if (auto hard_break = inline_source_edit_detail::adjacent_explicit_break_lexeme(
                document.source, offset, true)) {
            return hard_break;
        }
    }
    return SourceRange{offset - 1, offset};
}

inline std::optional<SourceRange> inline_forward_delete_range(
    const InlineDocument& document,
    std::size_t offset,
    bool explicit_break_lexemes_are_semantic = false) {
    offset = (std::min)(offset, document.source.size());
    if (offset == document.source.size()) return std::nullopt;
    if (auto hard_break = inline_source_edit_detail::adjacent_hard_break(
            document.tree.nodes, offset, false)) {
        return hard_break;
    }
    if (explicit_break_lexemes_are_semantic) {
        if (auto hard_break = inline_source_edit_detail::adjacent_explicit_break_lexeme(
                document.source, offset, false)) {
            return hard_break;
        }
    }
    return SourceRange{offset, offset + 1};
}

inline AppliedSourceEdit apply_inline_source_edit(
    NodeId owner_id,
    InlineDocument& document,
    const TextEdit& edit,
    const InlineParseContext& parse_context) {
    if (edit.container_id != owner_id) {
        throw std::invalid_argument("TextEdit container does not own this InlineDocument");
    }
    if (!edit.range.valid_for(document.source.size())) {
        throw std::out_of_range("TextEdit range is outside InlineDocument::source");
    }

    const auto before_source = document.source;
    const auto before_tree = document.tree;
    const auto removed = before_source.substr(edit.range.start, edit.range.length());
    apply_text_edit(document.source, edit);
    reparse_inline_document(document, parse_context);

    inline_source_edit_detail::reconcile_nodes(
        before_tree.nodes, document.tree.nodes, before_source, document.source, edit);
    TextEdit inverse;
    inverse.container_id = owner_id;
    inverse.range = {edit.range.start, edit.range.start + edit.replacement.size()};
    inverse.replacement = removed;
    return AppliedSourceEdit{edit, std::move(inverse), InlineIdentityPatch{
        inline_source_edit_detail::node_identities(before_tree.nodes),
        inline_source_edit_detail::node_identities(document.tree.nodes)}};
}

} // namespace folia
