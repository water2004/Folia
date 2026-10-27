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

suite editor_table_property_tests = [] {

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

"cross_cell_text_selection_is_row_major_reversible_and_structure_preserving"_test = [] {
    Editor editor("| aa | bb |\n| --- | --- |\n| cc | dd |");
    const auto fragments = document_text_fragments(editor.document());
    expect(fatal(bool(fragments.size() == 4u)));
    if (fragments.size() != 4) return;

    const TextSelection forward{
        {fragments[0].container_id, 1, TextAffinity::Downstream},
        {fragments[3].container_id, 1, TextAffinity::Upstream}};
    const TextSelection reverse{forward.active, forward.anchor};
    editor.set_selection(forward);
    expect(fatal(bool(editor.selected_markdown_cps() == U"a\tbb\ncc\td")));
    editor.set_selection(reverse);
    expect(fatal(bool(editor.selected_markdown_cps() == U"a\tbb\ncc\td")));

    editor.set_selection(forward);
    const auto before = editor.markdown_utf8();
    reset_core_operation_counters();
    auto deletion = editor.execute_document_delete_selection(editor.selection());
    expect(fatal(bool(deletion.has_value())));
    if (!deletion) return;
    auto const counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    auto const& edited = editor.document().root.children.front();
    expect(fatal(bool(edited.kind == BlockKind::Table)));
    expect(fatal(bool(edited.children.size() == 2u)));
    expect(fatal(bool(edited.children[0].children.size() == 2u)));
    expect(fatal(bool(edited.children[1].children.size() == 2u)));
    expect(fatal(bool(edited.children[0].children[0].inline_content.source == U"a")));
    expect(fatal(bool(edited.children[0].children[1].inline_content.source.empty())));
    expect(fatal(bool(edited.children[1].children[0].inline_content.source.empty())));
    expect(fatal(bool(edited.children[1].children[1].inline_content.source == U"d")));
    expect(fatal(bool(validate_document(editor.document()).empty())));

    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == before)));
    expect(fatal(bool(editor.selection() == forward)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == after)));
    expect(fatal(bool(editor.selection() == after_selection)));
};

"table_boundaries_prevent_cross_block_text_delete_from_removing_cells"_test = [] {
    Editor editor(
        "lead\n\n| aa | bb |\n| --- | --- |\n| cc | dd |\n\ntrail");
    const auto fragments = document_text_fragments(editor.document());
    expect(fatal(bool(fragments.size() == 6u)));
    if (fragments.size() != 6) return;
    const TextSelection before{
        {fragments[0].container_id, 2, TextAffinity::Downstream},
        {fragments[2].container_id, 1, TextAffinity::Upstream}};
    editor.set_selection(before);
    expect(fatal(bool(editor.selected_markdown_cps() == U"ad\naa\tb")));
    const auto before_markdown = editor.markdown_utf8();

    auto deletion = editor.execute_document_delete_selection(editor.selection());
    expect(fatal(bool(deletion.has_value())));
    if (!deletion) return;
    expect(fatal(bool(editor.document().root.children.size() == 3u)));
    auto const& table = editor.document().root.children[1];
    expect(fatal(bool(table.kind == BlockKind::Table)));
    expect(fatal(bool(table.children.size() == 2u)));
    expect(fatal(bool(table.children[0].children.size() == 2u)));
    expect(fatal(bool(table.children[1].children.size() == 2u)));
    expect(fatal(bool(editor.document().root.children[0].inline_content.source == U"le")));
    expect(fatal(bool(table.children[0].children[0].inline_content.source.empty())));
    expect(fatal(bool(table.children[0].children[1].inline_content.source == U"b")));
    expect(fatal(bool(table.children[1].children[0].inline_content.source == U"cc")));
    expect(fatal(bool(table.children[1].children[1].inline_content.source == U"dd")));
    expect(fatal(bool(editor.document().root.children[2].inline_content.source == U"trail")));
    expect(fatal(bool(validate_document(editor.document()).empty())));

    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == before_markdown)));
    expect(fatal(bool(editor.selection() == before)));
};

"cross_cell_selection_replacement_is_one_source_transaction"_test = [] {
    Editor editor("| aa | bb |\n| --- | --- |\n| cc | dd |");
    const auto fragments = document_text_fragments(editor.document());
    expect(fatal(bool(fragments.size() == 4u)));
    if (fragments.size() != 4) return;
    const TextSelection selection{
        {fragments[0].container_id, 1, TextAffinity::Downstream},
        {fragments[3].container_id, 1, TextAffinity::Upstream}};
    editor.set_selection(selection);
    const auto before = editor.markdown_utf8();

    auto replacement = editor.execute_document_insert_text(editor.selection(), U"X");
    expect(fatal(bool(replacement.has_value())));
    if (!replacement) return;
    auto const& table = editor.document().root.children.front();
    expect(fatal(bool(table.children.size() == 2u)));
    expect(fatal(bool(table.children[0].children.size() == 2u)));
    expect(fatal(bool(table.children[1].children.size() == 2u)));
    expect(fatal(bool(table.children[0].children[0].inline_content.source == U"aX")));
    expect(fatal(bool(table.children[0].children[1].inline_content.source.empty())));
    expect(fatal(bool(table.children[1].children[0].inline_content.source.empty())));
    expect(fatal(bool(table.children[1].children[1].inline_content.source == U"d")));
    expect(fatal(bool(editor.selection().is_caret())));
    expect(fatal(bool(editor.selection().active.container_id
        == fragments[0].container_id)));
    expect(fatal(bool(editor.selection().active.source_offset == 2u)));
    expect(fatal(bool(validate_document(editor.document()).empty())));

    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == before)));
    expect(fatal(bool(editor.selection() == selection)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == after)));
    expect(fatal(bool(editor.selection() == after_selection)));
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

"derived_symbols_and_outline_refresh_after_structure_edits"_test = [] {
    Editor editor("title");
    editor.set_selection(caret(first_text(editor), 0));
    reset_core_operation_counters();
    expect(fatal(bool(editor.execute_document_set_heading(editor.selection(), 2).has_value())));
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));
    expect(fatal(bool(counters.incremental_document_block_index_repairs == 1u)));
    expect(fatal(bool(counters.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(counters.full_document_outline_derivations == 1u)));
    expect(fatal(document_indexes_are_exact(editor.document())));
    expect(fatal(bool(editor.document().root.children.front().kind == BlockKind::Heading)));
    expect(fatal(bool(editor.symbols().headings.size() == 1u)));
    expect(fatal(bool(editor.outline().items.size() == 1u)));
    expect(fatal(bool(editor.outline().items.front().title_plain_text == "title")));
};

"set_dialect_reloads_explicitly_and_keeps_one_selection"_test = [] {
    MarkdownDialect dialect = default_dialect();
    dialect.math.inline_dollar = false;
    Editor editor("$x$", dialect);
    expect(fatal(bool(!inline_contains_kind(first_text(editor).inline_content, InlineCstKind::InlineMath))));
    editor.set_selection(caret(first_text(editor), 1));
    expect(fatal(bool(editor.execute_document_insert_text(editor.selection(), U"y").has_value())));
    expect(fatal(bool(editor.has_undo())));
    const auto before_reload_revision = editor.revision();
    dialect.math.inline_dollar = true;
    editor.set_dialect(dialect);
    expect(fatal(bool(inline_contains_kind(first_text(editor).inline_content, InlineCstKind::InlineMath))));
    expect(fatal(bool(editor.selection().active.container_id == first_text(editor).id)));
    expect(fatal(bool(editor.revision() == before_reload_revision + 1)));
    expect(fatal(bool(!editor.has_undo())));
    expect(fatal(bool(!editor.has_redo())));
};

"input_events_translate_to_commands_without_mutating_state"_test = [] {
    Editor editor("abc");
    EditorInputEvent text;
    text.kind = EditorInputEvent::Kind::TextInput;
    text.text_input.kind = TextInputEvent::Kind::InsertText;
    text.text_input.text = U"X";
    auto command = editor.handle_input(text);
    expect(fatal(bool(command.has_value())));
    expect(fatal(bool(command->kind == CommandKind::InsertText)));
    expect(fatal(bool(command->text == U"X")));
    expect(fatal(bool(editor.markdown_utf8() == "abc")));

    EditorInputEvent key;
    key.kind = EditorInputEvent::Kind::KeyDown;
    key.key.key_code = KeyCode::Backspace;
    command = editor.handle_input(key);
    expect(fatal(bool(command && command->kind == CommandKind::DeleteBackward)));
};

"selection_validation_rejects_foreign_or_out_of_range_positions"_test = [] {
    Editor editor("abc");
    bool foreign = false;
    try { editor.set_selection(TextSelection::caret({NodeId(999999), 0, TextAffinity::Downstream})); }
    catch (const std::out_of_range&) { foreign = true; }
    expect(fatal(bool(foreign)));
    bool outside = false;
    try { editor.set_selection(caret(first_text(editor), 4)); }
    catch (const std::out_of_range&) { outside = true; }
    expect(fatal(bool(outside)));
};

"all_editable_block_contexts_share_the_source_edit_history_pipeline"_test = [] {
    const std::vector<std::string> cases{
        "abc",
        "# abc",
        "- abc",
        "- [ ] abc",
        "> abc",
        "| abc |\n| --- |",
        "> [!NOTE]\n> abc",
        "[^1]: abc",
        "- > abc",
        "> - abc",
        "- [ ] > abc",
    };
    for (auto const& markdown : cases) {
        Editor editor(markdown);
        auto const id = first_text(editor).id;
        editor.set_selection(TextSelection::caret({id, 1, TextAffinity::Downstream}));
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(bool(editor.execute_document_insert_text(editor.selection(), U"X").has_value()))) << markdown;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u)));
        expect(fatal(bool(counters.full_document_serializations == 0u)));
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
        expect(fatal(bool(counters.inline_reparses == 1u)));
        auto edited = *editor.editable_source(id);
        expect(fatal(bool(edited == U"aXbc"))) << markdown << " source=" << cps_to_utf8(edited);
        auto const* owner = find_block(editor.document().root, id);
        expect(fatal(bool(owner != nullptr)));
        if (owner) expect(fatal(bool(flatten_tokens(owner->inline_content.tree, owner->inline_content.source) == owner->inline_content.source)));
        expect(fatal(bool(editor.selection().active.container_id == id)));
        expect(fatal(bool(editor.selection().active.source_offset == 2u)));

        const auto saved = editor.markdown_utf8();
        Editor reloaded(saved);
        expect(fatal(bool(first_text(reloaded).inline_content.source == U"aXbc")));

        expect(fatal(bool(editor.undo())));
        expect(fatal(bool(editor.markdown_utf8() == before)));
        expect(fatal(bool(*editor.editable_source(id) == U"abc")));
        expect(fatal(bool(editor.redo())));
        expect(fatal(bool(*editor.editable_source(id) == U"aXbc")));
    }
};

"enter_replaces_the_source_selection_before_applying_block_semantics"_test = [] {
    auto exercise = [](std::string markdown, std::u32string_view selected_source,
                       std::u32string_view expected_first, std::u32string_view expected_second) {
        Editor editor(std::move(markdown));
        const auto fragments = document_text_fragments(editor.document());
        auto selected = std::find_if(fragments.begin(), fragments.end(), [&](const auto& fragment) {
            return fragment.text == selected_source;
        });
        expect(fatal(bool(selected != fragments.end())));
        if (selected == fragments.end()) return;

        const auto before_markdown = editor.markdown_utf8();
        const TextSelection before{
            {selected->container_id, 1, TextAffinity::Downstream},
            {selected->container_id, 3, TextAffinity::Downstream},
        };
        editor.set_selection(before);
        reset_core_operation_counters();
        auto transaction = editor.execute_document_enter(editor.selection());
        expect(fatal(bool(transaction.has_value())));
        if (!transaction) return;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u)));
        expect(fatal(bool(counters.full_document_serializations == 0u)));
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));

        const auto after_fragments = document_text_fragments(editor.document());
        auto first = std::find_if(after_fragments.begin(), after_fragments.end(), [&](const auto& fragment) {
            return fragment.text == expected_first;
        });
        auto second = std::find_if(after_fragments.begin(), after_fragments.end(), [&](const auto& fragment) {
            return fragment.text == expected_second;
        });
        expect(fatal(bool(first != after_fragments.end())));
        expect(fatal(bool(second != after_fragments.end())));
        const auto after_markdown = editor.markdown_utf8();
        const auto after_selection = editor.selection();

        expect(fatal(bool(editor.undo())));
        expect(fatal(bool(editor.markdown_utf8() == before_markdown)));
        expect(fatal(bool(editor.selection() == before)));
        expect(fatal(bool(editor.redo())));
        expect(fatal(bool(editor.markdown_utf8() == after_markdown)));
        expect(fatal(bool(editor.selection() == after_selection)));
    };

    exercise("abcd", U"abcd", U"a", U"d");
    exercise("> abcd", U"abcd", U"a", U"d");
    exercise("- abcd", U"abcd", U"a", U"d");
    exercise("> [!NOTE] abcd\n> body", U"abcd", U"a", U"d");
    exercise("| abcd |\n| --- |", U"abcd", U"a<br>d", U"a<br>d");
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

"representative_inline_states_round_trip_through_every_editable_context"_test = [] {
    const std::vector<std::u32string> samples{
        U"abc", U"*abc*", U"_abc_", U"**abc**", U"__abc__", U"**", U"**abc",
        U"a***b***c", U"~~abc~~", U"~~abc", U"`abc`", U"`abc", U"[title](url)",
        U"[title](<url>)", U"[title](url \"name\")", U"[title](", U"![alt](url)",
        U"$abc$", U"$abc", U"\\*abc\\*", U"&amp;", U"a\\**b*",
    };

    for (const auto& sample : samples) {
        const auto original_source = U"p" + sample + U"q";
        const auto source_utf8 = cps_to_utf8(original_source);
        const std::vector<std::string> documents{
            source_utf8,
            "# " + source_utf8,
            "- " + source_utf8,
            "- [ ] " + source_utf8,
            "> " + source_utf8,
            "| " + source_utf8 + " |\n| --- |",
            "> [!NOTE]\n> " + source_utf8,
            "[^1]: " + source_utf8,
            "- > " + source_utf8,
        };

        for (const auto& markdown : documents) {
            Editor editor(markdown);
            const auto owner_id = first_text(editor).id;
            expect(fatal(bool(first_text(editor).inline_content.source == original_source)));
            const auto offset = 1 + sample.size() / 2;
            const auto before_source = editor.markdown_utf8();
            const auto before_selection = TextSelection::caret(
                {owner_id, offset, TextAffinity::Downstream});
            editor.set_selection(before_selection);

            reset_core_operation_counters();
            expect(fatal(bool(editor.execute_document_insert_text(editor.selection(), U"X").has_value())));
            const auto counters = read_core_operation_counters();
            expect(fatal(bool(counters.full_document_parses == 0u)));
            expect(fatal(bool(counters.full_document_serializations == 0u)));
            expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
            expect(fatal(bool(counters.inline_reparses == 1u)));

            auto expected_source = original_source;
            expected_source.insert(offset, 1, U'X');
            const auto* owner = find_block(editor.document().root, owner_id);
            expect(fatal(bool(owner != nullptr)));
            if (!owner) continue;
            expect(fatal(bool(owner->inline_content.source == expected_source)));
            expect(fatal(bool(flatten_tokens(owner->inline_content.tree, owner->inline_content.source)
                == owner->inline_content.source)));
            expect(fatal(bool(editor.selection().active.container_id == owner_id)));
            expect(fatal(bool(editor.selection().active.source_offset == offset + 1)));

            const auto edited_source = editor.markdown_utf8();
            Editor reloaded(edited_source);
            expect(fatal(bool(first_text(reloaded).inline_content.source == expected_source)));

            expect(fatal(bool(editor.undo())));
            expect(fatal(bool(editor.markdown_utf8() == before_source)));
            expect(fatal(bool(editor.selection() == before_selection)));
            expect(fatal(bool(editor.redo())));
            expect(fatal(bool(editor.markdown_utf8() == edited_source)));
            expect(fatal(bool(*editor.editable_source(owner_id) == expected_source)));
        }
    }
};

"random_editor_source_edits_restore_exactly_across_editable_contexts"_test = [] {
    std::mt19937_64 random{0xED1705u};
    const std::u32string alphabet = U"abc *_~`[]()!$\\&;'\"😀";
    const auto make_document = [](std::size_t context, std::u32string_view source) {
        const auto text = cps_to_utf8(source);
        switch (context) {
            case 0: return text;
            case 1: return "# " + text;
            case 2: return "- " + text;
            case 3: return "> " + text;
            case 4: return "| " + text + " |\n| --- |";
            case 5: return "> [!NOTE] " + text + "\n> body";
            case 6: return "> [!NOTE] title\n> " + text;
            default: return "[^n]: " + text;
        }
    };

    for (std::size_t iteration = 0; iteration < 240; ++iteration) {
        std::u32string original = U"p";
        const auto random_length = static_cast<std::size_t>(random() % 32);
        for (std::size_t index = 0; index < random_length; ++index) {
            original.push_back(alphabet[random() % alphabet.size()]);
        }
        original.push_back(U'q');

        const auto context = iteration % 8;
        Editor editor(make_document(context, original));
        const auto fragments = document_text_fragments(editor.document());
        auto owner = std::find_if(fragments.begin(), fragments.end(), [&](const auto& fragment) {
            return fragment.text == original;
        });
        expect(fatal(bool(owner != fragments.end()))) << iteration;
        if (owner == fragments.end()) continue;

        auto expected = original;
        const auto insert = (random() & 1u) != 0;
        const auto offset = insert
            ? static_cast<std::size_t>(random() % (original.size() + 1))
            : 1 + static_cast<std::size_t>(random() % original.size());
        const auto before = TextSelection::caret({
            owner->container_id,
            offset,
            TextAffinity::Downstream,
        });
        editor.set_selection(before);
        const auto before_markdown = editor.markdown_utf8();

        if (insert) expected.insert(offset, 1, U'X');
        else {
            expected.erase(offset - 1, 1);
            // The command removes a complete formatting pair when its target
            // is a delimiter. Build the oracle from the original CST, before
            // mutation; ordinary/incomplete text still loses one character.
            const auto* original_block = find_block(editor.document().root, owner->container_id);
            std::vector<const InlineCstNode*> pending;
            for (const auto& node : original_block->inline_content.tree.nodes) pending.push_back(&node);
            while (!pending.empty()) {
                const auto* node = pending.back(); pending.pop_back();
                for (const auto& child : node->children) pending.push_back(&child);
                if (node->status != ParseStatus::Complete || !node->delimited || !node->delimited->closing) continue;
                const auto paired = node->kind == InlineCstKind::Emphasis || node->kind == InlineCstKind::Strong
                    || node->kind == InlineCstKind::Strikethrough || node->kind == InlineCstKind::CodeSpan
                    || node->kind == InlineCstKind::InlineMath;
                const auto& parts = *node->delimited;
                const auto character = offset - 1;
                const auto in_marker = (character >= parts.opening.start && character < parts.opening.end)
                    || (character >= parts.closing->start && character < parts.closing->end);
                if (!paired || !in_marker) continue;
                expected = original.substr(0, node->range.start)
                    + original.substr(parts.content.start, parts.content.length())
                    + original.substr(node->range.end);
                break;
            }
        }

        reset_core_operation_counters();
        auto transaction = insert
            ? editor.execute_document_insert_text(editor.selection(), U"X")
            : editor.execute_document_delete_backward(editor.selection());
        expect(fatal(bool(transaction.has_value()))) << iteration;
        if (!transaction) continue;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << iteration;
        expect(fatal(bool(counters.full_document_serializations == 0u))) << iteration;
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << iteration;
        expect(fatal(bool(counters.inline_reparses == 1u))) << iteration;

        const auto* block = find_block(editor.document().root, owner->container_id);
        expect(fatal(bool(block != nullptr))) << iteration;
        if (!block) continue;
        const auto* inline_document = editable_inline_document(*block);
        expect(fatal(bool(inline_document != nullptr))) << iteration;
        if (!inline_document) continue;
        expect(fatal(bool(inline_document->source == expected))) << iteration;
        expect(fatal(bool(flatten_tokens(inline_document->tree, inline_document->source)
            == inline_document->source))) << iteration;
        expect(fatal(bool(editor.selection().active.container_id == owner->container_id))) << iteration;
        expect(fatal(bool(editor.selection().active.source_offset <= expected.size()))) << iteration;

        const auto after_markdown = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        Editor reloaded(after_markdown);
        const auto reloaded_fragments = document_text_fragments(reloaded.document());
        expect(fatal(bool(std::ranges::any_of(reloaded_fragments, [&](const auto& fragment) {
            return fragment.text == expected;
        })))) << iteration;

        expect(fatal(bool(editor.undo()))) << iteration;
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << iteration;
        expect(fatal(bool(editor.selection() == before))) << iteration;
        expect(fatal(bool(editor.redo()))) << iteration;
        expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << iteration;
        expect(fatal(bool(editor.selection() == after_selection))) << iteration;
        expect(fatal(bool(*editor.editable_source(owner->container_id) == expected))) << iteration;
    }
};

}; // suite editor_table_property_tests
