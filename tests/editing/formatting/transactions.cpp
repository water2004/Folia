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

suite editing_formatting_transactions_tests = [] {

"inline_delete_and_format_record_their_text_edits_directly"_test = [] {
    Editor editor("alpha");
    editor.set_selection(caret(first_text(editor), 2));
    reset_core_operation_counters();
    expect(fatal(bool(editor.execute_document_delete_backward(editor.selection()).has_value())));
    const auto deletion = read_core_operation_counters();
    expect(fatal(bool(deletion.full_document_parses == 0u)));
    expect(fatal(bool(deletion.full_document_serializations == 0u)));
    expect(fatal(bool(deletion.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(deletion.inline_reparses == 1u)));

    editor.set_selection(range(first_text(editor), 0, 2));
    reset_core_operation_counters();
    expect(fatal(bool(editor.execute_document_toggle_inline_format(
        editor.selection(), InlineFormat::Strong).has_value())));
    const auto formatting = read_core_operation_counters();
    expect(fatal(bool(formatting.full_document_parses == 0u)));
    expect(fatal(bool(formatting.full_document_serializations == 0u)));
    expect(fatal(bool(formatting.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(formatting.inline_reparses == 1u)));
};

"heading_adjacent_blank_line_delete_undo_and_redo_are_exact"_test = [] {
    Editor editor("# title\n\n\n## next");
    expect(fatal(bool(editor.document().root.children.size() == 3u)));
    if (editor.document().root.children.size() != 3u) return;
    const auto title_id = editor.document().root.children.front().id;
    const auto title_size = editor.document().root.children.front().inline_content().source.size();
    const auto blank_id = editor.document().root.children[1].id;
    const auto before = TextSelection::caret({blank_id, 0, TextAffinity::Downstream});
    editor.set_selection(before);

    expect(fatal(editor.execute_command(Command{.kind = CommandKind::DeleteBackward})));
    expect(fatal(bool(editor.markdown_utf8() == "# title\n## next")));
    expect(fatal(bool(editor.selection().active.container_id == title_id)));
    expect(fatal(bool(editor.selection().active.source_offset == title_size)));
    const auto after = editor.selection();

    expect(fatal(editor.undo()));
    expect(fatal(bool(editor.markdown_utf8() == "# title\n\n\n## next")));
    expect(fatal(bool(editor.selection() == before)));
    expect(fatal(editor.redo()));
    expect(fatal(bool(editor.markdown_utf8() == "# title\n## next")));
    expect(fatal(bool(editor.selection() == after)));
};

"format_undo_redo_preserve_original_marker_spelling"_test = [] {
    Editor editor("value");
    editor.set_selection(range(first_text(editor), 0, 5));
    expect(fatal(bool(editor.execute_document_toggle_inline_format(editor.selection(), InlineFormat::Strong).has_value())));
    expect(fatal(bool(editor.markdown_utf8() == "**value**")));
    expect(fatal(bool(inline_contains_kind(first_text(editor).inline_content(), InlineCstKind::Strong))));
    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == "value")));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == "**value**")));
};

"format_toggle_uses_cst_ranges_and_original_delimiters"_test = [] {
    struct Case {
        std::string source;
        InlineFormat format;
        std::size_t start;
        std::size_t end;
    };
    const std::vector<Case> cases{
        {"_value_", InlineFormat::Emphasis, 1, 6},
        {"__value__", InlineFormat::Strong, 2, 7},
        {"~~value~~", InlineFormat::Strikethrough, 2, 7},
        {"``value``", InlineFormat::Code, 2, 7},
        {"$value$", InlineFormat::Math, 1, 6},
        {"__value__", InlineFormat::Strong, 0, 9},
    };

    for (const auto& test : cases) {
        Editor editor(test.source);
        const auto before = range(first_text(editor), test.start, test.end);
        editor.set_selection(before);

        reset_core_operation_counters();
        const auto transaction = editor.execute_document_toggle_inline_format(
            editor.selection(),
            test.format);
        expect(fatal(bool(transaction.has_value()))) << test.source;
        if (!transaction) continue;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << test.source;
        expect(fatal(bool(counters.full_document_serializations == 0u))) << test.source;
        expect(fatal(bool(counters.inline_reparses == 1u))) << test.source;
        expect(fatal(bool(transaction->operations.size() == 1u))) << test.source;
        expect(fatal(bool(editor.markdown_utf8() == "value"))) << test.source;
        expect(fatal(bool(editor.selection().anchor.source_offset == 0u))) << test.source;
        expect(fatal(bool(editor.selection().active.source_offset == 5u))) << test.source;
        expect(fatal(bool(validate_document(editor.document()).empty()))) << test.source;

        const auto after = editor.selection();
        expect(fatal(bool(editor.undo()))) << test.source;
        expect(fatal(bool(editor.markdown_utf8() == test.source))) << test.source;
        expect(fatal(bool(editor.selection() == before))) << test.source;
        expect(fatal(bool(editor.redo()))) << test.source;
        expect(fatal(bool(editor.markdown_utf8() == "value"))) << test.source;
        expect(fatal(bool(editor.selection() == after))) << test.source;
    }
};

"format_toggle_at_a_caret_unwraps_the_innermost_matching_cst_node"_test = [] {
    Editor editor("**a *bc* d**");
    editor.set_selection(caret(first_text(editor), 6));
    const auto before = editor.selection();

    const auto transaction = editor.execute_document_toggle_inline_format(
        editor.selection(),
        InlineFormat::Emphasis);
    expect(fatal(bool(transaction.has_value())));
    if (!transaction) return;
    expect(fatal(bool(editor.markdown_utf8() == "**a bc d**")));
    expect(fatal(bool(editor.selection().is_caret())));
    expect(fatal(bool(editor.selection().active.source_offset == 5u)));
    expect(fatal(bool(inline_contains_kind(
        first_text(editor).inline_content(),
        InlineCstKind::Strong))));
    expect(fatal(bool(!inline_contains_kind(
        first_text(editor).inline_content(),
        InlineCstKind::Emphasis))));

    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == "**a *bc* d**")));
    expect(fatal(bool(editor.selection() == before)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == "**a bc d**")));
};

"partial_format_toggle_splits_the_original_run_without_normalizing_markers"_test = [] {
    Editor editor("__abcd__");
    const auto before = range(first_text(editor), 3, 5);
    editor.set_selection(before);

    const auto transaction = editor.execute_document_toggle_inline_format(
        editor.selection(),
        InlineFormat::Strong);
    expect(fatal(bool(transaction.has_value())));
    if (!transaction) return;
    expect(fatal(bool(editor.markdown_utf8() == "__a__bc__d__")));
    expect(fatal(bool(editor.selection().anchor.source_offset == 5u)));
    expect(fatal(bool(editor.selection().active.source_offset == 7u)));
    expect(fatal(bool(validate_document(editor.document()).empty())));

    const auto after = editor.selection();
    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == "__abcd__")));
    expect(fatal(bool(editor.selection() == before)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == "__a__bc__d__")));
    expect(fatal(bool(editor.selection() == after)));
};

};
