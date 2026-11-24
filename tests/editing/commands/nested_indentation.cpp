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

suite editing_commands_nested_indentation_tests = [] {

"rendered_tab_and_backtab_preserve_raw_fences_crlf_and_container_prefixes"_test = [] {
    const std::vector<std::string> fixtures{
        "```cpp\r\nalpha\r\nbeta\r\n```", "    alpha\r\n    beta",
        "> ```cpp\r\n> alpha\r\n> beta\r\n> ```",
        "- ```cpp\r\n  alpha\r\n  beta\r\n  ```",
        "<pre>alpha\r\nbeta</pre>",
    };
    for (const auto& source : fixtures) for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto& owner = *find_document_block(editor.document(), editor.document().cached_editable_order.front());
        const auto range = *editable_block_source_range(owner);
        const TextPosition start{owner.id, range.start};
        const TextPosition end{owner.id, range.end};
        const auto selected = reversed ? TextSelection{end, start} : TextSelection{start, end};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        // Use explicit text indentation within list-owned raw blocks: a Tab
        // on the list item itself has the structural list indentation policy.
        expect(fatal(editor.execute_command({.kind = CommandKind::IndentText}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() != before);
        text_command_reload_consistency(editor);
        text_command_history(editor, before, selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::OutdentText}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() == before) << source;
        text_command_reload_consistency(editor);
    }
};

"rendered_tab_prioritizes_code_body_and_table_cells_inside_lists"_test = [] {
    for (const auto delimiter : {"```cpp", "$$"}) {
        const auto opening = std::string(delimiter);
        const auto closing = opening == "```cpp" ? "```" : "$$";
        Editor editor("- first\n- " + opening + "\n  alpha\n  " + closing);
        const auto& owner = *find_document_block(editor.document(), editor.document().cached_editable_order.back());
        expect(fatal(owner.kind == (opening == "```cpp" ? BlockKind::CodeBlock : BlockKind::MathBlock)));
        const auto selected = TextSelection::caret(text_command_content_position(owner, 0));
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward})));
        expect(editor.markdown_utf8() == "- first\n- " + opening + "\n      alpha\n  " + closing);
        expect(editor.document().root.children.front().children.size() == 2_u);
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward})));
        expect(editor.markdown_utf8() == before);
        expect(editor.document().root.children.front().children.size() == 2_u);
    }
    {
        Editor editor("- first\n- | aa | bb |\n  | --- | --- |\n  | cc | dd |");
        const auto order = editor.document().cached_editable_order;
        expect(fatal(order.size() == 5_u));
        editor.set_selection(TextSelection::caret({order[1], 0}));
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward})));
        expect(editor.selection().active.container_id == order[2]);
        expect(editor.markdown_utf8() == before);
        expect(!editor.has_undo());
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward})));
        expect(editor.selection().active.container_id == order[1]);
        expect(editor.markdown_utf8() == before);
    }
};

"source_tab_routing_stays_literal_inside_nested_code_math_and_tables"_test = [] {
    const std::vector<std::string> fixtures{
        "- first\r\n- ```cpp\r\n  alpha\r\n  ```",
        "- first\r\n- $$\r\n  alpha\r\n  $$",
        "- first\r\n- | aa | bb |\r\n  | --- | --- |\r\n  | cc | dd |",
    };
    for (const auto& source : fixtures) for (const auto reversed : {false, true}) {
        Editor editor(source);
        expect(fatal(editor.enter_source_mode()));
        const auto end = source.find("\r\n", 9);
        const auto selected = reversed ? SourceSelection{end, 9} : SourceSelection{9, end};
        editor.set_source_selection(selected);
        const auto before = editor.source_editor()->source();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward})));
        text_command_local_work();
        auto expected = before;
        expected.insert(9, U"    ");
        expect(editor.source_editor()->source() == expected);
        expect(!editor.source_editor()->selection().is_caret());
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward})));
        expect(editor.source_editor()->source() == before);
        expect(editor.source_editor()->selection() == selected);
        expect(fatal(editor.undo()));
        expect(editor.source_editor()->source() == expected);
        expect(fatal(editor.undo()));
        expect(editor.source_editor()->source() == before);
        expect(editor.source_editor()->selection() == selected);
    }
};

};
