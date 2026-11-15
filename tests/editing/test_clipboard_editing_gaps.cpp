#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.ids;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

bool paste_clipboard(Editor& editor, std::u32string text) {
    Command command;
    command.kind = CommandKind::Paste;
    command.text = std::move(text);
    return editor.execute_command(command);
}

void expect_clipboard_history(Editor& editor, const std::string& before, TextSelection selection) {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
    expect(editor.undo());
    expect(editor.markdown_utf8() == before);
    expect(editor.selection() == selection);
    expect(editor.redo());
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    expect(document_indexes_are_exact(editor.document()));
}

suite clipboard_editing_gap_tests = [] {

"clipboard_gap_atomic_copy_preserves_object_source_in_both_selection_directions"_test = [] {
    for (const auto source : {"![alt](asset.png \"title\")", "***", "[TOC]",
             "---\ntitle: value\n---", "[ref]: https://example.com", "<script>alert(1)</script>"})
    for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto owner = editor.document().root.children.front().id;
        const TextPosition begin{owner, 0};
        const TextPosition end{owner, 1};
        editor.set_selection(reversed ? TextSelection{end, begin} : TextSelection{begin, end});
        reset_core_operation_counters();
        const auto copied = editor.selected_markdown_cps();
        const auto counts = read_core_operation_counters();
        expect(counts.full_document_parses == 0_u);
        expect(counts.full_document_serializations == 0_u);
        expect(bool(copied == utf8_to_cps(source))) << source;
        expect(copied.find(U'\ufffc') == std::u32string::npos);
        Editor target;
        expect(fatal(paste_clipboard(target, copied)));
        expect(target.document().root.children.front().kind == editor.document().root.children.front().kind);
        expect(target.markdown_utf8() == source);
        editor.set_selection(TextSelection::caret(end));
        expect(editor.selected_markdown_cps().empty());
    }
};

"clipboard_gap_repeated_atomic_paste_retains_each_object_and_exact_history"_test = [] {
    Editor editor;
    expect(fatal(paste_clipboard(editor, U"![first](first.png)")));
    const auto first_id = editor.document().root.children.front().id;
    const auto before = editor.markdown_utf8();
    const auto before_selection = editor.selection();
    expect(fatal(paste_clipboard(editor, U"![second](second.png)")));
    expect(editor.document().root.children.size() == 2_u);
    expect(editor.document().root.children.front().id == first_id);
    expect(editor.document().root.children.back().kind == BlockKind::ImageBlock);
    expect(editor.selection().active.container_id == editor.document().root.children.back().id);
    expect(editor.selection().active.source_offset == 1_u);
    expect_clipboard_history(editor, before, before_selection);
};

"clipboard_gap_whitespace_paste_at_atomic_boundary_creates_editable_source"_test = [] {
    for (const auto offset : {0u, 1u}) {
        Editor editor("![alt](asset.png)");
        const auto image_id = editor.document().root.children.front().id;
        editor.set_selection(TextSelection::caret({image_id, offset}));
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        expect(fatal(paste_clipboard(editor, U" \t ")));
        const auto* paragraph = find_document_block(editor.document(), editor.selection().active.container_id);
        expect(fatal(paragraph != nullptr));
        if (paragraph) {
            expect(paragraph->kind == BlockKind::Paragraph);
            expect(paragraph->inline_content.source == U" \t ");
        }
        expect(editor.document().root.children.size() == 2_u);
        expect_clipboard_history(editor, before, before_selection);
    }
};

"clipboard_gap_table_paste_preserves_whitespace_and_all_line_boundaries"_test = [] {
    const std::vector<std::pair<std::u32string, std::u32string>> cases{
        {U"  x\r\ny  ", U"  x<br>y  "},
        {U" \t ", U" \t "},
        {U"\n x \n", U"<br> x <br>"},
    };
    for (const auto& [source, expected] : cases) {
        Editor editor("| old | other |\n| --- | --- |");
        const auto& cell = first_text(editor);
        const auto owner = cell.id;
        editor.set_selection(range(cell, 0, cell.inline_content.source.size()));
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        expect(fatal(paste_clipboard(editor, source)));
        expect(bool(editor.editable_source(owner) == std::optional<std::u32string>{expected}));
        expect(editor.document().root.children.front().children.front().children.size() == 2_u);
        expect_clipboard_history(editor, before, before_selection);
    }
};

"clipboard_gap_html_text_paste_stays_literal_and_undoable"_test = [] {
    for (const auto source : {U"# heading", U"- list\n- item", U"```cpp\ncode\n```", U"$$\nx\n$$"}) {
        Editor editor("<p>ab</p>");
        const auto owner = first_text(editor).id;
        editor.set_selection(TextSelection::caret({owner, 1}));
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        expect(fatal(paste_clipboard(editor, source)));
        const auto* paragraph = find_document_block(editor.document(), owner);
        expect(fatal(paragraph != nullptr));
        if (paragraph) {
            expect(paragraph->kind == BlockKind::Paragraph);
            expect(paragraph->inline_content.syntax_mode == InlineSyntaxMode::HtmlText);
            expect(bool(paragraph->inline_content.source == U"a" + std::u32string(source) + U"b"));
        }
        expect(editor.document().root.children.size() == 1_u);
        expect_clipboard_history(editor, before, before_selection);
    }
};

"clipboard_gap_table_paste_escapes_pipes_without_changing_columns_on_reload"_test = [] {
    const std::vector<std::pair<std::u32string, std::u32string>> cases{
        {U"x|y", U"x\\|y"},
        {U"x\\|y", U"x\\|y"},
        {U"`x|y`", U"`x\\|y`"},
    };
    for (const auto& [source, expected] : cases) {
        Editor editor("| old | other |\n| --- | --- |");
        const auto& cell = first_text(editor);
        const auto owner = cell.id;
        editor.set_selection(range(cell, 0, cell.inline_content.source.size()));
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        expect(fatal(paste_clipboard(editor, source)));
        expect(bool(editor.editable_source(owner) == std::optional<std::u32string>{expected}));
        Editor reloaded(editor.markdown_utf8());
        expect(reloaded.document().root.children.front().children.front().children.size() == 2_u);
        expect_clipboard_history(editor, before, before_selection);
    }
};

"clipboard_gap_plain_paragraph_paste_preserves_terminal_newline"_test = [] {
    for (const auto clipboard : {U"X\n", U" X \r\n"}) {
        Editor editor("ab");
        const auto owner = first_text(editor).id;
        editor.set_selection(TextSelection::caret({owner, 1}));
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        expect(fatal(paste_clipboard(editor, clipboard)));
        const auto expected = clipboard == std::u32string_view(U"X\n") ? U"aX\nb" : U"a X \nb";
        expect(bool(editor.editable_source(owner) == std::optional<std::u32string>{expected}));
        expect_clipboard_history(editor, before, before_selection);
    }
};

};
}
