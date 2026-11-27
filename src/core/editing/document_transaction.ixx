// folia.core.document_transaction — reversible document operations produced by
// editing commands. Commands apply these operations directly to the
// authoritative document; transactions never retain complete document
// snapshots.
export module folia.core.document_transaction;
import std;
import folia.markdown.ids;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.block_source;
import folia.markdown.document;
import folia.markdown.text_edit;
import folia.markdown.metadata;

export namespace folia {

enum class DocumentTransactionReason { InsertText, Delete, Paste, Replace, Format, Structure };

struct DocumentTextOperation {
    TextEdit forward;
    TextEdit inverse;
    std::optional<std::vector<BlockLineIndentation>> indentation_before;
    std::optional<std::vector<BlockLineIndentation>> indentation_after;
    std::optional<InlineIdentityPatch> inline_identities;
};

enum class DocumentTreeEditKind { Insert, Remove, Move, UpdatePayload, ReorderChildren };

struct DocumentTreeEdit {
    DocumentTreeEditKind kind = DocumentTreeEditKind::Insert;
    NodeId parent_id{};
    NodeId other_parent_id{};
    NodeId moved_id{};
    std::size_t index = 0;
    std::size_t other_index = 0;
    BlockNode before;
    BlockNode after;
    // For a reorder, map each old child index to its new index within the
    // contiguous interval starting at index. Undo uses the inverse mapping.
    std::vector<std::size_t> order;
};

// A derived invalidation delta, not a replay operation. Its direction is
// already normalized for the applied transaction (including undo). Consumers
// can distinguish a child-vector splice from a change to a container's own
// properties without retaining a second subtree or inspecting source text.
struct DocumentTreeDelta {
    DocumentTreeEditKind kind = DocumentTreeEditKind::Insert;
    NodeId parent_id{};
    NodeId other_parent_id{};
    NodeId root_id{};
    std::size_t index = 0;
    std::size_t other_index = 0;
    NodeId first_owner{};
    NodeId last_owner{};
    bool source_payload_changed = false;
    std::optional<BlockNode> before_payload;
    std::optional<BlockNode> after_payload;
};

struct DocumentEndingEdit {
    std::u32string before;
    std::u32string after;
};

struct DocumentMetadataEdit {
    DocumentMetadata before;
    DocumentMetadata after;
};

using DocumentOperation = std::variant<DocumentTextOperation,DocumentTreeEdit,DocumentEndingEdit,DocumentMetadataEdit>;

struct DocumentSourceEdit : AppliedSourceEdit {
    std::vector<DocumentOperation> metadata_edits;
    std::optional<std::vector<BlockLineIndentation>> indentation_before;
    std::optional<std::vector<BlockLineIndentation>> indentation_after;
};

struct DocumentTransaction {
    std::vector<DocumentOperation> operations;
    TextSelection selection_before;
    TextSelection selection_after;
    std::uint64_t revision_before = 0;
    std::uint64_t revision_after = 0;
    DocumentTransactionReason reason = DocumentTransactionReason::Structure;
};

namespace document_transaction_detail {

inline BlockNode payload_shell(const BlockNode& source) {
    // Build the shell directly. Copying a node and clearing its content first
    // transiently duplicates the entire descendant tree and all block-local
    // source, which is especially costly for document-wide metadata edits.
    BlockNode shell;
    shell.id = source.id;
    shell.kind = source.kind;
    shell.separator_before = source.separator_before;
    shell.payload = source.payload
        ? std::make_unique<BlockNodeSpecial>(*source.payload)
        : nullptr;
    // Source text is replayed by TextEdits, but the grammar must survive a
    // paragraph-to-raw-block transition before those edits are applied.
    if (source.block_source.storage) {
        shell.block_source.tree().kind = source.block_source.tree().kind;
    }
    return shell;
}

inline BlockNode semantic_payload_shell(const BlockNode& source) {
    // Presentation consumers need semantic properties, not serializer prefix
    // provenance. In a long quote, copying source_prefixes here would turn a
    // one-item Enter into another copy of the entire ancestor's line records.
    BlockNode shell;
    shell.id = source.id;
    shell.kind = source.kind;
    if (!source.payload) return shell;
    auto const& payload = *source.payload;
    if (payload.text) shell.ensure_text_special() = *payload.text;
    if (payload.list) shell.ensure_list_special() = *payload.list;
    if (payload.item) shell.ensure_item_special() = *payload.item;
    if (payload.atomic) {
        auto const& before = *payload.atomic;
        auto& atomic = shell.ensure_atomic_special();
        atomic.code_indented = before.code_indented;
        atomic.math_delim = before.math_delim;
        atomic.toc_marker = before.toc_marker;
        atomic.fmt = before.fmt;
        atomic.unsup_reason = before.unsup_reason;
        atomic.ext_name = before.ext_name;
    }
    if (payload.table) {
        auto& table = shell.ensure_table_special();
        table.table_aligns = payload.table->table_aligns;
        table.table_header_row = payload.table->table_header_row;
        table.metadata = payload.table->metadata;
    }
    if (payload.supplemental) {
        auto const& supplemental = *payload.supplemental;
        if (supplemental.image) shell.ensure_image_special() = *supplemental.image;
        if (supplemental.container) shell.ensure_container_special() = *supplemental.container;
        if (supplemental.html) {
            auto const& before = *supplemental.html;
            auto& html = shell.ensure_html_special();
            html.root_tag = before.root_tag;
            html.opening_marker = before.opening_marker;
            html.closing_marker = before.closing_marker;
            html.text_alignment = before.text_alignment;
        }
    }
    return shell;
}

} // namespace document_transaction_detail

inline DocumentTreeDelta summarize_tree_delta(DocumentTreeEdit const& edit, bool forward) {
    DocumentTreeDelta delta;
    delta.kind = edit.kind;
    delta.parent_id = edit.parent_id;
    delta.other_parent_id = edit.other_parent_id;
    delta.index = edit.index;
    delta.other_index = edit.other_index;
    if (edit.kind == DocumentTreeEditKind::Move) {
        delta.root_id = edit.moved_id;
        if (!forward) {
            std::swap(delta.parent_id, delta.other_parent_id);
            std::swap(delta.index, delta.other_index);
        }
    } else if (edit.kind == DocumentTreeEditKind::UpdatePayload) {
        delta.root_id = edit.before.id;
        delta.source_payload_changed = edit.before.atomic_special().raw != edit.after.atomic_special().raw;
        delta.before_payload = document_transaction_detail::semantic_payload_shell(
            forward ? edit.before : edit.after);
        delta.after_payload = document_transaction_detail::semantic_payload_shell(
            forward ? edit.after : edit.before);
    } else if (edit.kind == DocumentTreeEditKind::ReorderChildren) {
        delta.root_id = edit.parent_id;
    } else {
        auto const& subtree = edit.kind == DocumentTreeEditKind::Insert ? edit.after : edit.before;
        delta.root_id = subtree.id;
        delta.first_owner = first_editable_subtree_owner(subtree);
        delta.last_owner = last_editable_subtree_owner(subtree);
        if (!forward) delta.kind = edit.kind == DocumentTreeEditKind::Insert
            ? DocumentTreeEditKind::Remove : DocumentTreeEditKind::Insert;
    }
    return delta;
}

inline DocumentTransaction make_recorded_document_transaction(
    std::vector<DocumentOperation> operations,
    TextSelection selection_before,
    TextSelection selection_after,
    std::uint64_t revision_before,
    std::uint64_t revision_after,
    DocumentTransactionReason reason) {
    DocumentTransaction transaction;
    transaction.operations = std::move(operations);
    transaction.selection_before = selection_before;
    transaction.selection_after = selection_after;
    transaction.revision_before = revision_before;
    transaction.revision_after = revision_after;
    transaction.reason = reason;
    return transaction;
}

} // namespace folia
