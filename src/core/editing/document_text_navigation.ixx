// Word/paragraph intent over the authoritative editable owner order. Raw
// blocks navigate their rendered body and map back to block-local source.
export module folia.core.document_text_navigation;
import std;
import folia.markdown.ids;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.block_source;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.core.document_edit_support;
import folia.core.document_source_edit;
import folia.core.inline_source_edit;
import folia.core.text_navigation;

export namespace folia {

enum class DocumentTextMove { WordLeft, WordRight, ParagraphStart, ParagraphEnd };

// Pointer word selection uses the same visible body as keyboard word motion.
// Raw source also owns hidden fences/HTML tags and indentation prefixes, which
// must never become selected merely because they border a whitespace run.
inline std::optional<TextSelection> document_word_selection_at(
    const EditorDocument& document, TextPosition position) {
    const auto* block = find_document_block(document, position.container_id);
    if (!block) return std::nullopt;
    std::optional<SourceRange> selected;
    if (editable_raw_block_source(*block)) {
        const auto& body = block->block_source.tree();
        const auto source = clamp_editable_block_source_offset(*block, position.source_offset);
        const auto mapped = body.content_to_source.lower_bound(source);
        const auto offset = mapped == body.content_to_source.end() ? body.content.size()
            : static_cast<std::size_t>(mapped - body.content_to_source.begin());
        if (auto range = text_word_selection_range(body.content, offset))
            selected = SourceRange{body.content_to_source[range->start], body.content_to_source[range->end]};
    } else if (const auto* inlines = editable_inline_document(*block)) {
        selected = text_word_selection_range(inlines->source, position.source_offset);
    }
    if (!selected) return std::nullopt;
    return TextSelection{{position.container_id, selected->start, TextAffinity::Downstream},
        {position.container_id, selected->end, TextAffinity::Upstream}};
}

inline std::optional<TextSelection> document_move_text_selection(
    const EditorDocument& document, const TextSelection& selection,
    DocumentTextMove movement, bool extend) {
    const bool backward = movement == DocumentTextMove::WordLeft
        || movement == DocumentTextMove::ParagraphStart;
    const bool word = movement == DocumentTextMove::WordLeft
        || movement == DocumentTextMove::WordRight;
    if (word && !extend && !selection.is_caret())
        return document_move_selection(document, selection,
            backward ? DocumentMove::Left : DocumentMove::Right, false);

    const auto* block = find_document_block(document, selection.active.container_id);
    if (!block) return std::nullopt;
    const auto range = editable_block_source_range(*block);
    const auto source = editable_block_text_view(*block);
    if (!range || !source) return std::nullopt;
    auto target = selection.active;
    target.source_offset = clamp_editable_block_source_offset(*block, target.source_offset);
    const auto edge = backward ? range->start : range->end;
    if (target.source_offset == edge || document_edit_detail::atomic_block(block->kind)) {
        auto moved = document_move_selection(document, TextSelection::caret(target),
            backward ? DocumentMove::Left : DocumentMove::Right, false);
        if (!moved) return std::nullopt;
        target = moved->active;
        if (!word) {
            const auto* destination = find_document_block(document, target.container_id);
            const auto destination_range = destination
                ? editable_block_source_range(*destination) : std::nullopt;
            if (!destination_range) return std::nullopt;
            target.source_offset = backward ? destination_range->start : destination_range->end;
        }
    } else if (!word) {
        target.source_offset = edge;
    } else if (editable_raw_block_source(*block)) {
        const auto& body = block->block_source.tree();
        const auto mapped = body.content_to_source.lower_bound(target.source_offset);
        const auto offset = mapped == body.content_to_source.end() ? body.content.size()
            : static_cast<std::size_t>(mapped - body.content_to_source.begin());
        const auto destination = backward ? previous_word_boundary(body.content, offset)
            : next_word_boundary(body.content, offset);
        target.source_offset = body.content_to_source[destination];
    } else {
        target.source_offset = backward ? previous_word_boundary(*source, target.source_offset)
            : next_word_boundary(*source, target.source_offset);
        if (const auto* owner = editable_inline_document(*block))
            target.source_offset = inline_format_marker_boundary(*owner, target.source_offset, !backward);
    }
    target.affinity = backward ? TextAffinity::Upstream : TextAffinity::Downstream;
    return extend ? TextSelection{selection.anchor, target} : TextSelection::caret(target);
}

inline std::optional<DocumentTransaction> document_delete_word(
    EditorDocument& document, const TextSelection& selection, bool backward) {
    if (!selection.is_caret()) return document_delete_selection(document, selection);
    const auto* block = find_document_block(document, selection.active.container_id);
    if (!block) return std::nullopt;
    const auto range = editable_block_source_range(*block);
    if (!range) return std::nullopt;
    // At a structural boundary the ordinary deletion contract owns merging,
    // unwrapping and object handling. A word command must not delete fences
    // or table structure by constructing a cross-owner text slice.
    if (document_edit_detail::atomic_block(block->kind)
        || (selection.active.source_offset <= range->start && backward)
        || (selection.active.source_offset >= range->end && !backward))
        return backward ? document_delete_backward(document, selection)
            : document_delete_forward(document, selection);
    auto selected = document_move_text_selection(document, selection,
        backward ? DocumentTextMove::WordLeft : DocumentTextMove::WordRight, true);
    if (!selected || selected->is_caret()) return std::nullopt;
    auto transaction = document_delete_selection(document, *selected);
    if (transaction) transaction->selection_before = selection;
    return transaction;
}

} // namespace folia
