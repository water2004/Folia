#include "support/folia_test.hpp"
import folia.core.editor;
import folia.core.command;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.inline_cst;
import folia.markdown.instrumentation;

using namespace folia;
using namespace boost::ut;

namespace { suite semantic_marker_tests = [] {
"paired formatting marker deletion removes the pair and preserves content and undo"_test = [] {
    for (auto spelling : {"*abc*", "**abc**", "_abc_", "__abc__", "~~abc~~", "`abc`", "``abc``", "````abc```` tail", "$abc$", "\\(abc\\)"})
    for (bool backward : {false, true}) {
        Editor editor(std::string{spelling} + "\n");
        auto const& block = editor.document().root.children.front();
        auto owner = block.id;
        auto const& node = block.inline_content.tree.nodes.front();
        auto content = node.delimiter_ranges().content;
        auto offset = backward ? content.start : content.end;
        auto before = editor.markdown_utf8();
        auto selection = TextSelection::caret({owner, offset, TextAffinity::Downstream});
        editor.set_selection(selection);
        reset_core_operation_counters();
        Command command;
        command.kind = backward ? CommandKind::DeleteBackward : CommandKind::DeleteForward;
        expect(editor.execute_command(command)) << spelling;
        auto counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0_u);
        expect(counters.full_document_serializations == 0_u);
        const auto expected = std::string_view{spelling}.ends_with(" tail") ? U"abc tail" : U"abc";
        expect(editor.editable_source(owner) == std::optional<std::u32string>{expected}) << spelling;
        auto after = editor.selection();
        expect(editor.undo());
        expect(static_cast<bool>(editor.markdown_utf8() == before));
        expect(editor.selection() == selection);
        expect(editor.redo());
        expect(editor.selection() == after);
        expect(editor.editable_source(owner) == std::optional<std::u32string>{expected});
    }
};

"display math marker deletion unwraps a single block with exact source history"_test = [] {
    for (auto source : {"$$\nabc\n$$\n", "\\[\nabc\n\\]\n", "> $$\n> abc\n> $$\n", "- $$\n  abc\n  $$\n"})
    for (bool backward : {false, true}) {
        Editor editor(source);
        auto selection = editor.selection();
        auto owner = selection.active.container_id;
        auto* block = find_document_block(editor.document(), owner);
        expect(block != nullptr);
        if (!block) continue;
        auto range = editable_block_source_range(*block);
        expect(range.has_value());
        if (!range) continue;
        selection = TextSelection::caret({owner, backward ? range->start : range->end, TextAffinity::Downstream});
        editor.set_selection(selection);
        auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        Command command;
        command.kind = backward ? CommandKind::DeleteBackward : CommandKind::DeleteForward;
        expect(editor.execute_command(command)) << source;
        auto counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0_u);
        expect(counters.full_document_serializations == 0_u);
        expect(editor.editable_source(owner) == std::optional<std::u32string>{U"abc"}) << source;
        auto after = editor.markdown_utf8();
        auto afterSelection = editor.selection();
        expect(editor.undo());
        expect(static_cast<bool>(editor.markdown_utf8() == before));
        expect(editor.selection() == selection);
        expect(editor.redo());
        expect(static_cast<bool>(editor.markdown_utf8() == after));
        expect(editor.selection() == afterSelection);
    }
};

"keyboard motion skips complete paired marker interiors but not incomplete input"_test = [] {
    for (auto source : {"**abc**", "__abc__", "~~abc~~", "``abc``", "\\(abc\\)"}) {
        Editor editor(std::string{source} + "\n");
        auto owner = editor.selection().active.container_id;
        editor.set_selection(TextSelection::caret({owner, 0, TextAffinity::Downstream}));
        expect(editor.execute_command(Command::MoveRight(false)));
        expect(editor.selection().active.source_offset == 2_u);
        expect(editor.execute_command(Command::MoveLeft(false)));
        expect(editor.selection().active.source_offset == 0_u);
    }
    Editor incomplete("**abc\n");
    expect(incomplete.execute_command(Command::MoveRight(false)));
    expect(incomplete.selection().active.source_offset == 1_u);
};

"ATX heading backspace reduces the semantic level and preserves exact undo"_test = [] {
    Editor editor("  ###  title ###\n");
    auto original = editor.markdown_utf8();
    auto selection = editor.selection();
    Command erase;
    erase.kind = CommandKind::DeleteBackward;
    expect(editor.execute_command(erase));
    expect(static_cast<bool>(editor.markdown_utf8() == "  ##  title ###\n"));
    expect(editor.undo());
    expect(static_cast<bool>(editor.markdown_utf8() == original));
    expect(editor.selection() == selection);
};

"semantic marker commands keep literal code math and incomplete text editable"_test = [] {
    for (auto source : {"`**x**`", "$**x**$", "**x"}) {
        Editor editor(std::string{source} + "\n");
        auto owner = editor.selection().active.container_id;
        auto before = editor.markdown_utf8();
        auto offset = std::string_view{source} == "**x" ? 1u : 3u;
        editor.set_selection(TextSelection::caret({owner, offset, TextAffinity::Downstream}));
        auto selection = editor.selection();
        auto expected = *editor.editable_source(owner);
        expected.erase(offset - 1, 1);
        Command erase;
        erase.kind = CommandKind::DeleteBackward;
        expect(editor.execute_command(erase));
        expect(editor.editable_source(owner) == std::optional<std::u32string>{expected});
        expect(editor.undo());
        expect(static_cast<bool>(editor.markdown_utf8() == before));
        expect(editor.selection() == selection);
    }
    Editor link("[**label**](url)\n");
    auto owner = link.selection().active.container_id;
    auto before = link.markdown_utf8();
    link.set_selection(TextSelection::caret({owner, 3, TextAffinity::Downstream}));
    Command erase;
    erase.kind = CommandKind::DeleteBackward;
    expect(link.execute_command(erase));
    expect(link.editable_source(owner) == std::optional<std::u32string>{U"[label](url)"});
    expect(link.undo());
    expect(static_cast<bool>(link.markdown_utf8() == before));
};
}; }
