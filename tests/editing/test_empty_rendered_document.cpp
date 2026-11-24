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

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

void expect_local_empty_html_edit() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
}

void expect_valid_empty_html_editor(const Editor& editor) {
    expect(!editor.document().cached_editable_order.empty());
    const auto* owner = find_document_block(editor.document(), editor.selection().active.container_id);
    expect(owner && editable_block_source_range(*owner).has_value());
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
}

std::vector<std::pair<BlockKind, std::u32string>> editable_contents(const Editor& editor) {
    std::vector<std::pair<BlockKind, std::u32string>> result;
    for (const auto id : editor.document().cached_editable_order) {
        const auto* owner = find_document_block(editor.document(), id);
        result.emplace_back(owner->kind, editor.editable_source(id).value_or(U""));
    }
    return result;
}

} // namespace

suite empty_rendered_document_tests = [] {

"empty_rendered_html_keeps_exact_load_source_and_an_editable_destination"_test = [] {
    for (const auto source : {"<!-- note -->", "<!-- note -->\n", "<!-- note -->\r\n",
            "<!-- note -->\r", "<div></div>", "<div><section></section></div>",
            "<div><!-- note --></div>", "> <!-- note -->", "- <!-- note -->",
            "> <!-- note -->\r\n", "- <!-- note -->\r\n"}) {
        Editor editor(source);
        expect(editor.markdown_utf8() == source) << source;
        expect_valid_empty_html_editor(editor);
        expect(!editor.has_undo());
        expect(editor.enter_source_mode());
        expect(editor.exit_source_mode());
        expect(editor.markdown_utf8() == source) << source;
        expect_valid_empty_html_editor(editor);
    }
};

"empty_rendered_comments_accept_typing_and_enter_as_one_reversible_edit"_test = [] {
    for (const auto source : {"<!-- note -->", "<!-- note -->\n", "<!-- note -->\r\n",
            "<!-- note -->\r", "> <!-- note -->", "- <!-- note -->",
            "> <!-- note -->\r\n", "- <!-- note -->\r\n"})
    for (const auto kind : {CommandKind::InsertText, CommandKind::InsertNewline}) {
        Editor editor(source);
        const auto before = editor.markdown_utf8();
        const auto selected = editor.selection();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = kind, .text = U"next"})))
            << source << static_cast<int>(kind);
        expect_local_empty_html_edit();
        const auto after = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        expect(after != before) << source << static_cast<int>(kind);
        expect(after.starts_with(before)) << source << static_cast<int>(kind) << after;
        const auto* owner = find_document_block(editor.document(), editor.selection().active.container_id);
        expect(fatal(owner && owner->kind == BlockKind::Paragraph));
        expect(bool(owner->inline_content().source == (kind == CommandKind::InsertText ? U"next" : U"")));
        expect_valid_empty_html_editor(editor);
        Editor reloaded(after);
        expect(reloaded.markdown_utf8() == after);
        expect(bool(editable_contents(reloaded) == editable_contents(editor))) << source << after;
        reset_core_operation_counters();
        expect(fatal(editor.undo()));
        expect_local_empty_html_edit();
        expect(editor.markdown_utf8() == before);
        expect(editor.selection() == selected);
        expect_valid_empty_html_editor(editor);
        expect(!editor.has_undo());
        reset_core_operation_counters();
        expect(fatal(editor.redo()));
        expect_local_empty_html_edit();
        expect(editor.markdown_utf8() == after);
        expect(editor.selection() == after_selection);
        expect_valid_empty_html_editor(editor);
    }
};

"empty_rendered_html_enter_persists_explicit_paragraph_boundaries"_test = [] {
    struct Case { const char* before; const char* after; std::size_t offset; };
    for (const auto& item : std::array{
            Case{"<div></div>", "<div><p></p><p></p></div>", 0},
            Case{"<div><section></section></div>", "<div><section><p></p><p></p></section></div>", 0},
            Case{"<div><!-- note --></div>", "<div><p></p><p><!-- note --></p></div>", 0},
            Case{"<div>alpha</div>", "<div><p>al</p><p>pha</p></div>", 2},
            Case{"<div><p align='right'>abc</p></div>", "<div><p align='right'>a</p><p>bc</p></div>", 1},
            Case{"<div><summary data-x='a'>abc</summary></div>", "<div><summary data-x='a'>a</summary><p>bc</p></div>", 1},
            Case{"<div><dt class='term'>abc</dt></div>", "<div><dt class='term'>a</dt><p>bc</p></div>", 1}}) {
        Editor editor(item.before);
        auto selected = editor.selection();
        selected.anchor.source_offset = selected.active.source_offset = item.offset;
        editor.set_selection(selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_document_enter(selected).has_value())) << item.before;
        expect_local_empty_html_edit();
        const auto after = editor.markdown_utf8();
        expect(after == item.after) << item.before << after;
        const auto after_selection = editor.selection();
        expect_valid_empty_html_editor(editor);
        Editor reloaded(after);
        expect(reloaded.markdown_utf8() == after);
        expect(bool(editable_contents(reloaded) == editable_contents(editor))) << item.before;
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == item.before);
        expect(editor.selection() == selected);
        expect_valid_empty_html_editor(editor);
        expect(!editor.has_undo());
        expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == after);
        expect(editor.selection() == after_selection);
        expect_valid_empty_html_editor(editor);
        expect(fatal(editor.execute_command(Command::InsertText(U"next"))));
        const auto typed = editor.markdown_utf8();
        Editor typed_reload(typed);
        expect(typed_reload.markdown_utf8() == typed);
        expect(bool(editable_contents(typed_reload) == editable_contents(editor)));
    }
};

"empty_rendered_comments_history_replays_across_source_and_rendered_views"_test = [] {
    for (const auto source : {"<!-- note -->", "> <!-- note -->\r\n", "- <!-- note -->"}) {
        Editor editor(source);
        const auto before = editor.selection();
        expect(fatal(editor.execute_command(Command::InsertText(U"next"))));
        const auto after = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        expect(fatal(editor.enter_source_mode()));
        reset_core_operation_counters();
        expect(fatal(editor.undo()));
        expect_local_empty_html_edit();
        expect(editor.markdown_utf8() == source);
        expect(fatal(editor.exit_source_mode()));
        expect(editor.selection() == before);
        expect_valid_empty_html_editor(editor);
        expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == after);
        expect(editor.selection() == after_selection);
        expect(fatal(editor.enter_source_mode()));
        expect(fatal(editor.undo()));
        expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == after);
        expect(fatal(editor.exit_source_mode()));
        expect(editor.selection() == after_selection);
        expect_valid_empty_html_editor(editor);
    }
};

"empty_rendered_comments_remain_editable_after_deleting_their_only_visible_text"_test = [] {
    Editor editor("<!-- note -->\n\nvisible");
    const auto selected = range(first_text(editor), 0, 7);
    editor.set_selection(selected);
    expect(fatal(editor.execute_command({.kind = CommandKind::DeleteSelection})));
    const auto empty = editor.markdown_utf8();
    expect(empty.starts_with("<!-- note -->"));
    expect_valid_empty_html_editor(editor);
    expect(fatal(editor.execute_command(Command::InsertText(U"again"))));
    const auto after = editor.markdown_utf8();
    expect(after == "<!-- note -->\n\nagain");
    Editor reloaded(after);
    expect(bool(editable_contents(reloaded) == editable_contents(editor)));
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == empty);
    expect_valid_empty_html_editor(editor);
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == "<!-- note -->\n\nvisible");
    expect(editor.selection() == selected);
};


"empty_rendered_comments_survive_removal_of_all_visible_atomic_owners"_test = [] {
    for (const bool reversed : {false, true}) {
        Editor editor("<!-- note -->\n\n![a](a.png)\n\n---");
        const auto before = editor.markdown_utf8();
        const auto& order = editor.document().cached_editable_order;
        expect(fatal(order.size() == 2_u));
        TextSelection selected{{order.front(), 0, TextAffinity::Downstream},
            {order.back(), 1, TextAffinity::Upstream}};
        if (reversed) std::swap(selected.anchor, selected.active);
        editor.set_selection(selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::DeleteSelection})));
        expect_local_empty_html_edit();
        expect(editor.markdown_utf8() == "<!-- note -->");
        expect_valid_empty_html_editor(editor);
        const auto empty_selection = editor.selection();
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == before);
        expect(editor.selection() == selected);
        expect(document_indexes_are_exact(editor.document()));
        expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == "<!-- note -->");
        expect(editor.selection() == empty_selection);
        expect_valid_empty_html_editor(editor);
        expect(fatal(editor.execute_command(Command::InsertText(U"again"))));
        expect(editor.markdown_utf8() == "<!-- note -->\n\nagain");
        expect(fatal(editor.enter_source_mode()));
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == "<!-- note -->");
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == before);
        expect(fatal(editor.exit_source_mode()));
        expect(editor.selection() == selected);
        expect_valid_empty_html_editor(editor);
    }
};

};
