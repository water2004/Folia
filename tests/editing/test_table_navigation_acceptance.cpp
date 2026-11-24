#include "support/folia_test.hpp"
import folia.core.document_edit;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

const BlockNode* navigation_table(const Editor& editor) {
    const BlockNode* result = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (!result && block.kind == BlockKind::Table) result = &block;
    });
    return result;
}

suite table_navigation_acceptance_tests = [] {

"table_navigation_shift_tab_reaches_the_preceding_owner"_test = [] {
    for (const auto source : {
            "before\n\n| a | b |\n| --- | --- |\n| c | d |",
            "> before\n\n| a | b |\n| --- | --- |\n| c | d |",
            "```\nvalue\n```\n\n| a | b |\n| --- | --- |\n| c | d |"}) {
        Editor editor(source);
        const auto* table = navigation_table(editor);
        expect(fatal(table != nullptr));
        if (!table) continue;
        const auto first_cell = table->children.front().children.front().id;
        const auto first_index = *document_editable_order_position(editor.document(), first_cell);
        const auto previous_id = editor.document().cached_editable_order[first_index - 1];
        const auto* previous = find_document_block(editor.document(), previous_id);
        const auto previous_range = *editable_block_source_range(*previous);
        const auto selection = TextSelection::caret({first_cell, 1, TextAffinity::Downstream});
        editor.set_selection(selection);
        const auto revision = editor.revision();
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        auto transaction = editor.execute_document_table_edit(selection, DocumentTableEdit::MoveCellPrevious);
        expect(fatal(transaction.has_value())) << source;
        expect(editor.selection() == TextSelection::caret({previous_id, previous_range.end, TextAffinity::Upstream}));
        expect(editor.revision() == revision);
        expect(editor.markdown_utf8() == before);
        expect(!editor.has_undo());
        expect(transaction->operations.empty());
    }
};

"table_navigation_shift_tab_creates_a_reversible_leading_paragraph"_test = [] {
    for (const auto source : {
            "| a | b |\n| --- | --- |\n| c | d |",
            "> | a | b |\n> | --- | --- |\n> | c | d |",
            "- | a | b |\n  | --- | --- |\n  | c | d |",
            "<div><table><tr><td>a</td><td>b</td></tr></table></div>"}) {
        Editor editor(source);
        const auto* table = navigation_table(editor);
        expect(fatal(table != nullptr)) << source;
        if (!table) continue;
        const auto table_id = table->id;
        auto table_path = *document_block_path(editor.document(), table_id);
        const auto before = editor.markdown_utf8();
        const auto first_cell = table->children.front().children.front().id;
        const auto selection = TextSelection::caret({first_cell, 0, TextAffinity::Downstream});
        editor.set_selection(selection);
        reset_core_operation_counters();
        auto transaction = editor.execute_document_table_edit(selection, DocumentTableEdit::MoveCellPrevious);
        expect(fatal(transaction.has_value())) << source;
        const auto counts = read_core_operation_counters();
        expect(counts.full_document_parses == 0_u);
        expect(counts.full_document_serializations == 0_u);
        expect(counts.full_tree_transaction_diffs == 0_u);
        const auto* paragraph = find_document_block(editor.document(), editor.selection().active.container_id);
        expect(fatal(paragraph != nullptr));
        expect(paragraph->kind == BlockKind::Paragraph);
        expect(paragraph->inline_content().source.empty());
        const auto paragraph_path = *document_block_path(editor.document(), paragraph->id);
        expect(paragraph_path == table_path);
        ++table_path.back();
        expect(*document_block_path(editor.document(), table_id) == table_path);
        if (std::string_view(source).starts_with("<div>"))
            expect(paragraph->inline_content().syntax_mode == InlineSyntaxMode::HtmlText);
        expect(validate_document(editor.document()).empty());
        expect(document_indexes_are_exact(editor.document()));
        const auto after = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        expect(editor.undo());
        expect(editor.markdown_utf8() == before);
        expect(editor.selection() == selection);
        expect(editor.redo());
        expect(editor.markdown_utf8() == after);
        expect(editor.selection() == after_selection);
        expect(document_indexes_are_exact(editor.document()));
    }
};

"table_navigation_collapses_cross_cell_ranges_from_the_active_cell"_test = [] {
    for (const auto backward : {false, true}) {
        Editor editor("| aa | bb |\n| --- | --- |\n| cc | dd |");
        const auto* table = navigation_table(editor);
        expect(fatal(table != nullptr));
        if (!table) continue;
        const auto first = table->children[0].children[0].id;
        const auto second = table->children[0].children[1].id;
        const auto third = table->children[1].children[0].id;
        const auto selection = TextSelection{
            {first, 1, TextAffinity::Downstream}, {second, 1, TextAffinity::Upstream}};
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        const auto revision = editor.revision();
        expect(editor.execute_document_table_edit(selection, backward
            ? DocumentTableEdit::MoveCellPrevious : DocumentTableEdit::MoveCellNext).has_value());
        expect(editor.selection() == TextSelection::caret({backward ? first : third, 0, TextAffinity::Downstream}));
        expect(editor.markdown_utf8() == before);
        expect(editor.revision() == revision);
        expect(!editor.has_undo());
    }
};

};

} // namespace
