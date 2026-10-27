#include <algorithm>
#include <string>
#include <string_view>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.editor;
import folia.markdown.instrumentation;
import folia.markdown.serializer;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

void exercise_editor_edits(std::string_view label, const std::string& source) {
    Editor editor(source);
    const auto before = editor.markdown_utf8();
    expect(before == source) << label << " initial save";
    const auto owner = first_text(editor);
    const auto selection_before = TextSelection::caret(
        {owner.id, (std::min)(std::size_t{2}, owner.inline_content.source.size()),
         TextAffinity::Downstream});
    editor.set_selection(selection_before);

    reset_core_operation_counters();
    const auto entered = editor.execute_document_enter(editor.selection());
    expect(entered.has_value()) << label << " Enter";
    if (!entered) return;
    const auto enter_counters = read_core_operation_counters();
    expect(enter_counters.full_document_parses == 0u) << label << " Enter parse count";
    expect(enter_counters.full_document_serializations == 0u) << label << " Enter serialize count";
    const auto after_enter = editor.markdown_utf8();
    expect(editor.undo()) << label << " Enter undo";
    expect(editor.markdown_utf8() == before) << label << " Enter undo source";
    expect(editor.selection() == selection_before) << label << " Enter undo selection";
    expect(editor.redo()) << label << " Enter redo";
    expect(editor.markdown_utf8() == after_enter) << label << " Enter redo source";

    Editor deletion(source);
    const auto delete_owner = first_text(deletion);
    const auto delete_before = deletion.markdown_utf8();
    const auto delete_selection = TextSelection::caret(
        {delete_owner.id, (std::min)(std::size_t{2}, delete_owner.inline_content.source.size()),
         TextAffinity::Downstream});
    deletion.set_selection(delete_selection);
    reset_core_operation_counters();
    const auto deleted = deletion.execute_document_delete_backward(deletion.selection());
    expect(deleted.has_value()) << label << " Backspace";
    if (!deleted) return;
    const auto delete_counters = read_core_operation_counters();
    expect(delete_counters.full_document_parses == 0u) << label << " Backspace parse count";
    expect(delete_counters.full_document_serializations == 0u) << label << " Backspace serialize count";
    const auto after_delete = deletion.markdown_utf8();
    expect(deletion.undo()) << label << " Backspace undo";
    expect(deletion.markdown_utf8() == delete_before) << label << " Backspace undo source";
    expect(deletion.selection() == delete_selection) << label << " Backspace undo selection";
    expect(deletion.redo()) << label << " Backspace redo";
    expect(deletion.markdown_utf8() == after_delete) << label << " Backspace redo source";
}

} // namespace

suite editor_list_separator_history_tests = [] {

"commonmark_306_separator_survives_enter_and_history"_test = [] {
    exercise_editor_edits("CommonMark #306", "- alpha\n\n- beta\n");
};

"nested_list_separator_survives_enter_and_history"_test = [] {
    exercise_editor_edits("nested list", "- parent\n\n  - child\n\n  - sibling\n\n- next\n");
};

"task_list_separator_survives_enter_and_history"_test = [] {
    exercise_editor_edits("task list", "- [ ] alpha\n\n- [x] beta\n");
};

}; // suite editor_list_separator_history_tests
