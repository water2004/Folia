#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.document_text_navigation;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

#include "editing/support/text_commands.hpp"

suite editing_commands_indentation_tests = [] {

"rendered_tab_reclassifies_indented_paragraphs_and_preserves_selection_direction"_test = [] {
    for (const auto source : {"alpha", "alpha\r\nbeta", "> alpha\r\n> beta", "alpha\n\nbeta"})
    for (const auto reversed : {false, true}) {
        Editor editor(source);
        expect(fatal(editor.execute_command({.kind = CommandKind::SelectAll})));
        auto selection = editor.selection();
        if (reversed) std::swap(selection.anchor, selection.active);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() != before);
        expect(!editor.selection().is_caret());
        const auto start_index = *document_editable_order_position(editor.document(), editor.selection().anchor.container_id);
        const auto end_index = *document_editable_order_position(editor.document(), editor.selection().active.container_id);
        expect(reversed ? start_index >= end_index : start_index <= end_index);
        text_command_reload_consistency(editor);
        text_command_history(editor, before, selection);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() == before) << source;
        text_command_reload_consistency(editor);
    }
    Editor caret_editor("alpha");
    const auto before = caret_editor.markdown_utf8();
    const auto selected = caret(caret_editor.document().root.children.front(), 0);
    caret_editor.set_selection(selected);
    reset_core_operation_counters();
    expect(fatal(caret_editor.execute_command({.kind = CommandKind::TabForward})));
    text_command_local_work();
    expect(caret_editor.markdown_utf8() == "    alpha");
    expect(caret_editor.document().root.children.front().kind == BlockKind::CodeBlock);
    text_command_reload_consistency(caret_editor);
    text_command_history(caret_editor, before, selected);
    expect(fatal(caret_editor.execute_command({.kind = CommandKind::TabBackward})));
    expect(caret_editor.markdown_utf8() == before);
    expect(caret_editor.document().root.children.front().kind == BlockKind::Paragraph);
};

"rendered_indentation_excludes_an_endpoint_at_the_next_line_start"_test = [] {
    for (const auto reversed : {false, true}) {
        Editor editor("alpha\r\nbeta\r\ngamma");
        const auto id = editor.document().cached_editable_order.front();
        const TextPosition start{id, 0};
        const TextPosition end{id, 7};
        const auto selected = reversed ? TextSelection{end, start} : TextSelection{start, end};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::IndentText})));
        text_command_local_work();
        expect(editor.markdown_utf8() == "    alpha\r\nbeta\r\ngamma");
        text_command_reload_consistency(editor);
        text_command_history(editor, before, selected);
    }
};

"rendered_heading_tab_indents_exact_markdown_and_backtab_restores_heading"_test = [] {
    const std::vector<std::pair<std::string, std::string>> fixtures{
        {"# alpha", "    # alpha"},
        {"## alpha ##", "    ## alpha ##"},
        {"alpha\r\n=====", "    alpha\r\n    ====="},
        {"alpha\r\nbeta\r\n---  ", "    alpha\r\n    beta\r\n    ---  "},
        {"> # alpha", ">     # alpha"},
        {"> alpha\r\n> ===", ">     alpha\r\n>     ==="},
    };
    for (const auto& [source, expected] : fixtures) for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto& heading = *find_document_block(editor.document(), editor.document().cached_editable_order.front());
        expect(fatal(heading.kind == BlockKind::Heading)) << source;
        const TextPosition start{heading.id, 0};
        const TextPosition end{heading.id, heading.inline_content().source.size()};
        const auto selected = reversed ? TextSelection{end, start} : TextSelection{start, end};
        editor.set_selection(selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() == expected) << source;
        text_command_reload_consistency(editor);
        text_command_history(editor, source, selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() == source);
        text_command_reload_consistency(editor);
    }
    for (const auto& [source, expected] : fixtures) {
        Editor editor(source);
        const auto id = editor.document().cached_editable_order.front();
        editor.set_selection(TextSelection::caret({id, 0}));
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward}))) << source;
        expect(editor.markdown_utf8() == expected);
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward}))) << source;
        expect(editor.markdown_utf8() == source);
        text_command_reload_consistency(editor);
        editor.set_selection(TextSelection::caret({id, 1}));
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward})));
        expect(find_document_block(editor.document(), id)->kind == BlockKind::Heading);
        text_command_reload_consistency(editor);
    }
};

"rendered_code_outdent_reclassifies_literal_atomic_and_container_syntax"_test = [] {
    for (const auto source : {"    ***", "    [TOC]", "    - alpha", "    <p>alpha</p>"}) {
        Editor editor(source);
        const auto& code = *find_document_block(editor.document(), editor.document().cached_editable_order.front());
        const auto selected = TextSelection::caret(text_command_content_position(code, 0));
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() == std::string(source).substr(4));
        text_command_reload_consistency(editor);
        text_command_history(editor, before, selected);
    }
};

"typing_after_empty_paragraph_indentation_activates_code_without_global_work"_test = [] {
    for (const auto prefix : {U"    ", U"\t", U" \t"}) for (const auto tab : {false, true}) {
        if (tab && std::u32string_view(prefix) != U"    ") continue;
        Editor editor("");
        const auto initial = editor.selection();
        expect(fatal(editor.execute_command(tab ? Command{.kind = CommandKind::TabForward}
            : Command::InsertText(prefix))));
        const auto blank_source = editor.markdown_utf8();
        const auto blank_selection = editor.selection();
        expect(blank_source == cps_to_utf8(prefix));
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(Command::InsertText(U"alpha"))));
        text_command_local_work();
        expect(read_core_operation_counters().block_fragment_parses == 1_u);
        expect(editor.markdown_utf8() == cps_to_utf8(std::u32string(prefix) + U"alpha"));
        const auto* code = find_document_block(editor.document(), editor.selection().active.container_id);
        expect(fatal(code != nullptr && code->kind == BlockKind::CodeBlock));
        expect(bool(code->block_source.tree().content == U"alpha"));
        text_command_reload_consistency(editor);
        text_command_history(editor, blank_source, blank_selection);
        expect(fatal(editor.undo()));
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8().empty());
        expect(editor.selection() == initial);
    }
    Editor ordinary("alpha");
    ordinary.set_selection(caret(ordinary.document().root.children.front(), 2));
    reset_core_operation_counters();
    expect(fatal(ordinary.execute_command(Command::InsertText(U"X"))));
    expect(read_core_operation_counters().block_fragment_parses == 0_u);
    text_command_local_work();
    Editor html("<p>alpha</p>");
    html.set_selection(TextSelection::caret({html.document().cached_editable_order.front(), 0}));
    reset_core_operation_counters();
    expect(fatal(html.execute_command(Command::InsertText(U"    "))));
    expect(read_core_operation_counters().block_fragment_parses == 0_u);
    text_command_local_work();
    expect(html.markdown_utf8() == "<p>    alpha</p>");
    text_command_reload_consistency(html);
};

};
