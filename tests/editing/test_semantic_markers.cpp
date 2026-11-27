#include "support/folia_test.hpp"
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.core.editor;
import folia.core.command;
import folia.core.document_edit;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.inline_cst;
import folia.markdown.instrumentation;

using namespace folia;
using namespace boost::ut;

namespace { suite semantic_marker_tests = [] {
"paired formatting marker deletion removes the pair and preserves content and undo"_test = [] {
    for (auto spelling : {"*abc*", "**abc**", "_abc_", "__abc__", "~~abc~~", "==abc==", "`abc`", "``abc``", "````abc```` tail", "$abc$", "\\(abc\\)"})
    for (bool backward : {false, true}) {
        Editor editor(std::string{spelling} + "\n");
        auto const& block = editor.document().root.children.front();
        auto owner = block.id;
        auto const& node = block.inline_content().tree.nodes.front();
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

"display math deletion removes exactly one whole delimiter with exact source history"_test = [] {
    const std::array sources{
        "$$\nabc\n$$\n", "\\[\r\nabc\r\n\\]\r\n", ">$$\n> abc\n>$$\n",
        "> [!NOTE]\n>$$\n> abc\n>$$\n", ">$$\n> abc\n>$$",
        "> - > \\[\n>   > \\begin{cases} a & b \\end{cases}\n>   > \\]\n", "$$$$", "\\[\\]"};
    for (std::string source : sources)
    for (bool closing : {false, true})
    for (auto command : {CommandKind::DeleteBackward, CommandKind::DeleteForward, CommandKind::DeleteSelection}) {
        Editor editor(source);
        const BlockNode* math = nullptr;
        walk_blocks(editor.document().root, [&](const BlockNode& block) {
            if (!math && block.kind == BlockKind::MathBlock) math = &block;
        });
        expect(fatal(math != nullptr)) << source;
        if (!math) continue;
        const auto id = math->id;
        const auto bracket = math->block_source.tree().kind == BlockSourceKind::BracketMath;
        const auto marker = block_source_marker_range(math->block_source,
            closing ? BlockSourceTokenKind::ClosingMarker : BlockSourceTokenKind::OpeningMarker);
        expect(fatal(marker.has_value()));
        if (!marker) continue;
        const auto before = command == CommandKind::DeleteSelection
            ? TextSelection{{id, marker->start}, {id, marker->end}}
            : TextSelection::caret({id, command == CommandKind::DeleteBackward ? marker->end : marker->start});
        editor.set_selection(before);

        auto expected = source;
        const auto physical = closing ? expected.rfind(bracket ? "\\]" : "$$")
            : expected.find(bracket ? "\\[" : "$$");
        expected.erase(physical, marker->length());
        reset_core_operation_counters();
        expect(editor.execute_command({.kind = command}));
        const auto counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0_u && counters.full_document_serializations == 0_u);
        expect(editor.markdown_utf8() == expected) << source << expected << editor.markdown_utf8();
        const auto after = editor.selection();
        expect(validate_document(editor.document()).empty());
        expect(editor.undo() && editor.markdown_utf8() == source && editor.selection() == before);
        expect(editor.redo() && editor.markdown_utf8() == expected && editor.selection() == after);
        const auto reloaded = Editor(expected).markdown_utf8();
        expect(reloaded == expected) << expected << reloaded;
    }
};

"math caret motion visits both sides of delimiters and skips their interiors"_test = [] {
    for (auto source : {"$$\r\nx\r\n$$", "\\[\r\nx\r\n\\]"}) {
        Editor editor(source);
        const auto id = editor.selection().active.container_id;
        editor.set_caret({id, 0});
        for (std::size_t stop : {2, 4, 5, 7, 9}) {
            expect(editor.execute_command(Command::MoveRight(false)));
            expect(editor.selection().active.source_offset == stop);
        }
        for (std::size_t stop : {7, 5, 4, 2, 0}) {
            expect(editor.execute_command(Command::MoveLeft(false)));
            expect(editor.selection().active.source_offset == stop);
        }
        editor.set_caret({id, 1, TextAffinity::Upstream});
        expect(editor.selection().active.source_offset == 0_u);
        editor.set_caret({id, 1, TextAffinity::Downstream});
        expect(editor.selection().active.source_offset == 2_u);
        editor.set_caret({id, 5, TextAffinity::Upstream});
        expect(editor.execute_command({.kind = CommandKind::MoveDown}));
        expect(editor.selection().active.source_offset == 7_u);
    }
};

"keyboard motion skips complete paired marker interiors but not incomplete input"_test = [] {
    for (auto source : {"**abc**", "__abc__", "~~abc~~", "==abc==", "``abc``", "\\(abc\\)"}) {
        Editor editor(std::string{source} + "\n");
        auto owner = editor.selection().active.container_id;
        editor.set_selection(TextSelection::caret({owner, 0, TextAffinity::Downstream}));
        expect(editor.execute_command(Command::MoveRight(false)));
        expect(editor.selection().active.source_offset == 2_u);
        expect(editor.execute_command(Command::MoveLeft(false)));
        expect(editor.selection().active.source_offset == 0_u);
    }
    for (auto source : {"**abc\n", "==abc\n"}) {
        Editor incomplete(source);
        expect(incomplete.execute_command(Command::MoveRight(false)));
        expect(incomplete.selection().active.source_offset == 1_u);
    }
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
