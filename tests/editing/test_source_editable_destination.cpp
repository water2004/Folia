#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

void source_destination_history(Editor& editor, const std::u32string& before,
    const std::u32string& after, SourceSelection selected) {
    expect(editor.source_editor()->source() == after);
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
    expect(!editor.document().cached_editable_order.empty());
    expect(fatal(editor.undo()));
    expect(editor.source_editor()->source() == before && editor.source_editor()->selection() == selected);
    expect(document_indexes_are_exact(editor.document()));
    expect(!editor.has_undo());
    expect(fatal(editor.redo()));
    expect(editor.source_editor()->source() == after);
    expect(document_indexes_are_exact(editor.document()));
    expect(editor.exit_source_mode());
    expect(editor.markdown_utf8() == cps_to_utf8(after));
    const auto* owner = find_document_block(editor.document(), editor.selection().active.container_id);
    expect(fatal(owner && editable_block_source_range(*owner).has_value()));
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == cps_to_utf8(before));
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == cps_to_utf8(after));
    expect(editor.enter_source_mode());
    expect(editor.source_editor()->source() == after);
    Editor reloaded(cps_to_utf8(after));
    expect(reloaded.markdown_utf8() == cps_to_utf8(after));
    expect(!reloaded.document().cached_editable_order.empty());
    expect(document_indexes_are_exact(reloaded.document()));
}

void source_destination_locality() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0u && counts.full_document_serializations == 0u
        && counts.full_tree_transaction_diffs == 0u && counts.full_document_block_index_scans == 0u);
}

} // namespace

suite source_editable_destination_tests = [] {

"source invisible documents preserve exact endings and a zero-source destination"_test = [] {
    for (const auto base : {U"<!-- note -->", U"> <!-- note -->", U"- <!-- note -->"})
    for (const auto ending : {U"", U"\n", U"\r\n", U"\r"}) {
        const auto before = std::u32string(base) + ending;
        Editor editor(cps_to_utf8(before));
        expect(fatal(editor.markdown_utf8() == cps_to_utf8(before)));
        expect(fatal(!editor.document().cached_editable_order.empty()));
        expect(editor.enter_source_mode());
        expect(editor.source_editor()->source() == before);
        const auto offset = before.find(U"note") + 2;
        const auto selected = SourceSelection::caret(offset);
        editor.set_source_selection(selected);
        auto expected = before;
        expected.insert(offset, U"X");
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(Command::InsertText(U"X"))));
        source_destination_locality();
        source_destination_history(editor, before, expected, selected);
    }
};

"source deleting the final visible owner preserves untouched invisible suffix roots"_test = [] {
    for (const auto ending : {std::u32string{U"\n"}, std::u32string{U"\r\n"}, std::u32string{U"\r"}})
    for (const auto reverse : {false, true}) {
        const auto prefix = U"erase" + ending + ending;
        const auto comments = U"<!-- one -->" + ending + ending + U"<!-- two -->" + ending + ending
            + U"<!-- three -->" + ending + ending + U"<!-- four -->" + ending;
        const auto before = prefix + comments;
        Editor editor(cps_to_utf8(before));
        expect(fatal(editor.enter_source_mode()));
        const auto suffix_id = editor.document().root.children.back().id;
        const SourceSelection selected = reverse ? SourceSelection{prefix.size(), 0}
            : SourceSelection{0, prefix.size()};
        editor.set_source_selection(selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::DeleteSelection})));
        source_destination_locality();
        expect(editor.document().root.children.back().kind == BlockKind::Paragraph);
        expect(editor.document().root.children[editor.document().root.children.size() - 2].id == suffix_id);
        source_destination_history(editor, before, comments, selected);
    }
};

"source replacement with invisible markup creates its destination in the same history entry"_test = [] {
    for (const auto comment : {U"<!-- note -->", U"> <!-- note -->", U"- <!-- note -->"}) {
        const std::u32string before = U"editable";
        Editor editor(cps_to_utf8(before));
        expect(fatal(editor.enter_source_mode()));
        const SourceSelection selected{before.size(), 0};
        editor.set_source_selection(selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(Command::InsertText(comment))));
        source_destination_locality();
        source_destination_history(editor, before, std::u32string(comment), selected);
    }
};

"source local edits keep an untouched distant zero-source destination"_test = [] {
    const std::u32string before = U"<!-- one -->\n\n<!-- two -->\n\n<!-- three -->\n\n<!-- four -->";
    Editor editor(cps_to_utf8(before));
    expect(fatal(!editor.document().cached_editable_order.empty()));
    const auto fallback = editor.document().cached_editable_order.front();
    expect(fatal(editor.enter_source_mode()));
    const auto offset = before.find(U"one");
    const auto selected = SourceSelection::caret(offset);
    editor.set_source_selection(selected);
    auto expected = before;
    expected.insert(offset, U"X");
    reset_core_operation_counters();
    expect(fatal(editor.execute_command(Command::InsertText(U"X"))));
    source_destination_locality();
    expect(editor.document().cached_editable_order.front() == fallback);
    source_destination_history(editor, before, expected, selected);
};

};
