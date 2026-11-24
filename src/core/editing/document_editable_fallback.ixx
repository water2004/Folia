// A zero-source editing destination for documents containing only invisible
// structure, such as complete HTML comments. The parser remains lossless and
// does not manufacture editor-only content.
export module folia.core.document_editable_fallback;
import std;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.ids;
import folia.markdown.inline_document;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.core.document_edit_primitives;
import folia.core.document_transaction;

export namespace folia {

struct DocumentEditableFallback {
    NodeId owner;
    std::size_t root_index = 0;
};

// The caller has established that no editable owner survives. Keeping this
// precondition explicit avoids another tree walk after a scoped source patch
// or an edit that has already looked for its surviving insertion position.
inline TextPosition append_document_editable_destination(
    EditorDocument& document, std::vector<DocumentOperation>* operations = nullptr) {
    document_edit_detail::NodeAllocator allocator(document);
    auto paragraph = document_edit_detail::empty_paragraph(allocator, document);
    const auto index = document.root.children.size();
    // Explicitly suppress the ordinary sibling gap. Until the user edits
    // here this owner contributes no bytes to the existing document.
    if (index != 0) paragraph.separator_before = U"";
    const auto position = TextPosition{paragraph.id, 0, TextAffinity::Downstream};
    if (operations) {
        DocumentTreeEdit insert;
        insert.parent_id = document.root.id;
        insert.index = index;
        insert.after = paragraph;
        operations->emplace_back(std::move(insert));
    }
    document.root.children.push_back(std::move(paragraph));
    return position;
}

inline std::optional<TextPosition> ensure_document_editable_destination(
    EditorDocument& document, std::vector<DocumentOperation>* operations = nullptr) {
    if (first_document_text_position(document)) return std::nullopt;
    return append_document_editable_destination(document, operations);
}

// This is derived editor state, recognized from the normal authoritative tree
// after its owner index has been refreshed. No flag is stored in Markdown or
// in parser nodes, and undo restores the same ordinary empty paragraph.
inline std::optional<DocumentEditableFallback> document_editable_fallback(
    const EditorDocument& document) {
    if (document.cached_editable_order.size() != 1 || document.root.children.size() < 2)
        return std::nullopt;
    const auto& last = document.root.children.back();
    if (last.id != document.cached_editable_order.front()
        || last.kind != BlockKind::Paragraph
        || !last.inline_content().source.empty()
        || last.inline_content().syntax_mode != InlineSyntaxMode::Markdown
        || !last.separator_before || !(*last.separator_before).empty()) return std::nullopt;
    return DocumentEditableFallback{last.id, document.root.children.size() - 1};
}

inline void materialize_document_editable_fallback(
    EditorDocument& document, DocumentEditableFallback fallback,
    DocumentTransaction& transaction) {
    auto& roots = document.root.children;
    auto found = std::ranges::find(roots, fallback.owner, &BlockNode::id);
    // Enter/paste may split the zero-source destination. Its untouched left
    // half has no saved content; use the newly created owner as the first real
    // paragraph, and record removal of the old landing point in this edit.
    if (found != roots.end() && found->kind == BlockKind::Paragraph
        && found->inline_content().source.empty()
        && transaction.selection_after.anchor.container_id != fallback.owner
        && transaction.selection_after.active.container_id != fallback.owner) {
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = document.root.id;
        remove.index = static_cast<std::size_t>(found - roots.begin());
        remove.before = *found;
        transaction.operations.emplace_back(std::move(remove));
        roots.erase(found);
    }
    auto real_gap = [&](BlockNode& block) {
        if (block.separator_before && !(*block.separator_before).empty()) return;
        DocumentTreeEdit gap;
        gap.kind = DocumentTreeEditKind::UpdatePayload;
        gap.before = document_transaction_detail::payload_shell(block);
        const auto ending = document.trailing_line_ending.empty()
            ? std::u32string{U"\n"} : document.trailing_line_ending;
        // The saved document ending already supplies the final empty line.
        // Counting it again here would reload as two empty paragraphs after
        // a single Enter in a comment-only file that originally ended in EOL.
        const bool saved_empty_line = !document.trailing_line_ending.empty()
            && block.id == roots.back().id && block.kind == BlockKind::Paragraph
            && block.inline_content().source.empty();
        block.separator_before = saved_empty_line ? ending : ending + ending;
        gap.after = document_transaction_detail::payload_shell(block);
        transaction.operations.emplace_back(std::move(gap));
    };
    if (fallback.root_index < roots.size()) real_gap(roots[fallback.root_index]);
    // A command can insert an object before the original owner and retain
    // that empty owner for its caret. It now needs an ordinary saved gap too.
    found = std::ranges::find(roots, fallback.owner, &BlockNode::id);
    if (found != roots.end() && found->separator_before && (*found->separator_before).empty())
        real_gap(*found);
}

} // namespace folia
