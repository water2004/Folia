#include <cstddef>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.core.command;
import folia.markdown.dialect;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_ids;
import folia.markdown.document_text;
import folia.core.editor;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.core.input;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.core.theme;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

suite editing_tables_structure_transactions_tests = [] {

"table_navigation_changes_selection_without_creating_history"_test = [] {
    Editor editor("| A | B |\n| --- | --- |\n| 1 | 2 |");
    const auto& table = editor.document().root.children.front();
    const auto first_id = table.children.front().children.front().id;
    const auto second_id = table.children.front().children[1].id;
    editor.set_selection(TextSelection::caret({first_id, 0, TextAffinity::Downstream}));
    const auto had_undo = editor.has_undo();
    reset_core_operation_counters();
    expect(fatal(bool(editor.execute_document_table_edit(editor.selection(), DocumentTableEdit::MoveCellNext).has_value())));
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(editor.selection().active.container_id == second_id)));
    expect(fatal(bool(editor.has_undo() == had_undo)));
};

"tab_from_the_final_cell_appends_one_reversible_row"_test = [] {
    Editor editor("| A | B |\n| --- | --- |\n| 1 | 2 |");
    const auto& table = editor.document().root.children.front();
    const auto last_id = table.children.back().children.back().id;
    const auto before = TextSelection::caret(
        {last_id, 1, TextAffinity::Downstream});
    editor.set_selection(before);

    auto transaction = editor.execute_document_table_edit(
        editor.selection(), DocumentTableEdit::MoveCellNext);
    expect(fatal(bool(transaction.has_value())));
    if (!transaction) return;
    expect(fatal(bool(!transaction->operations.empty())));
    expect(fatal(bool(editor.document().root.children.front().children.size() == 3u)));
    const auto& appended = editor.document().root.children.front().children.back();
    expect(fatal(bool(appended.children.size() == 2u)));
    expect(fatal(bool(editor.selection().active.container_id
        == appended.children.front().id)));
    expect(fatal(bool(editor.selection().active.source_offset == 0u)));

    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.document().root.children.front().children.size() == 2u)));
    expect(fatal(bool(editor.selection() == before)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.document().root.children.front().children.size() == 3u)));
};

"table_structure_commands_reject_cross_cell_text_ranges"_test = [] {
    Editor editor("| aa | bb |\n| --- | --- |\n| cc | dd |");
    const auto fragments = document_text_fragments(editor.document());
    expect(fatal(bool(fragments.size() == 4u)));
    if (fragments.size() != 4) return;
    const TextSelection cross_cell{
        {fragments[0].container_id, 1, TextAffinity::Downstream},
        {fragments[1].container_id, 1, TextAffinity::Upstream}};
    editor.set_selection(cross_cell);
    const auto before = editor.markdown_utf8();
    const auto revision = editor.revision();
    expect(fatal(!editor.execute_document_table_edit(
        editor.selection(), DocumentTableEdit::InsertRowBelow).has_value()));
    expect(fatal(bool(editor.markdown_utf8() == before)));
    expect(fatal(bool(editor.revision() == revision)));
    expect(fatal(bool(editor.selection() == cross_cell)));

    const TextSelection same_cell{
        {fragments[0].container_id, 0, TextAffinity::Downstream},
        {fragments[0].container_id, 1, TextAffinity::Upstream}};
    editor.set_selection(same_cell);
    expect(fatal(bool(editor.execute_document_table_edit(
        editor.selection(), DocumentTableEdit::InsertRowBelow).has_value())));
};

"table_structure_edits_record_rows_columns_and_payloads"_test = [] {
    struct Case {
        DocumentTableEdit edit;
        std::size_t row;
        std::size_t column;
        TableAlignment alignment = TableAlignment::None;
        std::size_t argument = 0;
    };
    const std::vector<Case> cases{
        {DocumentTableEdit::InsertRowBelow, 1, 0},
        {DocumentTableEdit::DeleteRow, 1, 0},
        {DocumentTableEdit::MoveRowDown, 1, 0},
        {DocumentTableEdit::MoveRowTo, 1, 0, TableAlignment::None, 2},
        {DocumentTableEdit::InsertColumnRight, 1, 0},
        {DocumentTableEdit::DeleteColumn, 1, 0},
        {DocumentTableEdit::MoveColumnRight, 1, 0},
        {DocumentTableEdit::MoveColumnTo, 1, 0, TableAlignment::None, 1},
        {DocumentTableEdit::SetColumnAlignment, 1, 0, TableAlignment::Center},
        {DocumentTableEdit::Normalize, 1, 0},
    };
    for (const auto& test : cases) {
        Editor editor("| A | B |\n| --- | --- |\n| 1 | 2 |\n| 3 | 4 |");
        const auto& table = editor.document().root.children.front();
        const auto owner = table.children[test.row].children[test.column].id;
        const auto before = TextSelection::caret({owner, 0, TextAffinity::Downstream});
        const auto before_markdown = editor.markdown_utf8();
        editor.set_selection(before);
        reset_core_operation_counters();
        auto transaction = editor.execute_document_table_edit(
            editor.selection(), test.edit, test.alignment, test.argument);
        expect(fatal(bool(transaction.has_value()))) << static_cast<int>(test.edit);
        if (!transaction) continue;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << static_cast<int>(test.edit);
        expect(fatal(bool(counters.full_document_serializations == 0u))) << static_cast<int>(test.edit);
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << static_cast<int>(test.edit);
        expect(fatal(bool(!transaction->operations.empty()))) << static_cast<int>(test.edit);
        const auto after_markdown = editor.markdown_utf8();
        const auto after = editor.selection();
        expect(fatal(bool(editor.undo()))) << static_cast<int>(test.edit);
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << static_cast<int>(test.edit);
        expect(fatal(bool(editor.selection() == before))) << static_cast<int>(test.edit);
        expect(fatal(bool(editor.redo()))) << static_cast<int>(test.edit);
        expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << static_cast<int>(test.edit);
        expect(fatal(bool(editor.selection() == after))) << static_cast<int>(test.edit);
    }
};

"table_line_break_is_one_semantic_delete_unit"_test = [] {
    Editor editor("| abcd |\n| --- |");
    const auto owner = first_text(editor).id;
    editor.set_selection({
        {owner, 1, TextAffinity::Downstream},
        {owner, 3, TextAffinity::Downstream},
    });
    expect(fatal(bool(editor.execute_document_enter(editor.selection()).has_value())));
    expect(fatal(bool(*editor.editable_source(owner) == U"a<br>d")));
    expect(fatal(bool(editor.selection().active.source_offset == 5u)));

    reset_core_operation_counters();
    expect(fatal(bool(editor.execute_document_delete_backward(editor.selection()).has_value())));
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(counters.inline_reparses == 1u)));
    expect(fatal(bool(*editor.editable_source(owner) == U"ad")));
    expect(fatal(bool(editor.selection().active.source_offset == 1u)));
};

};
