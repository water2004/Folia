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
import folia.markdown.table_metadata;
import folia.core.table_values;

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
    auto path = document_block_path(document, editable_owner);
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

struct TableCellSelection {
    NodeId table;
    TableRegion region;
};

struct MergedTableCell {
    const BlockNode* table;
    TableRegion region;

    const BlockNode& anchor() const { return table->children[region.first.row].children[region.first.column]; }

    std::vector<const BlockNode*> text_parts() const {
        std::vector<const BlockNode*> parts;
        for (auto row = region.first.row; row <= region.last.row; ++row)
            for (auto column = region.first.column; column <= region.last.column; ++column) {
                const auto& cell = table->children[row].children[column];
                if (!cell.inline_content().source.empty()) parts.push_back(&cell);
            }
        if (parts.empty()) parts.push_back(&anchor());
        return parts;
    }
};

inline std::optional<MergedTableCell> document_merged_table_cell(
    const EditorDocument& document, const TableCellAddress& address) {
    const auto* table = find_document_block(document, address.table_id);
    if (!table || table->has_html_source() || table->children.empty()) return std::nullopt;
    const auto* merge = table_merge_at(table->table_special().metadata.get(),
        {address.row, address.column}, table->children.size(), table->children.front().children.size());
    if (!merge) return std::nullopt;
    return MergedTableCell{table, *merge};
}

inline std::optional<TableCellAddress> document_visual_table_cell_address(const EditorDocument& document, TextPosition position) {
    auto address = document_table_cell_address(document, position.container_id);
    if (!address) return std::nullopt;
    const auto* table = find_document_block(document, address->table_id);
    if (!table || table->has_html_source()) return std::nullopt;
    if (const auto* merge = table_merge_at(table->table_special().metadata.get(),
            {address->row, address->column}, table->children.size(), table->children.front().children.size())) {
        address->row = merge->first.row;
        address->column = merge->first.column;
        const auto& row = table->children[address->row];
        address->row_id = row.id;
        address->cell_id = row.children[address->column].id;
    }
    return address;
}

inline std::optional<NodeId> document_table_text_owner(const EditorDocument& document, TextPosition position) {
    const auto address = document_visual_table_cell_address(document, position);
    return address ? std::optional{address->cell_id} : std::nullopt;
}

inline std::optional<TextSelection> document_table_cell_contents(const EditorDocument& document, TextPosition position) {
    const auto address = document_table_cell_address(document, position.container_id);
    if (!address) return std::nullopt;
    const auto* table = find_document_block(document, address->table_id);
    if (!table || table->has_html_source()) return std::nullopt;
    if (const auto merged = document_merged_table_cell(document, *address)) {
        const auto parts = merged->text_parts();
        return TextSelection{{parts.front()->id, 0},
            {parts.back()->id, parts.back()->inline_content().source.size(), TextAffinity::Upstream}};
    }
    const auto* cell = find_document_block(document, address->cell_id);
    if (!cell) return std::nullopt;
    return TextSelection{{cell->id, 0}, {cell->id, cell->inline_content().source.size(), TextAffinity::Upstream}};
}

inline std::optional<std::vector<TextSpan>> document_merged_table_text_slices(
    const EditorDocument& document, const TextSelection& selection) {
    if (selection.selects_cells() || selection.anchor.container_id == selection.active.container_id) return std::nullopt;
    const auto address = document_table_cell_address(document, selection.active.container_id);
    const auto merged = address ? document_merged_table_cell(document, *address) : std::nullopt;
    if (!merged) return std::nullopt;
    const auto parts = merged->text_parts();
    auto find = [&](TextPosition position) {
        return std::ranges::find_if(parts, [&](const auto* cell) { return cell->id == position.container_id; });
    };
    auto first = find(selection.anchor);
    auto last = find(selection.active);
    if (first == parts.end() || last == parts.end()) return std::nullopt;
    auto start = selection.anchor.source_offset;
    auto end = selection.active.source_offset;
    if (first > last) {
        std::swap(first, last);
        std::swap(start, end);
    }
    std::vector<TextSpan> slices;
    slices.reserve(last - first + 1);
    for (auto part = first; part <= last; ++part) {
        const auto size = (*part)->inline_content().source.size();
        slices.push_back({(*part)->id, {part == first ? start : 0, part == last ? end : size}});
    }
    return slices;
}

inline std::optional<std::u32string> document_selected_merged_table_text(
    const EditorDocument& document, const TextSelection& selection) {
    const auto slices = document_merged_table_text_slices(document, selection);
    if (!slices) return std::nullopt;
    std::u32string result;
    for (const auto& slice : *slices) {
        const auto* cell = find_document_block(document, slice.container_id);
        if (&slice != &slices->front()) result += U'\n';
        result.append(cell->inline_content().source, slice.source_range.start, slice.source_range.length());
    }
    return result;
}

inline std::optional<TextSelection> document_move_merged_table_text(
    const EditorDocument& document, const TextSelection& selection, bool forward, bool extend) {
    if (!extend && !selection.is_caret()) return std::nullopt;
    const auto address = document_table_cell_address(document, selection.active.container_id);
    const auto merged = address ? document_merged_table_cell(document, *address) : std::nullopt;
    if (!merged) return std::nullopt;
    if (selection.anchor.container_id != selection.active.container_id
        && document_table_text_owner(document, selection.anchor) != merged->anchor().id) return std::nullopt;
    const auto parts = merged->text_parts();
    const auto current = std::ranges::find_if(parts, [&](const auto* cell) { return cell->id == selection.active.container_id; });
    if (current == parts.end() || selection.active.source_offset != (forward ? (*current)->inline_content().source.size() : 0))
        return std::nullopt;
    const auto boundary = forward ? parts.end() - 1 : parts.begin();
    if (current == boundary) return selection;
    const auto* next = forward ? *(current + 1) : *(current - 1);
    const TextPosition destination{next->id, forward ? 0 : next->inline_content().source.size(),
        forward ? TextAffinity::Downstream : TextAffinity::Upstream};
    return TextSelection{extend ? selection.anchor : destination, destination};
}

inline bool table_cell_is_computed(const BlockNode& table, TableCoordinate cell) {
    const auto& metadata = table.table_special().metadata;
    return metadata && ((table_has_totals(metadata.get()) && cell.row + 1 == table.children.size())
        || (cell.row >= metadata->header_rows && cell.column < metadata->formulas.size()
            && !metadata->formulas[cell.column].empty()));
}

struct TableCellDetails {
    TableCellAddress address;
    std::shared_ptr<const TableMetadata> metadata;
    std::u32string source;
    bool computed = false;
    bool header = false;
    std::optional<bool> checkbox;
    std::optional<TableRegion> merged;

    TableColumnType column_type() const {
        return !header && !computed && metadata && address.column < metadata->column_types.size()
            ? metadata->column_types[address.column] : TableColumnType{};
    }
};

inline bool document_table_cell_is_computed(const EditorDocument& document, NodeId owner) {
    const auto* block = find_document_block(document, owner);
    if (!block || block->kind != BlockKind::TableCell) return false;
    const auto cell = document_table_cell_address(document, owner);
    const auto* table = cell ? find_document_block(document, cell->table_id) : nullptr;
    return table && table_cell_is_computed(*table, {cell->row, cell->column});
}

inline std::optional<TableCellDetails> document_table_cell_details(
    const EditorDocument& document, TextPosition position) {
    const auto address = document_table_cell_address(document, position.container_id);
    if (!address) return std::nullopt;
    const auto* table = find_document_block(document, address->table_id);
    const auto* cell = find_document_block(document, address->cell_id);
    if (!table || !cell || table->has_html_source()) return std::nullopt;
    auto const& metadata = table->table_special().metadata;
    const bool header = address->row < (metadata ? metadata->header_rows : 1);
    const auto merged = document_merged_table_cell(document, *address);
    const auto& firstSource = merged ? merged->text_parts().front()->inline_content().source : cell->inline_content().source;
    return TableCellDetails{*address, metadata, cell->inline_content().source,
        table_cell_is_computed(*table, {address->row, address->column}), header, table_checkbox(firstSource),
        merged ? std::optional{merged->region} : std::nullopt};
}

inline std::optional<TableCellSelection> document_table_cell_selection(
    const EditorDocument& document, const TextSelection& selection) {
    if (!selection.selects_cells()) return std::nullopt;
    auto anchor = document_table_cell_address(document, selection.anchor.container_id);
    auto active = document_table_cell_address(document, selection.active.container_id);
    if (!anchor || !active || anchor->table_id != active->table_id) return std::nullopt;
    const auto* table = find_document_block(document, active->table_id);
    if (!table || table->has_html_source() || table->children.empty()) return std::nullopt;
    TableRegion region{
        {(std::min)(anchor->row, active->row), (std::min)(anchor->column, active->column)},
        {(std::max)(anchor->row, active->row), (std::max)(anchor->column, active->column)}};
    const auto& metadata = table->table_special().metadata;
    if (metadata && !metadata->merges.empty()) {
        auto rows = table->children.size();
        auto columns = table->children.front().children.size();
        TableRegion prior;
        do {
            prior = region;
            for (const auto& merge : metadata->merges)
                if (merge.last.row < rows && merge.last.column < columns && region.intersects(merge))
                    region.include(merge);
        } while (region != prior);
    }
    return TableCellSelection{table->id, region};
}

inline std::optional<TextSelection> table_adjacent_text(
    const EditorDocument& document, const BlockNode& table, bool after) {
    const auto& row = after ? table.children.back() : table.children.front();
    const auto& cell = after ? row.children.back() : row.children.front();
    const auto index = document_editable_order_position(document, cell.id);
    if (!index) return std::nullopt;
    if (after && *index + 1 < document.cached_editable_order.size())
        return TextSelection::caret({document.cached_editable_order[*index + 1], 0});
    if (!after && *index > 0) {
        const auto owner = document.cached_editable_order[*index - 1];
        const auto* block = find_document_block(document, owner);
        if (auto range = block ? editable_block_source_range(*block) : std::nullopt)
            return TextSelection::caret({owner, range->end, TextAffinity::Upstream});
    }
    return std::nullopt;
}

inline std::optional<TextSelection> document_table_step(
    const EditorDocument& document, const TextSelection& selection, int row_step, int column_step, bool extend) {
    if (!selection.selects_cells()) return std::nullopt;
    auto active = document_table_cell_address(document, selection.active.container_id);
    if (!active) return std::nullopt;
    auto const* table = find_document_block(document, active->table_id);
    if (!table || table->children.empty()) return std::nullopt;
    const auto* metadata = table->table_special().metadata.get();
    const auto rows = metadata && metadata->folded && active->row < metadata->header_rows
        ? (std::min)(table->children.size(), metadata->header_rows) : table->children.size();
    const auto columns = table->children.front().children.size();
    TableCoordinate target{active->row, active->column};
    if (const auto* merge = table_merge_at(metadata, target, rows, columns)) {
        if (row_step > 0) target.row = merge->last.row;
        if (column_step > 0) target.column = merge->last.column;
    }
    if (!extend && ((row_step < 0 && target.row == 0) || (row_step > 0 && target.row + 1 == rows)))
        if (auto adjacent = table_adjacent_text(document, *table, row_step > 0)) return adjacent;
    auto move = [](std::size_t from, int step, std::size_t maximum) {
        if (step < 0 && from > 0) return from - 1;
        if (step > 0 && from + 1 < maximum) return from + 1;
        return from;
    };
    target = {move(target.row, row_step, rows), move(target.column, column_step, columns)};
    if (const auto* merge = table_merge_at(metadata, target, rows, columns)) target = merge->first;
    const auto id = table->children[target.row].children[target.column].id;
    TextPosition destination{id, 0, TextAffinity::Downstream};
    return TextSelection::cells(extend ? selection.anchor : destination, destination);
}

inline std::optional<TextSelection> document_table_text_selection(
    const EditorDocument& document, const TextSelection& selection) {
    if (!selection.selects_cells()) return std::nullopt;
    return document_table_cell_contents(document, selection.active);
}

inline std::optional<TextPosition> document_table_enter_target(
    const EditorDocument& document, const TextSelection& selection) {
    const auto cell = document_table_edit_target(document,
        selection.selects_cells() ? TextSelection::caret(selection.active) : selection);
    if (!cell) return std::nullopt;
    const auto* table = find_document_block(document, cell->table_id);
    if (!table || table->has_html_source()) return std::nullopt;
    if (selection.selects_cells() && table_cell_is_computed(*table, {cell->row, cell->column}))
        return std::nullopt;
    if (auto text = document_table_text_selection(document, selection)) return text->active;
    if (!selection.is_caret()) return selection.active;
    const auto& metadata = table->table_special().metadata;
    const auto columns = table->children.front().children.size();
    for (auto row = cell->row + 1; row < table->children.size(); ++row) {
        const auto& cells = table->children[row].children;
        if (cell->column < cells.size()
            && !table_cell_is_covered(metadata.get(), {row, cell->column}, table->children.size(), columns))
            return TextPosition{cells[cell->column].id, 0, TextAffinity::Downstream};
    }
    return selection.active;
}

inline std::optional<TextSelection> document_table_escape_selection(
    const EditorDocument& document, const TextSelection& selection) {
    const auto cell = document_table_cell_address(document, selection.active.container_id);
    if (!cell) return std::nullopt;
    const auto* table = find_document_block(document, cell->table_id);
    if (!table || table->has_html_source() || table->children.empty()) return std::nullopt;
    if (!selection.selects_cells()) return TextSelection::cells(selection.active, selection.active);
    if (selection.anchor.container_id != selection.active.container_id)
        return TextSelection::cells(selection.active, selection.active);
    if (auto next = table_adjacent_text(document, *table, true)) return next;
    return table_adjacent_text(document, *table, false);
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
        const auto visible = block ? editable_block_source_range(*block) : std::nullopt;
        if (!visible) return std::nullopt;
        intersects_table = intersects_table
            || document_table_cell_address(document, owner).has_value();
        auto start = index == first_index
            ? (std::clamp)(first.source_offset, visible->start, visible->end)
            : visible->start;
        auto end = index == last_index
            ? (std::clamp)(last.source_offset, visible->start, visible->end)
            : visible->end;
        if (first_index == last_index && end < start) std::swap(start, end);
        result.push_back({owner, {start, end}});
    }
    return intersects_table
        ? std::optional<std::vector<TextSpan>>{std::move(result)}
        : std::nullopt;
}

} // namespace folia
