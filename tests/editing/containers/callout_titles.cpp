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

suite editing_containers_callout_titles_tests = [] {

"callout_titles_are_inline_source_owners_with_local_history"_test = [] {
    Editor editor("> [!NOTE] _title_\n> body");
    const auto callout_id = editor.document().root.children.front().id;
    const auto* callout = find_block(editor.document().root, callout_id);
    expect(fatal(bool(callout != nullptr)));
    expect(fatal(bool(callout && callout->kind == BlockKind::Callout)));
    const auto* title = callout ? callout_title_block(*callout) : nullptr;
    expect(fatal(bool(title != nullptr)));
    if (!callout || !title) return;
    const auto title_id = title->id;
    expect(fatal(bool(title->inline_content().source == U"_title_")));

    const auto before_selection = TextSelection::caret(
        {title_id, 1, TextAffinity::Downstream});
    editor.set_selection(before_selection);
    reset_core_operation_counters();
    expect(fatal(bool(editor.execute_document_insert_text(editor.selection(), U"X").has_value())));
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(counters.inline_reparses == 1u)));

    callout = find_block(editor.document().root, callout_id);
    title = callout ? callout_title_block(*callout) : nullptr;
    expect(fatal(bool(title != nullptr)));
    if (!callout || !title) return;
    expect(fatal(bool(title->inline_content().source == U"_Xtitle_")));
    expect(fatal(bool(flatten_tokens(title->inline_content().tree, title->inline_content().source)
        == title->inline_content().source)));
    const auto after_selection = editor.selection();
    expect(fatal(bool(after_selection.active == TextPosition{
        title_id, 2, TextAffinity::Downstream})));
    expect(fatal(bool(editor.markdown_utf8() == "> [!NOTE] _Xtitle_\n> body")));

    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.selection() == before_selection)));
    expect(fatal(bool(editor.markdown_utf8() == "> [!NOTE] _title_\n> body")));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.selection() == after_selection)));
    expect(fatal(bool(editor.markdown_utf8() == "> [!NOTE] _Xtitle_\n> body")));
};

"callout_title_tree_splits_restore_exactly_through_history"_test = [] {
    Editor editor("> [!NOTE] title\n> body");
    const auto callout_id = editor.document().root.children.front().id;
    const auto title_id = editor.document().root.children.front().children.front().id;
    const auto before_selection = TextSelection::caret(
        {title_id, 2, TextAffinity::Downstream});
    editor.set_selection(before_selection);
    reset_core_operation_counters();
    expect(fatal(bool(editor.execute_document_enter(editor.selection()).has_value())));
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    const auto after_selection = editor.selection();
    expect(fatal(bool(after_selection.active.container_id != title_id)));
    expect(fatal(bool(editor.markdown_utf8() == "> [!NOTE] ti\n> tle\n>\n> body")));

    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == "> [!NOTE] title\n> body")));
    expect(fatal(bool(editor.selection() == before_selection)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == "> [!NOTE] ti\n> tle\n>\n> body")));
    expect(fatal(bool(editor.selection() == after_selection)));
};

};
