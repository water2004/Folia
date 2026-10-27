// folia.core.table_selection — semantic table-cell addressing for selections.
//
// Text positions remain block-local.  This module only projects those
// positions onto the unified block tree so table commands can distinguish a
// text range from an explicit structural target without inventing a second
// selection coordinate.
export module folia.core.table_selection;
import std;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.ids;
import folia.markdown.text_edit;

export namespace folia {

struct TableCellAddress {
    NodeId table_id{};
    NodeId row_id{};
    NodeId cell_id{};
    std::size_t row = 0;
    std::size_t column = 0;

    bool operator==(TableCellAddress const&) const = default;
};

inline std::optional<TableCellAddress> document_table_cell_address(
    EditorDocument const& document,
    NodeId editable_owner) {
    auto path = block_path(document.root, editable_owner);
    if (!path) return std::nullopt;
    while (!path->empty()) {
        auto const* candidate = block_at_path(document.root, *path);
        if (candidate && candidate->kind == BlockKind::TableCell) break;
        path->pop_back();
    }
    if (path->size() < 3) return std::nullopt;

    auto cell_path = *path;
    auto const column = cell_path.back();
    cell_path.pop_back();
    auto const* row = block_at_path(document.root, cell_path);
    if (!row || row->kind != BlockKind::TableRow
        || column >= row->children.size()) return std::nullopt;

    auto table_path = cell_path;
    auto const row_index = table_path.back();
    table_path.pop_back();
    auto const* table = block_at_path(document.root, table_path);
    if (!table || table->kind != BlockKind::Table
        || row_index >= table->children.size()) return std::nullopt;

    return TableCellAddress{
        table->id,
        row->id,
        row->children[column].id,
        row_index,
        column};
}

inline std::optional<TableCellAddress> document_table_edit_target(
    EditorDocument const& document,
    TextSelection const& selection) {
    auto anchor = document_table_cell_address(
        document, selection.anchor.container_id);
    auto active = document_table_cell_address(
        document, selection.active.container_id);
    if (!anchor || !active || anchor->cell_id != active->cell_id) {
        return std::nullopt;
    }
    return active;
}

// Project a cross-owner selection into independent source ranges whenever it
// intersects a table. The table is a structure barrier: callers may edit or
// copy these ranges, but must not remove individual cells while processing an
// ordinary text selection.
inline std::optional<std::vector<TextSpan>> document_table_barrier_text_slices(
    EditorDocument const& document,
    TextSelection const& selection) {
    auto anchor_index = document_editable_order_position(
        document, selection.anchor.container_id);
    auto active_index = document_editable_order_position(
        document, selection.active.container_id);
    if (!anchor_index || !active_index) return std::nullopt;

    auto first = selection.anchor;
    auto last = selection.active;
    auto first_index = *anchor_index;
    auto last_index = *active_index;
    if (last_index < first_index
        || (first_index == last_index
            && last.source_offset < first.source_offset)) {
        std::swap(first, last);
        std::swap(first_index, last_index);
    }

    bool intersects_table = false;
    std::vector<TextSpan> result;
    result.reserve(last_index - first_index + 1);
    for (auto index = first_index; index <= last_index; ++index) {
        auto const owner = document.cached_editable_order[index];
        auto const* block = find_document_block(document, owner);
        auto source = block ? editable_block_text_view(*block) : std::nullopt;
        if (!source) return std::nullopt;
        intersects_table = intersects_table
            || document_table_cell_address(document, owner).has_value();
        auto start = index == first_index
            ? (std::min)(first.source_offset, source->size())
            : std::size_t{0};
        auto end = index == last_index
            ? (std::min)(last.source_offset, source->size())
            : source->size();
        if (first_index == last_index && end < start) std::swap(start, end);
        result.push_back({owner, {start, end}});
    }
    return intersects_table
        ? std::optional<std::vector<TextSpan>>{std::move(result)}
        : std::nullopt;
}

} // namespace folia
