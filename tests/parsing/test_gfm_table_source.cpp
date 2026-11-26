#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <iterator>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ids;

import folia.markdown.ast;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_copy;
import folia.core.document_paste;
import folia.core.document_format;
import folia.core.document_history;
import folia.markdown.dialect;
import folia.core.editor;
import folia.core.command;
import folia.core.table_selection;
import folia.markdown.instrumentation;
import folia.markdown.parser;
import folia.markdown.selection;
import folia.markdown.serializer;
import folia.markdown.table_metadata;
import folia.markdown.text_edit;

import folia.markdown.block_source;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.utf;
#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

namespace {

BlockNode const* find_kind(BlockNode const& block, BlockKind kind) {
    if (block.kind == kind) return &block;
    for (auto const& child : block.children) {
        if (auto const* found = find_kind(child, kind)) return found;
    }
    return nullptr;
}

} // namespace

suite gfm_table_source_tests = [] {
    auto dialect = folia::test::commonmark::gfm_dialect();

    "native_table_rectangular_copy_clear_paste_and_undo"_test = [] {
        Editor editor("| A | B | C |\n| - | - | - |\n| 1 | 2 | 3 |\n| 4 | 5 | 6 |\n"
            "<!-- gogoga-table: {\"bg\":{\"B3\":\"blue\"}} -->");
        auto cell = [&](std::size_t row, std::size_t column) {
            return editor.document().root.children.front().children[row].children[column].id;
        };
        const auto selected = TextSelection::cells({cell(1, 1), 0}, {cell(2, 2), 0});
        editor.set_selection(selected);
        expect(editor.selected_markdown_cps() == U"2\t3\n5\t6");
        auto region = document_table_cell_selection(editor.document(), selected);
        expect(fatal(region.has_value()));
        auto clipboard = document_table_clipboard(editor.document(), selected.active, region->region);
        expect(fatal(clipboard.has_value()));
        expect(clipboard->markdown.contains(U"\"A2\":\"blue\""));
        expect(editor.execute_command({.kind = CommandKind::DeleteSelection}));
        expect(editor.selection() == selected);
        expect(editor.editable_source(cell(1, 0)) == U"1");
        expect(editor.editable_source(cell(2, 1)) == U"");
        expect(editor.undo());
        expect(editor.selection() == selected);
        expect(editor.execute_command({.kind = CommandKind::Paste, .text = U"x"}));
        expect(editor.editable_source(cell(1, 0)) == U"1");
        expect(editor.editable_source(cell(2, 2)) == U"x");
        expect(editor.undo());
        expect(editor.execute_command({.kind = CommandKind::InsertText, .text = U"z"}));
        expect(editor.editable_source(cell(1, 1)) == U"2");
        expect(editor.editable_source(cell(2, 2)) == U"z");
        expect(editor.undo());
        expect(editor.selection() == selected);
    };

    "native_table_fill_extends_series_and_sort_preserves_row_identity"_test = [] {
        Editor editor("| A | B |\n| - | - |\n| 3 | Item 01 |\n| 1 | Item 03 |\n"
            "<!-- gogoga-table: {\"bg\":{\"B2\":\"blue\"}} -->");
        auto cell = [&](std::size_t row, std::size_t column) {
            return editor.document().root.children.front().children[row].children[column].id;
        };
        const auto original = editor.markdown_utf8();
        const auto colored_owner = cell(1, 1);
        editor.set_selection(TextSelection::cells({cell(1, 0), 0}, {cell(1, 0), 0}));
        expect(editor.execute_command({.kind = CommandKind::SortTableRows}));
        expect(cell(2, 1) == colored_owner);
        expect(editor.markdown_utf8().contains("\"B3\":\"blue\""));
        expect(document_table_cell_address(editor.document(), colored_owner)->row == 2_u);
        expect(editor.undo());
        expect(editor.markdown_utf8() == original);
        editor.set_selection(TextSelection::cells({cell(1, 1), 0}, {cell(2, 1), 0}));
        expect(editor.execute_command({.kind = CommandKind::FillTableSelection,
            .table_fill_target = TableCoordinate{4, 1}}));
        expect(editor.editable_source(cell(3, 1)) == U"Item 05");
        expect(editor.editable_source(cell(4, 1)) == U"Item 07");
        expect(editor.undo());
        expect(editor.markdown_utf8() == original);
    };

    "native_table_calculations_update_cells_and_totals_in_the_user_transaction"_test = [] {
        Editor editor("| Price | Qty | Cost |\n| - | - | - |\n| 3 | 2 | |\n| 7 | 2 | |");
        auto cell = [&](std::size_t row, std::size_t column) {
            return editor.document().root.children.front().children[row].children[column].id;
        };
        editor.set_caret({cell(0, 2), 0});
        expect(editor.execute_command({.kind = CommandKind::SetTableAppearance,
            .table_format = TableFormatEdit{.kind = TableFormatKind::Formula, .text = U"{Price}*{Qty}"}}));
        expect(editor.editable_source(cell(1, 2)) == U"6");
        expect(editor.editable_source(cell(2, 2)) == U"14");
        expect(editor.execute_command({.kind = CommandKind::SetTableAppearance,
            .table_format = TableFormatEdit{.kind = TableFormatKind::Total, .total = TableTotal::Sum}}));
        expect(editor.editable_source(cell(3, 2)) == U"20");
        editor.set_selection({{cell(1, 0), 0}, {cell(1, 0), 1}});
        expect(editor.execute_command({.kind = CommandKind::InsertText, .text = U"5"}));
        expect(editor.editable_source(cell(1, 2)) == U"10");
        expect(editor.editable_source(cell(3, 2)) == U"24");
        expect(editor.undo());
        expect(editor.editable_source(cell(1, 2)) == U"6");
        expect(editor.editable_source(cell(3, 2)) == U"20");
        expect(editor.redo());
        expect(editor.editable_source(cell(3, 2)) == U"24");
        editor.set_selection({{cell(0, 0), 0}, {cell(0, 0), 5}});
        expect(editor.execute_command({.kind = CommandKind::InsertText, .text = U"Rate"}));
        expect(editor.editable_source(cell(1, 2)) == U"10");
        expect(editor.markdown_utf8().contains("{Rate}*{Qty}"));
        editor.set_caret({cell(0, 0), 0});
        expect(!editor.execute_command({.kind = CommandKind::SetTableAppearance,
            .table_format = TableFormatEdit{.kind = TableFormatKind::Formula, .text = U"{Cost}"}}));
        expect(editor.undo());
        expect(editor.markdown_utf8().contains("{Price}*{Qty}"));
        expect(editor.editable_source(cell(0, 0)) == U"Price");
    };

    "native_table_html_clipboard_and_rich_grid_metadata"_test = [] {
        const auto converted = table_clipboard_html_table(
            U"<div><table><tr><th>A</th><th>B</th></tr><tr><td>x</td><td>y</td></tr></table></div>");
        expect(fatal(converted.has_value()));
        expect(*converted == U"| A | B |\n| --- | --- |\n| x | y |");
        expect(!table_clipboard_html_table(
            U"<p>Caption</p><table><tr><td>A</td><td>B</td></tr><tr><td>x</td><td>y</td></tr></table>"));
        Editor editor("| a | b | c |\n| - | - | - |\n| 1 | 2 | 3 |");
        const auto start = editor.document().root.children.front().children[1].children[1].id;
        editor.set_selection(TextSelection::cells({start, 0}, {start, 0}));
        expect(editor.execute_command({.kind = CommandKind::Paste,
            .text = U"| A | B |\n| - | - |\n| x | y |\n"
                    U"<!-- gogoga-table: {\"w\":[70,85],\"bg\":{\"A2\":\"blue\"},\"merge\":[\"A2:B2\"]} -->"}));
        const auto& table = editor.document().root.children.front();
        const auto& metadata = table.table_special().metadata;
        expect(fatal(metadata && table.children.size() == 3_u));
        expect(metadata->widths[1] == 70.0_f);
        expect(metadata->cells.at({2, 1}).background == TableColor::Blue);
        expect(metadata->merges.front() == TableRegion{{2, 1}, {2, 2}});
        expect(editor.undo());
        expect(editor.markdown_utf8() == "| a | b | c |\n| - | - | - |\n| 1 | 2 | 3 |");
    };

    "native_table_keyboard_ranges_leave_table_and_append_rows"_test = [] {
        Editor editor("before\n\n| A | B |\n| - | - |\n| x | y |\n\nafter");
        const auto before = editor.document().root.children.front().id;
        const auto header = editor.document().root.children[1].children.front().children.front().id;
        const auto last = editor.document().root.children[1].children.back().children.back().id;
        editor.set_selection(TextSelection::cells({header, 0}, {header, 0}));
        expect(editor.execute_command(Command::MoveRight(true)));
        expect(document_table_cell_selection(editor.document(), editor.selection())->region.last.column == 1_u);
        auto collapsed = document_table_escape_selection(editor.document(), editor.selection());
        expect(fatal(collapsed.has_value()));
        expect(collapsed->selects_cells() && collapsed->anchor == collapsed->active);
        expect(editor.execute_command(Command::MoveUp(false)));
        expect(editor.selection().active.container_id == before);
        editor.set_selection(TextSelection::cells({last, 0}, {last, 0}));
        expect(editor.execute_command({.kind = CommandKind::TabForward}));
        const auto destination = document_table_cell_address(editor.document(), editor.selection().active.container_id);
        expect(fatal(destination.has_value()));
        expect(destination->row == 2_u);
        expect(destination->column == 0_u);
        expect(editor.undo());
        expect(editor.selection() == TextSelection::cells({last, 0}, {last, 0}));
    };

    "gogoga_table_metadata_roundtrips_and_moves_with_columns"_test = [dialect] {
        const std::string source =
            "| a | b | c |\r\n| - | - | - |\r\n| 1 | 2 | 3 |\r\n"
            "<!-- gogoga-table: { \"w\":[120,null,200],\"bg\":{\"B2\":\"yellow\"},"
            "\"merge\":[\"B2:C2\"],\"style\":\"zebra\",\"unknown\":{\"value\":\"x\\u007cy\"} } -->\r\n"
            "\r\nafter\r\n";
        auto parsed = parse_text(500, source, dialect);
        const auto* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(fatal(table && table->table_special().metadata));
        const auto meta = table->table_special().metadata;
        expect(meta->widths[0] == 120.0_f);
        expect(meta->cells.at({1, 1}).background == TableColor::Yellow);
        expect(meta->preset == TablePreset::Zebra);
        expect(serialize_markdown(parsed.document) == source);
        const auto selection = TextSelection::caret({table->children[1].children[0].id, 0, TextAffinity::Downstream});
        auto transaction = document_edit_table(parsed.document, selection, DocumentTableEdit::InsertColumnLeft);
        expect(fatal(transaction.has_value()));
        table = find_kind(parsed.document.root, BlockKind::Table);
        const auto changed = table->table_special().metadata;
        expect(changed->widths[1] == 120.0_f);
        expect(changed->cells.at({1, 2}).background == TableColor::Yellow);
        expect(changed->merges.front().first.column == 2_u);
        expect(changed->merges.front().last.column == 3_u);
        expect(serialize_markdown(parsed.document).find("\"unknown\":{\"value\":\"x\\u007cy\"}") != std::string::npos);
        DocumentHistory history;
        history.push(*transaction);
        auto cursor = transaction->selection_after;
        expect(history.undo(parsed.document, cursor));
        expect(serialize_markdown(parsed.document) == source);
        expect(history.redo(parsed.document, cursor));
        expect(find_kind(parsed.document.root, BlockKind::Table)->table_special().metadata == changed);
    };

    "gogoga_table_metadata_roundtrips_with_container_prefix_and_invalid_comments"_test = [dialect] {
        constexpr std::string_view cases[] = {
            "> | a | b |\n> | - | - |\n> | x | y |\n> <!-- gogoga-table: {\"head\":2} -->",
            "- | a | b |\r\n  | - | - |\r\n  <!-- gogoga-table: {\"density\":\"compact\"} -->\r\n",
            "| a | b |\n| - | - |\n<!-- gogoga-table: {bad} -->\n",
            "| a | b |\n| - | - |\n\n<!-- gogoga-table: {\"head\":2} -->\n",
        };
        for (std::size_t index = 0; index < std::size(cases); ++index) {
            auto parsed = parse_text(501 + index, std::string(cases[index]), dialect);
            auto const* table = find_kind(parsed.document.root, BlockKind::Table);
            expect(fatal(table != nullptr));
            expect(static_cast<bool>(table->table_special().metadata) == (index < 2));
            expect(serialize_markdown(parsed.document) == cases[index]);
        }
    };

    "gogoga_table_column_width_writes_interoperable_metadata_without_reformatting_rows"_test = [dialect] {
        const std::string source = " a   | b \r\n:--- | ---:\r\n x   | y  ";
        auto parsed = parse_text(510, source, dialect);
        auto const* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(fatal(table != nullptr));
        const auto cell = table->children.front().children.back().id;
        const auto selection = TextSelection::caret({cell, 0, TextAffinity::Downstream});
        auto transaction = document_edit_table(parsed.document, selection,
            DocumentTableEdit::SetColumnWidth, TableAlignment::None, 145);
        expect(fatal(transaction.has_value()));
        expect(serialize_markdown(parsed.document) == source + "\r\n<!-- gogoga-table: {\"w\":[null,145]} -->");
    };

    "gogoga_table_clipboard_transfers_metadata_and_pastes_grids_as_one_edit"_test = [dialect] {
        const std::string source =
            "| a | b |\n| - | - |\n| π | **z** |\n<!-- gogoga-table: {\"style\":\"zebra\",\"bg\":{\"A2\":\"blue\"}} -->";
        auto parsed = parse_text(515, source, dialect);
        auto* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(fatal(table != nullptr));
        auto position = TextPosition{table->children[1].children[1].id, 0, TextAffinity::Downstream};
        const auto clipboard = document_table_clipboard(parsed.document, position);
        expect(fatal(clipboard.has_value()));
        expect(cps_to_utf8(clipboard->markdown) == source);
        expect(table_clipboard_markdown(clipboard->html) == clipboard->markdown);
        expect(clipboard->html.contains(U"<strong>z</strong>"));
        expect(clipboard->html.contains(U";background-color:#"));

        reset_core_operation_counters();
        auto transaction = document_paste_text(parsed.document, TextSelection::caret(position),
            U"\"a\nb\"\tc|d\r\nx\t\"\"\"q\"\"\"");
        expect(fatal(transaction.has_value()));
        const auto counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0u);
        expect(counters.full_document_serializations == 0u);
        table = find_kind(parsed.document.root, BlockKind::Table);
        expect(table->children.size() == 3_u);
        expect(table->children[1].children.size() == 3_u);
        expect(table->children[1].children[1].inline_content().source == U"a<br>b");
        expect(table->children[1].children[2].inline_content().source == U"c\\|d");
        expect(table->children[2].children[2].inline_content().source == U"\"q\"");
        DocumentHistory history;
        history.push(*transaction);
        auto selection = transaction->selection_after;
        expect(history.undo(parsed.document, selection));
        expect(serialize_markdown(parsed.document) == source);
        expect(history.redo(parsed.document, selection));
        expect(find_kind(parsed.document.root, BlockKind::Table)->children.size() == 3_u);
    };

    "gfm_table_rows_continue_without_pipes_and_stop_at_blocks"_test = [dialect] {
        for (const auto& suffix : {std::string("\ntext\n"), std::string("> quote\n"),
                std::string("2. item\n"), std::string("-\n"), std::string("# heading\n")}) {
            const auto source = std::string("| a | b |\n| - | - |\nplain\n") + suffix;
            const auto parsed = parse_text(130, source, dialect);
            const auto* table = find_kind(parsed.document.root, BlockKind::Table);
            expect(fatal(table != nullptr));
            expect(table->children.size() == 2_u);
            expect(serialize_markdown(parsed.document) == source);
        }
        const std::string source = "first\nsecond\n| a | b |\n| - | - |\nrow\n";
        const auto parsed = parse_text(131, source, dialect);
        expect(parsed.document.root.children.size() == 2_u);
        expect(find_kind(parsed.document.root, BlockKind::Table) != nullptr);
        expect(serialize_markdown(parsed.document) == source);
    };

    "gfm_table_source_roundtrips_official_save_failures"_test = [dialect] {
        constexpr std::string_view cases[] = {
            "| abc | defghi |\n:-: | -----------:\nbar | baz\n",
            "| f\\|oo  |\n| ------ |\n| b `\\|` az |\n| b **\\|** im |\n",
            "| abc | def |\n| --- | --- |\n| bar |\n| bar | baz | boo |\n",
        };
        for (std::size_t index = 0; index < std::size(cases); ++index) {
            auto parsed = parse_text(100 + index, std::string(cases[index]), dialect);
            expect(serialize_markdown(parsed.document) == cases[index]);
        }
    };

    "gfm_table_source_roundtrips_nested_prefixes_tabs_and_crlf"_test = [dialect] {
        constexpr std::string_view cases[] = {
            "> | a\t | b |\r\n> | --- | --- |\r\n> x  | y\t\r\n",
            "- | a\t | b |\n  | --- | --- |\n  x  | y\t\n",
        };
        for (std::size_t index = 0; index < std::size(cases); ++index) {
            auto parsed = parse_text(110 + index, std::string(cases[index]), dialect);
            expect(serialize_markdown(parsed.document) == cases[index]);
        }
    };

    "gfm_table_cell_edit_history_keeps_padding_without_full_document_work"_test = [dialect] {
        constexpr std::string_view source = "| head  |\r\n| ----- |\r\n| b `\\|` az  |\r\n";
        auto parsed = parse_text(120, std::string(source), dialect);
        auto const* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(table != nullptr);
        if (!table || table->children.size() < 2 || table->children[1].children.empty()) return;
        auto const* cell = &table->children[1].children[0];
        const auto old = serialize_markdown(parsed.document);
        reset_core_operation_counters();
        auto transaction = document_insert_text(parsed.document,
            TextSelection::caret({cell->id, cell->inline_content().source.size(), TextAffinity::Downstream}), U"!");
        expect(transaction.has_value());
        if (!transaction) return;
        auto counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0u);
        expect(counters.full_document_serializations == 0u);
        expect(serialize_markdown(parsed.document).find("| b `\\|` az!  |") != std::string::npos);

        DocumentHistory history;
        auto selection = transaction->selection_after;
        history.push(*transaction);
        reset_core_operation_counters();
        expect(history.undo(parsed.document, selection));
        counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0u);
        expect(counters.full_document_serializations == 0u);
        expect(serialize_markdown(parsed.document) == old);
        reset_core_operation_counters();
        expect(history.redo(parsed.document, selection));
        counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0u);
        expect(counters.full_document_serializations == 0u);
        expect(serialize_markdown(parsed.document).find("| b `\\|` az!  |") != std::string::npos);
    };

    "gfm_table_empty_column_insert_undo_regenerates_changed_rows"_test = [dialect] {
        constexpr std::string_view source = "| head | second |\n| --- | --- |\n| one |\n";
        auto parsed = parse_text(121, std::string(source), dialect);
        auto const* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(table != nullptr);
        if (!table || table->children.size() < 2) return;
        const auto before = serialize_markdown(parsed.document);
        const auto cell_id = table->children[1].children.back().id;
        auto transaction = document_edit_table(parsed.document,
            TextSelection::caret({cell_id, 0, TextAffinity::Downstream}),
            DocumentTableEdit::InsertColumnRight);
        expect(transaction.has_value());
        if (!transaction) return;
        expect(serialize_markdown(parsed.document) != before);
        DocumentHistory history;
        auto selection = transaction->selection_after;
        history.push(*transaction);
        expect(history.undo(parsed.document, selection));
        expect(serialize_markdown(parsed.document) == before);
        expect(history.redo(parsed.document, selection));
        expect(serialize_markdown(parsed.document) != before);
    };

    "gfm_table_row_and_column_undo_restore_exact_source"_test = [dialect] {
        constexpr std::string_view source = "| a  | b |\n| --- | --- |\n| c  | d\t|\n| e | f |\n";
        auto parsed = parse_text(122, std::string(source), dialect);
        auto const* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(table != nullptr);
        if (!table || table->children.size() < 3) return;
        const auto before = serialize_markdown(parsed.document);
        const auto first_data_cell = table->children[1].children[0].id;
        const auto second_data_cell = table->children[1].children[1].id;
        auto run_and_undo = [&](NodeId cell_id, DocumentTableEdit edit) {
            auto transaction = document_edit_table(parsed.document,
                TextSelection::caret({cell_id, 0, TextAffinity::Downstream}), edit);
            expect(transaction.has_value());
            if (!transaction) return;
            DocumentHistory history;
            auto selection = transaction->selection_after;
            history.push(*transaction);
            expect(history.undo(parsed.document, selection));
            expect(serialize_markdown(parsed.document) == before);
        };
        run_and_undo(first_data_cell, DocumentTableEdit::InsertRowBelow);
        run_and_undo(first_data_cell, DocumentTableEdit::DeleteRow);
        run_and_undo(second_data_cell, DocumentTableEdit::DeleteColumn);
    };

    "gfm_table_canonical_format_clears_row_source_and_undo_restores_it"_test = [dialect] {
        constexpr std::string_view source = "|a  | b|\n|---|---|\n| c  |d |";
        auto parsed = parse_text(123, std::string(source), dialect);
        auto const* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(table != nullptr);
        if (!table || table->children.empty() || table->children.front().children.empty()) return;
        const auto cell_id = table->children.front().children.front().id;
        const auto before = serialize_markdown(parsed.document);
        auto transaction = document_format_markdown(parsed.document,
            TextSelection::caret({cell_id, 0, TextAffinity::Downstream}));
        expect(transaction.has_value());
        if (!transaction) return;
        expect(serialize_markdown(parsed.document) != before);
        auto const* formatted_table = find_kind(parsed.document.root, BlockKind::Table);
        expect(formatted_table != nullptr);
        if (!formatted_table) return;
        for (const auto& row : formatted_table->children)
            expect(!row.table_special().row_syntax.has_value());
        DocumentHistory history;
        auto selection = transaction->selection_after;
        history.push(*transaction);
        expect(history.undo(parsed.document, selection));
        expect(serialize_markdown(parsed.document) == before);
        auto const* restored_table = find_kind(parsed.document.root, BlockKind::Table);
        expect(restored_table != nullptr);
        if (restored_table)
            for (const auto& row : restored_table->children)
                expect(row.table_special().row_syntax.has_value());
    };

    "gfm_table_source_owner_ids_remap_during_fresh_id_paste"_test = [dialect] {
        constexpr std::string_view source = "| a  | b |\n| --- | --- |\n| c | d |";
        Editor editor("", dialect);
        const auto transaction = editor.execute_document_paste_text(
            editor.selection(), std::u32string(source.begin(), source.end()));
        expect(transaction.has_value());
        if (!transaction) return;
        expect(editor.markdown_utf8() == source);
        auto const* table = find_kind(editor.document().root, BlockKind::Table);
        expect(table != nullptr);
        if (!table) return;
        for (const auto& row : table->children) {
            const auto& syntax = row.table_special().row_syntax;
            expect(syntax.has_value());
            if (!syntax) continue;
            expect(syntax->cell_owners.size() == row.children.size());
            for (std::size_t index = 0; index < syntax->cell_owners.size(); ++index)
                expect(syntax->cell_owners[index] == row.children[index].id);
        }
    };
};
