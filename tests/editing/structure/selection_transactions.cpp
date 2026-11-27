#include <cstddef>
#include <random>
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

suite editing_structure_selection_transactions_tests = [] {

"structural_prefix_backspace_records_uniform_tree_operations"_test = [] {
    auto exercise = [](Editor& editor, NodeId owner, std::string_view label) {
        const auto before_markdown = editor.markdown_utf8();
        const auto before = TextSelection::caret({owner, 0, TextAffinity::Downstream});
        editor.set_selection(before);
        reset_core_operation_counters();
        auto transaction = editor.execute_document_delete_backward(editor.selection());
        expect(fatal(bool(transaction.has_value()))) << label << " transaction";
        if (!transaction) return;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << label;
        expect(fatal(bool(counters.full_document_serializations == 0u))) << label;
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << label;
        expect(fatal(bool(!transaction->operations.empty()))) << label;
        const auto after_markdown = editor.markdown_utf8();
        const auto after = editor.selection();
        expect(fatal(bool(editor.undo()))) << label;
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << label;
        expect(fatal(bool(editor.selection() == before))) << label;
        expect(fatal(bool(editor.redo()))) << label;
        expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << label;
        expect(fatal(bool(editor.selection() == after))) << label;
    };

    Editor heading("# title");
    exercise(heading, heading.document().root.children.front().id, "heading");

    Editor quote("> quote");
    exercise(quote, quote.document().root.children.front().children.front().id, "quote");

    Editor callout("> [!NOTE] title\n> body");
    exercise(
        callout,
        callout.document().root.children.front().children.front().id,
        "callout title");

    Editor untitled_callout("> [!NOTE]\n> body");
    exercise(
        untitled_callout,
        untitled_callout.document().root.children.front().children.front().id,
        "untitled callout");

    for (auto markdown : {
             std::string{"- item"},
             std::string{"- one\n- two\n- three"},
             std::string{"> - item"}}) {
        Editor list(markdown);
        const BlockNode* selected = nullptr;
        walk_blocks(list.document().root, [&](const BlockNode& block) {
            if (block.kind != BlockKind::Paragraph) return;
            if (!selected || block.inline_content().source == U"two") selected = &block;
        });
        expect(fatal(bool(selected != nullptr))) << markdown;
        if (selected) exercise(list, selected->id, markdown);
    }

    Editor nested_list("- parent\n  - child");
    const BlockNode* nested_child = nullptr;
    walk_blocks(nested_list.document().root, [&](const BlockNode& block) {
        if (block.kind == BlockKind::Paragraph && block.inline_content().source == U"child") {
            nested_child = &block;
        }
    });
    expect(fatal(bool(nested_child != nullptr)));
    if (nested_child) exercise(nested_list, nested_child->id, "nested list");
};

"cross_block_selection_delete_records_source_and_pruning_operations"_test = [] {
    auto exercise = [](Editor& editor, TextSelection selection, std::string_view label) {
        const auto before_markdown = editor.markdown_utf8();
        editor.set_selection(selection);
        reset_core_operation_counters();
        auto transaction = editor.execute_document_delete_selection(editor.selection());
        expect(fatal(bool(transaction.has_value()))) << label;
        if (!transaction) return;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << label << " parses";
        expect(fatal(bool(counters.full_document_serializations == 0u))) << label << " serializations";
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << label << " tree diffs";
        expect(fatal(bool(counters.full_document_text_projections == 0u))) << label << " projections";
        expect(fatal(bool(counters.full_document_block_index_scans == 0u))) << label << " index scans";
        expect(fatal(bool(counters.incremental_document_block_index_repairs == 1u))) << label << " index repairs";
        expect(fatal(bool(!transaction->operations.empty()))) << label << " operations";
        expect(fatal(document_indexes_are_exact(editor.document()))) << label << " indexes after";
        const auto after_markdown = editor.markdown_utf8();
        const auto after = editor.selection();
        const auto undo_succeeded = editor.undo();
        expect(fatal(bool(undo_succeeded))) << label << " undo";
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << label << " undo source";
        expect(fatal(bool(editor.selection() == selection))) << label << " undo selection";
        expect(fatal(document_indexes_are_exact(editor.document()))) << label << " undo indexes";
        const auto redo_succeeded = editor.redo();
        expect(fatal(bool(redo_succeeded))) << label << " redo";
        expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << label << " redo source";
        expect(fatal(bool(editor.selection() == after))) << label << " redo selection";
        expect(fatal(document_indexes_are_exact(editor.document()))) << label << " redo indexes";
    };

    Editor paragraphs("alpha\n\nbeta\n\ngamma");
    exercise(
        paragraphs,
        TextSelection{
            {paragraphs.document().root.children.front().id, 2, TextAffinity::Downstream},
            {paragraphs.document().root.children.back().id, 3, TextAffinity::Downstream}},
        "paragraphs");

    Editor nested("alpha\n\n> beta\n\n- gamma\n\nomega");
    const BlockNode* first = nullptr;
    const BlockNode* last = nullptr;
    walk_blocks(nested.document().root, [&](const BlockNode& block) {
        if (block.kind != BlockKind::Paragraph) return;
        if (block.inline_content().source == U"alpha") first = &block;
        if (block.inline_content().source == U"omega") last = &block;
    });
    expect(fatal(bool(first != nullptr && last != nullptr)));
    if (first && last) {
        exercise(
            nested,
            TextSelection{
                {first->id, 2, TextAffinity::Downstream},
                {last->id, 2, TextAffinity::Downstream}},
            "nested containers");
    }

    Editor callout("> [!NOTE] title\n> body");
    const auto callout_id = callout.document().root.children.front().id;
    const auto title_id = callout.document().root.children.front().children.front().id;
    const auto body_id = callout.document().root.children.front().children[1].id;
    exercise(
        callout,
        TextSelection{
            {title_id, 2, TextAffinity::Downstream},
            {body_id, 2, TextAffinity::Downstream}},
        "callout title to body");
    const auto* remaining_callout = find_block(callout.document().root, callout_id);
    expect(fatal(bool(remaining_callout != nullptr)));
    expect(fatal(bool(remaining_callout && callout_title_block(*remaining_callout) != nullptr)));

    Editor raw("```\nalpha\n```\n\nomega");
    const auto code_id = raw.document().root.children.front().id;
    const auto paragraph_id = raw.document().root.children.back().id;
    const auto code_start = block_source_editable_range(
        raw.document().root.children.front().block_source).start;
    exercise(
        raw,
        TextSelection{
            {code_id, code_start, TextAffinity::Downstream},
            {paragraph_id, 2, TextAffinity::Downstream}},
        "raw source boundary");

    Editor math("alpha\n\nintermediate\n\n$$\nx\n$$\n");
    exercise(math, TextSelection{
        {math.document().root.children.front().id, 2, TextAffinity::Downstream},
        {math.document().root.children.back().id, 2, TextAffinity::Downstream}},
        "selection removes a display math opener");
};

"selection_replacement_composes_delete_and_insert_source_edits"_test = [] {
    Editor editor("alpha");
    const auto owner_id = first_text(editor).id;
    const TextSelection before_selection{
        {owner_id, 1, TextAffinity::Downstream},
        {owner_id, 4, TextAffinity::Upstream}};
    editor.set_selection(before_selection);
    reset_core_operation_counters();
    auto transaction = editor.execute_document_insert_text(editor.selection(), U"X");
    expect(fatal(bool(transaction.has_value())));
    if (!transaction) return;
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(counters.inline_reparses == 2u)));
    expect(fatal(bool(transaction->operations.size() == 2u)));
    expect(fatal(bool(editor.markdown_utf8() == "aXa")));
    const auto after_selection = editor.selection();

    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == "alpha")));
    expect(fatal(bool(editor.selection() == before_selection)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == "aXa")));
    expect(fatal(bool(editor.selection() == after_selection)));
};

};
