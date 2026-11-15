#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

void source_edge_history(Editor& editor, const Command& command, std::u32string_view expected) {
    const auto before = editor.source_editor()->source();
    const auto selected = editor.source_editor()->selection();
    reset_core_operation_counters();
    const auto applied = editor.execute_command(command);
    if (!applied || editor.source_editor()->source() != expected)
        std::cerr << "source edge kind " << static_cast<int>(command.kind)
            << " before " << std::quoted(cps_to_utf8(before)) << " expected " << std::quoted(cps_to_utf8(expected))
            << " actual " << std::quoted(cps_to_utf8(editor.source_editor()->source()))
            << " applied " << applied << '\n';
    expect(fatal(applied));
    const auto counters = read_core_operation_counters();
    expect(counters.full_document_parses == 0u && counters.full_document_serializations == 0u);
    expect(fatal(editor.source_editor()->source() == expected)) << cps_to_utf8(editor.source_editor()->source());
    expect(fatal(validate_document(editor.document()).empty()));
    const auto after_selection = editor.source_editor()->selection();
    expect(fatal(editor.undo()));
    expect(editor.source_editor()->source() == before);
    expect(editor.source_editor()->selection() == selected);
    expect(fatal(editor.redo()));
    expect(editor.source_editor()->source() == expected);
    expect(editor.source_editor()->selection() == after_selection);
    expect(fatal(editor.exit_source_mode()));
    expect(editor.markdown_utf8() == cps_to_utf8(expected));
    expect(fatal(editor.enter_source_mode()));
}

}

suite source_command_edge_tests = [] {

"source command edges preserve blank callout bodies and caret on unwrap"_test = [] {
    for (const auto& [source, expected] : std::vector<std::pair<std::u32string, std::u32string>>{
        {U"> [!NOTE]\n>\n> body", U"\nbody"},
        {U"> [!NOTE]\r\n>\r\n>\r\n> body", U"\r\n\r\nbody"},
        {U"> [!NOTE] title\n>\n> body\n>", U"title\n\nbody\n"},
        {U"  > [!NOTE]\n  > body", U"  body"},
    }) {
        Editor editor(cps_to_utf8(source));
        expect(fatal(editor.markdown_utf8() == cps_to_utf8(source)));
        expect(fatal(editor.enter_source_mode()));
        const auto before_offset = source.find(U"body") + 2;
        editor.set_source_selection(SourceSelection::caret(before_offset));
        source_edge_history(editor, {.kind = CommandKind::ToggleCallout, .callout_kind = U"NOTE"}, expected);
        if (editor.source_editor()->selection().active != expected.find(U"body") + 2)
            std::cerr << "source callout offset before " << before_offset << " expected " << expected.find(U"body") + 2
                << " actual " << editor.source_editor()->selection().active << '\n';
        expect(editor.source_editor()->selection().active == expected.find(U"body") + 2);
    }
};

"source command edges convert ordinary quotes into callouts at their existing level"_test = [] {
    for (const auto& [source, expected] : std::vector<std::pair<std::u32string, std::u32string>>{
        {U"> first\n> second", U"> [!TIP]\n> first\n> second"},
        {U"  > first\r\n  > second", U"  > [!TIP]\r\n  > first\r\n  > second"},
        {U"> first\n>\n> second", U"> [!TIP]\n> first\n>\n> second"},
    }) {
        Editor editor(cps_to_utf8(source));
        expect(fatal(editor.markdown_utf8() == cps_to_utf8(source)));
        expect(fatal(editor.enter_source_mode()));
        editor.set_source_selection(SourceSelection::caret(source.find(U"second") + 3));
        source_edge_history(editor, {.kind = CommandKind::ToggleCallout, .callout_kind = U"TIP"}, expected);
        expect(editor.source_editor()->selection().active == expected.find(U"second") + 3);
        expect(editor.document().root.children.front().kind == BlockKind::Callout);
        expect(editor.document().root.children.front().children.back().kind == BlockKind::Paragraph);
    }
};

"source command edges remove setext and optional ATX closing heading markers"_test = [] {
    struct Case { std::u32string source; Command command; std::u32string expected; std::size_t offset; };
    for (const auto& test : std::vector<Case>{
        {U"Title\n---", {.kind = CommandKind::ClearHeading}, U"Title", 2},
        {U"Title\r\n===\r\n\r\nbody", {.kind = CommandKind::ClearHeading}, U"Title\r\n\r\nbody", 8},
        {U"Title\n---", {.kind = CommandKind::SetHeading, .level = 3}, U"### Title", 2},
        {U"Title\n===", {.kind = CommandKind::SetHeading, .level = 2}, U"## Title", 7},
        {U"> Title\n> ---", {.kind = CommandKind::ClearHeading}, U"> Title", 4},
        {U"> Title\n> ---", {.kind = CommandKind::SetHeading, .level = 3}, U"> ### Title", 10},
        {U"## Title ##", {.kind = CommandKind::ClearHeading}, U"Title", 5},
        {U"# ###", {.kind = CommandKind::ClearHeading}, U"", 2},
    }) {
        Editor editor(cps_to_utf8(test.source));
        expect(fatal(editor.enter_source_mode()));
        editor.set_source_selection(SourceSelection::caret(test.offset));
        source_edge_history(editor, test.command, test.expected);
    }
};

"source command edges preserve an adjacent divider when clearing an ATX heading"_test = [] {
    Editor editor("## title\n\n---\n\nbody");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(4));
    source_edge_history(editor, {.kind = CommandKind::ClearHeading}, U"title\n\n---\n\nbody");
    expect(editor.document().root.children[1].kind == BlockKind::ThematicBreak);
};

"source command edges keep leading fence marker characters inside the language info"_test = [] {
    Editor editor("body");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection({0, 4});
    source_edge_history(editor, {.kind = CommandKind::InsertCodeBlock, .lang = U"~`lang"},
        U"~~~ ~`lang\nbody\n~~~");
    expect(editor.document().root.children.front().kind == BlockKind::CodeBlock);
    expect(block_source_content(editor.document().root.children.front().block_source) == U"body");
    editor.set_source_selection(SourceSelection::caret(13));
    source_edge_history(editor, {.kind = CommandKind::SetCodeBlockLanguage, .lang = U"~~next"},
        U"~~~ ~~next\nbody\n~~~");
    expect(block_source_content(editor.document().root.children.front().block_source) == U"body");
};

"source command edges toggle multiline inline code using both delimiter lines"_test = [] {
    for (const auto& source : {std::u32string{U"first\nsecond"}, std::u32string{U"first\r\nsecond"}}) {
        Editor editor(cps_to_utf8(source));
        expect(fatal(editor.enter_source_mode()));
        const auto end = source.find(U"second");
        editor.set_source_selection({0, end});
        const auto expected = U"`" + source.substr(0, end) + U"`" + source.substr(end);
        source_edge_history(editor, {.kind = CommandKind::ToggleInlineCode}, expected);
        editor.set_source_selection({1, end + 1});
        source_edge_history(editor, {.kind = CommandKind::ToggleInlineCode}, source);
    }
    Editor editor("before `first\nsecond` after");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(17));
    source_edge_history(editor, {.kind = CommandKind::ToggleInlineCode}, U"before first\nsecond after");
    expect(editor.source_editor()->selection().active == 16u);
};

"source command edges allow a footnote definition when its spelling is only literal code"_test = [] {
    Editor editor("```text\n[^note]: literal\n```\n\nbody");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(editor.source_editor()->source().size()));
    const auto before = editor.source_editor()->source();
    source_edge_history(editor, {.kind = CommandKind::CreateFootnoteDefinition, .footnote_label = "note"},
        before + U"\n\n[^note]: ");
    const auto after = editor.source_editor()->source();
    expect(!editor.execute_command({.kind = CommandKind::CreateFootnoteDefinition, .footnote_label = "note"}));
    expect(editor.source_editor()->source() == after);
};

};
