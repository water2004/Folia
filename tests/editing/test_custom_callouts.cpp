#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_source_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.inline_cst;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

void expect_local_callout_edit() {
    const auto counters = read_core_operation_counters();
    expect(counters.full_document_parses == 0u);
    expect(counters.full_document_serializations == 0u);
    expect(counters.full_tree_transaction_diffs == 0u);
}

}

suite custom_callout_editing_tests = [] {

"custom callout commands create switch and unwrap with exact semantic history"_test = [] {
    Editor editor("body");
    const auto body_id = editor.document().root.children.front().id;
    editor.set_selection(TextSelection::caret({body_id, 2}));
    const auto initial_selection = editor.selection();
    const std::string wrapped = "> [!RESEARCH-IDEA_2]\n> body";
    reset_core_operation_counters();
    expect(fatal(editor.execute_command({.kind = CommandKind::ToggleCallout, .callout_kind = U"research-idea_2"})));
    expect_local_callout_edit();
    expect(editor.markdown_utf8() == wrapped);
    expect(editor.document().root.children.front().kind == BlockKind::Callout);
    expect(editor.document().root.children.front().container_special().callout_kind == "RESEARCH-IDEA_2");
    expect(editor.selection() == initial_selection);
    const auto callout_id = editor.document().root.children.front().id;
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == "body");
    expect(editor.selection() == initial_selection);
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == wrapped);
    expect(editor.document().root.children.front().id == callout_id);

    reset_core_operation_counters();
    expect(fatal(editor.execute_command({.kind = CommandKind::ToggleCallout, .callout_kind = U"研究笔记-2"})));
    expect_local_callout_edit();
    const std::string switched = "> [!研究笔记-2]\n> body";
    expect(editor.markdown_utf8() == switched);
    expect(editor.document().root.children.front().id == callout_id);
    expect(editor.document().root.children.front().children.front().id == body_id);
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == wrapped);
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == switched);

    reset_core_operation_counters();
    expect(fatal(editor.execute_command({.kind = CommandKind::ToggleCallout, .callout_kind = U"研究笔记-2"})));
    expect_local_callout_edit();
    expect(editor.markdown_utf8() == "body");
    expect(editor.document().root.children.front().id == body_id);
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == switched);
    expect(editor.selection() == initial_selection);
    expect(validate_document(editor.document()).empty());
};

"custom callout kind changes preserve parsed titles spacing and source selection"_test = [] {
    const std::u32string before = U"> [!my-note]  **标题**\r\n> body";
    const std::u32string after = U"> [!研究笔记-2]  **标题**\r\n> body";
    for (const auto source_mode : {false, true}) {
        Editor editor(cps_to_utf8(before));
        const auto body_id = editor.document().root.children.front().children.back().id;
        if (source_mode) {
            expect(fatal(editor.enter_source_mode()));
            editor.set_source_selection(SourceSelection::caret(before.find(U"body") + 2));
        } else editor.set_selection(TextSelection::caret({body_id, 2}));
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::ToggleCallout, .callout_kind = U"研究笔记-2"})));
        expect_local_callout_edit();
        expect(editor.markdown_utf8() == cps_to_utf8(after));
        expect(editor.document().root.children.front().container_special().callout_kind == "研究笔记-2");
        if (source_mode) expect(editor.source_editor()->selection() == SourceSelection::caret(after.find(U"body") + 2));
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == cps_to_utf8(before));
        if (source_mode) expect(editor.source_editor()->selection() == SourceSelection::caret(before.find(U"body") + 2));
        expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == cps_to_utf8(after));
        expect(validate_document(editor.document()).empty());
    }
};

"custom callout source commands compare identifiers case insensitively"_test = [] {
    Editor editor("> [!my-note_2]\n> body");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(editor.source_editor()->source().find(U"body") + 2));
    reset_core_operation_counters();
    expect(fatal(editor.execute_command({.kind = CommandKind::ToggleCallout, .callout_kind = U"MY-note_2"})));
    expect_local_callout_edit();
    expect(bool(editor.source_editor()->source() == U"body"));
    expect(editor.source_editor()->selection() == SourceSelection::caret(2));
    expect(fatal(editor.undo()));
    expect(bool(editor.source_editor()->source() == U"> [!my-note_2]\n> body"));
    expect(fatal(editor.redo()));
    expect(bool(editor.source_editor()->source() == U"body"));
};

"custom callout commands reject malformed identifiers without modifying the document"_test = [] {
    for (const auto source_mode : {false, true})
    for (const auto& kind : {U"bad type", U"[bad", U"bad]", U"bad\\type", U"bad\nline", U"bad\u00a0space"}) {
        Editor editor("body");
        if (source_mode) expect(fatal(editor.enter_source_mode()));
        const auto revision = editor.revision();
        expect(!editor.execute_command({.kind = CommandKind::ToggleCallout, .callout_kind = kind}));
        expect(editor.markdown_utf8() == "body");
        expect(editor.revision() == revision);
        expect(!editor.has_undo());
        expect(validate_document(editor.document()).empty());
    }
};

"custom callout title edits retain type marker and use block local source history"_test = [] {
    const std::string source = "> [!my-type_2] **title**\n> body";
    Editor editor(source);
    const auto title_id = editor.document().root.children.front().children.front().id;
    editor.set_selection(TextSelection::caret({title_id, 4}));
    const auto selection = editor.selection();
    reset_core_operation_counters();
    expect(fatal(editor.execute_document_insert_text(selection, U"X").has_value()));
    expect_local_callout_edit();
    expect(editor.markdown_utf8() == "> [!my-type_2] **tiXtle**\n> body");
    const auto& title = editor.document().root.children.front().children.front().inline_content;
    expect(bool(flatten_tokens(title.tree, title.source) == title.source));
    expect(editor.document().root.children.front().container_special().callout_kind == "MY-TYPE_2");
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == source);
    expect(editor.selection() == selection);
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == "> [!my-type_2] **tiXtle**\n> body");
};

};
