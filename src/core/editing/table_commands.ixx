export module folia.core.table_commands;
import std;
import folia.markdown.document;
import folia.markdown.text_edit;
import folia.markdown.table_metadata;
import folia.core.document_transaction;

export namespace folia {
std::optional<DocumentTransaction> document_sort_table(
    EditorDocument& document, const TextSelection& selection, bool descending);
std::optional<DocumentTransaction> document_fill_table(
    EditorDocument& document, const TextSelection& selection, TableCoordinate destination, bool copy);
std::optional<DocumentTransaction> document_commit_table_cell(EditorDocument& document,
    const TextSelection& selection, std::optional<std::u32string_view> replacement = {});
std::optional<DocumentTransaction> document_edit_table_cell_text(EditorDocument& document,
    const TextSelection& selection, const std::function<std::optional<DocumentTransaction>(TextSelection)>& edit,
    TextPosition* destination = nullptr);
std::optional<DocumentTransaction> document_toggle_table_checkbox(
    EditorDocument& document, const TextSelection& selection, bool create);
std::optional<DocumentTransaction> document_delete_table(
    EditorDocument& document, const TextSelection& selection);
}
