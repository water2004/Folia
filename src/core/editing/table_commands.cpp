module folia.core.table_commands;
import std;
import folia.core.table_values;
import folia.core.table_selection;
import folia.core.document_paste;
import folia.core.document_operation_apply;
import folia.core.document_edit_primitives;
import folia.core.document_block_edit;
import folia.markdown.ast;
import folia.markdown.inline_document;

namespace folia {

std::optional<DocumentTransaction> document_sort_table(
    EditorDocument& document, const TextSelection& selection, bool descending) {
    const auto target = document_table_cell_address(document, selection.active.container_id);
    if (!target) return std::nullopt;
    auto* table = find_document_block(document, target->table_id);
    if (!table || table->has_html_source() || table->children.size() < 3) return std::nullopt;
    const auto& metadata = table->table_special().metadata;
    const auto headers = metadata ? metadata->header_rows : 1;
    const auto total = metadata && std::ranges::any_of(metadata->totals, [](auto kind) { return kind != TableTotal::None; });
    const auto end = table->children.size() - total;
    if (end <= headers + 1) return std::nullopt;
    if (metadata && std::ranges::any_of(metadata->merges, [&](const auto& merge) {
        return merge.first.row != merge.last.row && merge.last.row >= headers && merge.first.row < end;
    })) return std::nullopt;
    struct Key {
        std::u32string_view text;
        std::optional<double> number;
        std::optional<TableDate> date;
        std::size_t row;
    };
    std::vector<Key> keys;
    keys.reserve(end - headers);
    for (auto row = headers; row < end; ++row) {
        auto text = trim_table_value(table->children[row].children[target->column].inline_content().source);
        keys.push_back({text, table_number(text, true), table_date(text), row});
    }
    std::ranges::stable_sort(keys, [&](const Key& left, const Key& right) {
        if (left.text.empty() != right.text.empty()) return !left.text.empty();
        // A single total ordering also keeps mixed numbers and text sortable.
        const auto group = [](const Key& key) { return key.number ? 0 : key.date ? 1 : 2; };
        const auto compare = group(left) != group(right) ? group(left) <=> group(right)
            : left.number ? (*left.number < *right.number ? std::strong_ordering::less
                : *left.number > *right.number ? std::strong_ordering::greater : std::strong_ordering::equal)
            : left.date ? left.date->date <=> right.date->date
            : left.text <=> right.text;
        return descending ? compare > 0 : compare < 0;
    });
    DocumentTreeEdit reorder;
    reorder.kind = DocumentTreeEditKind::ReorderChildren;
    reorder.parent_id = table->id;
    reorder.index = headers;
    reorder.order.resize(keys.size());
    bool changed = false;
    for (std::size_t destination = 0; destination < keys.size(); ++destination) {
        const auto source = keys[destination].row - headers;
        reorder.order[source] = destination;
        changed |= destination != source;
    }
    if (!changed) return std::nullopt;
    const auto revision = document.revision;
    auto displaced_metadata = reorder_table_metadata_rows(metadata, headers, reorder.order);
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision);
    operations.emplace_back(std::move(reorder));
    if (!apply_document_operation(document, operations.back(), true)) {
        operations.pop_back();
        return std::nullopt;
    }
    if (displaced_metadata != metadata) {
        DocumentTreeEdit payload;
        payload.kind = DocumentTreeEditKind::UpdatePayload;
        payload.before = document_transaction_detail::payload_shell(*table);
        table->ensure_table_special().metadata = std::move(displaced_metadata);
        payload.after = document_transaction_detail::payload_shell(*table);
        operations.emplace_back(std::move(payload));
    }
    ++document.revision;
    rollback.commit();
    return make_recorded_document_transaction(std::move(operations), selection, selection,
        revision, document.revision, DocumentTransactionReason::Structure);
}

std::optional<DocumentTransaction> document_fill_table(
    EditorDocument& document, const TextSelection& selection, TableCoordinate destination, bool copy) {
    const auto selected = document_table_cell_selection(document, selection);
    if (!selected || selected->region.contains(destination)) return std::nullopt;
    auto const* table = find_document_block(document, selected->table);
    if (!table) return std::nullopt;
    auto region = selected->region;
    const auto rows = table->children.size();
    const auto columns = table->children.front().children.size();
    const auto distance = [](std::size_t item, std::size_t first, std::size_t last) {
        return item < first ? first - item : item > last ? item - last : 0;
    };
    auto dy = distance(destination.row, region.first.row, region.last.row);
    auto dx = distance(destination.column, region.first.column, region.last.column);
    // Pointer drag can grow a table. Bound only the arithmetic range here,
    // then allocate the cells requested by the caller in the same undo step.
    if (dy > rows + 4096 || dx > columns + 4096) return std::nullopt;
    const auto vertical = dy >= dx;
    const auto* metadata = table->table_special().metadata.get();
    const auto headers = metadata ? metadata->header_rows : 1;
    if (vertical && region.first.row >= headers && destination.row < headers) destination.row = headers;
    const auto backwards = vertical ? destination.row < region.first.row : destination.column < region.first.column;
    auto target = region.first;
    const auto count = vertical ? distance(destination.row, region.first.row, region.last.row) : dx;
    if (count == 0) return std::nullopt;
    if (vertical) target.row = backwards ? destination.row : region.last.row + 1;
    else target.column = backwards ? destination.column : region.last.column + 1;
    const auto height = region.last.row - region.first.row + 1;
    const auto width = region.last.column - region.first.column + 1;
    document_paste_detail::ClipboardGrid cells(vertical ? count : height,
        std::vector<std::u32string>(vertical ? width : count));
    std::vector<std::u32string_view> series;
    for (std::size_t line = 0; line < (vertical ? width : height); ++line) {
        series.clear();
        for (std::size_t index = 0; index < (vertical ? height : width); ++index) {
            auto row = region.first.row + (vertical ? index : line);
            auto column = region.first.column + (vertical ? line : index);
            series.emplace_back(table->children[row].children[column].inline_content().source);
        }
        auto values = table_fill_series(series, count, backwards, copy);
        for (std::size_t index = 0; index < count; ++index) {
            auto slot = backwards ? count - index - 1 : index;
            cells[vertical ? slot : line][vertical ? line : slot] = std::move(values[index]);
        }
    }
    auto result = document_paste_detail::paste_table_grid(document, selection, {table->id, {}, {},
        target.row, target.column}, cells);
    if (!result) return std::nullopt;
    table = find_document_block(document, selected->table);
    if (!table) return result;
    auto from = region.first;
    auto to = region.last;
    if (vertical) (backwards ? from.row : to.row) = destination.row;
    else (backwards ? from.column : to.column) = destination.column;
    result->selection_after = TextSelection::cells({table->children[from.row].children[from.column].id, 0},
        {table->children[to.row].children[to.column].id, 0});
    return result;
}

std::optional<DocumentTransaction> document_edit_table_cell_text(EditorDocument& document,
    const TextSelection& selection, const std::function<std::optional<DocumentTransaction>(TextSelection)>& edit,
    TextPosition* destination) {
    if (selection.selects_cells()) return edit(selection);
    const auto address = document_table_cell_address(document, selection.active.container_id);
    const auto merged = address ? document_merged_table_cell(document, *address) : std::nullopt;
    if (!merged) return edit(selection);
    const auto& anchor = merged->anchor();
    const auto parts = merged->text_parts();
    if (parts.size() == 1 && parts.front()->id == anchor.id) return edit(selection);

    // Keep the original slots until a content edit succeeds. The fold into
    // the anchor and the caller's local source edit share one undo entry.
    std::u32string source;
    auto mapped = selection;
    auto mapped_destination = destination ? *destination : TextPosition{};
    for (const auto* part : parts) {
        if (!source.empty()) source += U"<br>";
        for (auto* position : {&mapped.anchor, &mapped.active, &mapped_destination}) {
            if (position->container_id != part->id) continue;
            position->container_id = anchor.id;
            position->source_offset += source.size();
        }
        source += part->inline_content().source;
    }
    if (mapped.anchor.container_id != anchor.id || mapped.active.container_id != anchor.id) return edit(selection);

    const auto revision = document.revision;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision);
    document_edit_detail::NodeAllocator allocator(document);
    auto replace = [&](const BlockNode& cell, std::u32string replacement) {
        auto change = document_edit_detail::edit_inline(document, cell.id,
            {0, cell.inline_content().source.size()}, std::move(replacement), allocator);
        if (!change) return false;
        document_edit_detail::append_source_operation(operations, std::move(*change));
        return true;
    };
    if (!replace(anchor, std::move(source))) return std::nullopt;
    for (const auto* part : parts)
        if (part->id != anchor.id && !replace(*part, {})) return std::nullopt;

    if (destination) *destination = mapped_destination;
    auto result = edit(mapped);
    if (!result || result->operations.empty()) return std::nullopt;
    document_edit_detail::MutationRollback edit_rollback(document, result->operations, result->revision_before);
    operations.reserve(operations.size() + result->operations.size());
    operations.insert(operations.end(), std::make_move_iterator(result->operations.begin()),
        std::make_move_iterator(result->operations.end()));
    edit_rollback.commit();
    result->operations = std::move(operations);
    result->selection_before = selection;
    rollback.commit();
    return result;
}

std::optional<DocumentTransaction> document_commit_table_cell(EditorDocument& document,
    const TextSelection& selection, std::optional<std::u32string_view> replacement) {
    auto detail = document_table_cell_address(document, selection.active.container_id);
    if (!detail) return std::nullopt;
    auto* table = find_document_block(document, detail->table_id);
    const auto* cell = find_document_block(document, detail->cell_id);
    if (!table || !cell || table->has_html_source()) return std::nullopt;
    if (!replacement)
        if (const auto merged = document_merged_table_cell(document, *detail)) {
            const auto parts = merged->text_parts();
            if (parts.size() != 1 || parts.front()->id != merged->anchor().id) return std::nullopt;
        }
    const auto& metadata = table->table_special().metadata;
    const auto headers = metadata ? metadata->header_rows : 1;
    const auto body = detail->row >= headers && (!table_has_totals(metadata.get()) || detail->row + 1 < table->children.size());
    if (body && metadata && detail->column < metadata->formulas.size() && !metadata->formulas[detail->column].empty())
        return std::nullopt;
    const auto type = body && metadata && detail->column < metadata->column_types.size()
        ? metadata->column_types[detail->column] : TableColumnType{};
    if (!replacement && type.kind == TableColumnKind::Text) return std::nullopt;
    const auto& source = cell->inline_content().source;
    auto normalized = format_table_value(replacement.value_or(source), type);
    normalized = document_edit_detail::table_cell_input_source({}, 0, normalized);
    const auto revision = document.revision;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision);
    const auto next_length = normalized.size();
    auto updated_metadata = metadata;
    if (type.kind == TableColumnKind::Status || type.kind == TableColumnKind::Multi) {
        auto options = type.kind == TableColumnKind::Multi ? table_tags(normalized)
            : std::vector<std::u32string_view>{trim_table_value(normalized)};
        for (auto option : options) {
            if (option.empty()) continue;
            const auto* known = detail->column < updated_metadata->options.size()
                ? &updated_metadata->options[detail->column] : nullptr;
            if (known && known->contains(option)) continue;
            TableFormatEdit color{
                .kind = TableFormatKind::OptionColor,
                .color = static_cast<TableColor>(known ? known->size() % 7 : 0),
                .text = std::u32string(option)};
            updated_metadata = format_table_metadata(updated_metadata, color,
                {{detail->row, detail->column}, {detail->row, detail->column}}, U"\n");
        }
    }
    if (normalized != source) {
        document_edit_detail::NodeAllocator allocator(document);
        auto change = document_edit_detail::edit_inline(document, cell->id,
            {0, source.size()}, std::move(normalized), allocator);
        if (!change) return std::nullopt;
        document_edit_detail::append_source_operation(operations, std::move(*change));
    }
    if (updated_metadata != metadata) {
        DocumentTreeEdit payload;
        payload.kind = DocumentTreeEditKind::UpdatePayload;
        payload.before = document_transaction_detail::payload_shell(*table);
        table->ensure_table_special().metadata = std::move(updated_metadata);
        payload.after = document_transaction_detail::payload_shell(*table);
        operations.emplace_back(std::move(payload));
    }
    if (operations.empty()) return std::nullopt;
    ++document.revision;
    auto after = selection;
    if (!after.selects_cells())
        after = TextSelection::caret({detail->cell_id, next_length, TextAffinity::Upstream});
    rollback.commit();
    return make_recorded_document_transaction(std::move(operations), selection, after,
        revision, document.revision, DocumentTransactionReason::Replace);
}

std::optional<DocumentTransaction> document_toggle_table_checkbox(
    EditorDocument& document, const TextSelection& selection, bool create) {
    const auto active = document_table_cell_address(document, selection.active.container_id);
    if (!active) return std::nullopt;
    const auto* table = find_document_block(document, active->table_id);
    if (!table || table->has_html_source()) return std::nullopt;
    if (selection.anchor.container_id != selection.active.container_id) return std::nullopt;
    if (table_cell_is_computed(*table, {active->row, active->column})) return std::nullopt;
    const auto* cell = &table->children[active->row].children[active->column];
    if (const auto merged = document_merged_table_cell(document, *active)) cell = merged->text_parts().front();
    const auto& source = cell->inline_content().source;
    const auto checked = table_checkbox(source);
    if (!create && !checked) return std::nullopt;
    SourceRange range{1, 2};
    std::u32string replacement = checked.value_or(false) ? U" " : U"x";
    if (create) {
        if (checked) {
            const auto start = source.find_first_not_of(U" \t\r\n", 3);
            range = {0, start == source.npos ? source.size() : start};
            replacement.clear();
        } else {
            range = {0, 0};
            replacement = source.empty() ? U"[ ]" : U"[ ] ";
        }
    }
    const auto revision = document.revision;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision);
    document_edit_detail::NodeAllocator allocator(document);
    auto edit = document_edit_detail::edit_inline(document, cell->id, range, std::move(replacement), allocator);
    if (!edit) return std::nullopt;
    document_edit_detail::append_source_operation(operations, std::move(*edit));
    ++document.revision;
    auto position = TextPosition{active->cell_id, 0};
    rollback.commit();
    return make_recorded_document_transaction(std::move(operations), selection,
        selection.selects_cells() ? selection : TextSelection::cells(position, position),
        revision, document.revision, DocumentTransactionReason::Replace);
}

std::optional<DocumentTransaction> document_delete_table(
    EditorDocument& document, const TextSelection& selection) {
    const auto active = document_table_cell_address(document, selection.active.container_id);
    if (!active) return std::nullopt;
    const auto position = TextPosition{active->cell_id, 0};
    const auto target = document_table_escape_selection(document, TextSelection::cells(position, position));
    const auto revision = document.revision;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision);
    if (!document_edit_detail::remove_node_recorded(document, active->table_id, operations)) return std::nullopt;
    document_edit_detail::prune_empty_containers_recorded(document.root, operations, document);
    ++document.revision;
    rollback.commit();
    return make_recorded_document_transaction(std::move(operations), selection,
        target.value_or(TextSelection{}), revision, document.revision, DocumentTransactionReason::Structure);
}

}
