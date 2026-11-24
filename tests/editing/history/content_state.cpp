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

suite editing_history_content_state_tests = [] {

"content_state_tracks_save_points_without_reusing_undone_branch_identity"_test = [] {
    Editor editor("abc");
    const auto saved = editor.content_state();
    editor.set_selection(caret(first_text(editor), 1));
    expect(editor.content_state() == saved);
    reset_core_operation_counters();
    expect(editor.execute_document_insert_text(editor.selection(), U"X").has_value());
    const auto edited = editor.content_state();
    const auto revision = editor.revision();
    expect(edited != saved);
    expect(editor.undo());
    expect(editor.content_state() == saved);
    expect(editor.redo());
    expect(editor.content_state() == edited);
    expect(editor.undo());
    expect(editor.execute_document_insert_text(editor.selection(), U"Y").has_value());
    expect(editor.revision() == revision);
    expect(editor.content_state() != saved && editor.content_state() != edited);
    expect(!editor.has_redo());
    const auto counters = read_core_operation_counters();
    expect(counters.full_document_parses == 0_u);
    expect(counters.full_document_serializations == 0_u);
    expect(editor.markdown_utf8() == "aYbc");
    Editor replacement("abc");
    expect(replacement.content_state() != saved);
};

"content_state_survives_view_switches_and_source_history_replay"_test = [] {
    Editor editor("abc");
    editor.set_selection(caret(first_text(editor), 1));
    const auto saved = editor.content_state();
    expect(editor.enter_source_mode());
    expect(editor.content_state() == saved);
    reset_core_operation_counters();
    Command input;
    input.kind = CommandKind::InsertText;
    input.text = U"X";
    expect(editor.execute_command(input));
    const auto edited = editor.content_state();
    expect(edited != saved);
    expect(editor.undo());
    expect(editor.content_state() == saved);
    expect(editor.redo());
    expect(editor.content_state() == edited);
    expect(editor.exit_source_mode());
    expect(editor.content_state() == edited);
    expect(editor.undo());
    expect(editor.content_state() == saved);
    const auto counters = read_core_operation_counters();
    expect(counters.full_document_parses == 0_u);
    expect(counters.full_document_serializations == 0_u);
    expect(editor.markdown_utf8() == "abc");
};

"default_editor_has_one_authoritative_selection"_test = [] {
    Editor editor;
    expect(fatal(bool(editor.document().root.kind == BlockKind::Document)));
    expect(fatal(bool(editor.document().root.children.size() == 1u)));
    expect(fatal(bool(editor.selection().is_caret())));
    expect(fatal(bool(editor.selection().active.container_id == editor.document().root.children.front().id)));
    expect(fatal(bool(editor.selection().active.source_offset == 0u)));
};

};
