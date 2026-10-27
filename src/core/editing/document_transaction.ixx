// folia.core.document_transaction — reversible document operations produced by
// editing commands. Commands apply these operations directly to the
// authoritative document; transactions never retain complete document
// snapshots.
export module folia.core.document_transaction;
import std;
import folia.markdown.ast;
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

enum class DocumentTreeEditKind { Insert, Remove, Move, UpdatePayload };

struct DocumentTreeEdit {
    DocumentTreeEditKind kind = DocumentTreeEditKind::Insert;
    NodeId parent_id{};
    NodeId other_parent_id{};
    NodeId moved_id{};
    std::size_t index = 0;
    std::size_t other_index = 0;
    BlockNode before;
    BlockNode after;
};

struct DocumentSourceEdit : AppliedSourceEdit {
    std::vector<DocumentTreeEdit> metadata_edits;
    std::optional<std::vector<BlockLineIndentation>> indentation_before;
    std::optional<std::vector<BlockLineIndentation>> indentation_after;
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
    return shell;
}

} // namespace document_transaction_detail

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
