// Preserve complete inline HTML envelopes when a semantic HTML block splits.
export module folia.core.document_html_split;
import std;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.text_edit;

export namespace folia {

struct HtmlInlineSplitPlan {
    std::size_t source_offset = 0;
    std::u32string left_suffix;
    std::u32string right_prefix;
    std::size_t right_caret_offset = 0;
};

namespace document_html_split_detail {

inline const InlineCstNode* containing_node(
    const InlineCstNodes& nodes, std::size_t offset) {
    for (const auto& node : nodes) {
        if (node.range.start < offset && offset < node.range.end) return &node;
    }
    return nullptr;
}

inline bool complete_envelope(const InlineCstNode& node, std::size_t source_size) {
    const auto& ranges = node.delimiter_ranges();
    return node.kind == InlineCstKind::HtmlElement
        && node.status == ParseStatus::Complete && ranges.closing
        && !ranges.opening.empty() && !ranges.closing->empty()
        && ranges.opening.valid_for(source_size) && ranges.content.valid_for(source_size)
        && ranges.closing->valid_for(source_size)
        && ranges.opening.end == ranges.content.start
        && ranges.content.end == ranges.closing->start;
}

inline std::size_t normalize_offset(const InlineDocument& document, std::size_t offset) {
    const auto* nodes = &document.tree.nodes;
    while (const auto* node = containing_node(*nodes, offset)) {
        if (complete_envelope(*node, document.source.size())) {
            const auto& ranges = node->delimiter_ranges();
            // Visible caret mappings use marker edges. A selection returning
            // from source mode can still be inside a tag: keep it on the
            // adjacent content edge rather than cutting an attribute or tag.
            if (offset < ranges.opening.end) offset = ranges.content.start;
            else if (offset > ranges.closing->start) offset = ranges.content.end;
        } else if (node->kind == InlineCstKind::HtmlElement
            || node->kind == InlineCstKind::HtmlComment
            || node->kind == InlineCstKind::Entity) {
            // Comments, void elements and character references have
            // no reopenable body. Their nearest source edge is the same
            // visible boundary; ties choose the following edge.
            return offset - node->range.start < node->range.end - offset
                ? node->range.start : node->range.end;
        }
        nodes = &node->children;
    }
    return offset;
}

} // namespace document_html_split_detail

inline HtmlInlineSplitPlan plan_html_inline_split(
    const InlineDocument& document, std::size_t offset) {
    HtmlInlineSplitPlan plan;
    plan.source_offset = (std::min)(offset, document.source.size());
    if (document.syntax_mode != InlineSyntaxMode::HtmlText) return plan;
    plan.source_offset = document_html_split_detail::normalize_offset(
        document, plan.source_offset);

    std::vector<SourceRange> closings;
    const auto* nodes = &document.tree.nodes;
    while (const auto* node = document_html_split_detail::containing_node(
               *nodes, plan.source_offset)) {
        if (document_html_split_detail::complete_envelope(*node, document.source.size())) {
            const auto& ranges = node->delimiter_ranges();
            if (ranges.content.start <= plan.source_offset
                && plan.source_offset <= ranges.content.end) {
                plan.right_prefix += inline_source_slice(document, ranges.opening);
                closings.push_back(*ranges.closing);
            }
        }
        nodes = &node->children;
    }
    for (auto index = closings.size(); index > 0; --index)
        plan.left_suffix += inline_source_slice(document, closings[index - 1]);
    plan.right_caret_offset = plan.right_prefix.size();
    return plan;
}

} // namespace folia
