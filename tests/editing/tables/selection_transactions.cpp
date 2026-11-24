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

suite editing_tables_selection_transactions_tests = [] {

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
    expect(fatal(bool(edited.children[0].children[0].inline_content().source == U"a")));
    expect(fatal(bool(edited.children[0].children[1].inline_content().source.empty())));
    expect(fatal(bool(edited.children[1].children[0].inline_content().source.empty())));
    expect(fatal(bool(edited.children[1].children[1].inline_content().source == U"d")));
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
    expect(fatal(bool(editor.document().root.children[0].inline_content().source == U"le")));
    expect(fatal(bool(table.children[0].children[0].inline_content().source.empty())));
    expect(fatal(bool(table.children[0].children[1].inline_content().source == U"b")));
    expect(fatal(bool(table.children[1].children[0].inline_content().source == U"cc")));
    expect(fatal(bool(table.children[1].children[1].inline_content().source == U"dd")));
    expect(fatal(bool(editor.document().root.children[2].inline_content().source == U"trail")));
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
    expect(fatal(bool(table.children[0].children[0].inline_content().source == U"aX")));
    expect(fatal(bool(table.children[0].children[1].inline_content().source.empty())));
    expect(fatal(bool(table.children[1].children[0].inline_content().source.empty())));
    expect(fatal(bool(table.children[1].children[1].inline_content().source == U"d")));
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

};
