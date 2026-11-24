#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.inline_document;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace { suite clipboard_boundary_gap_tests = [] {

"clipboard_boundary_gap_copy_retains_selected_empty_paragraphs"_test = [] {
    for (const auto source : {"alpha\n\n\nbeta", "alpha\n\n\n\nbeta"}) {
        Editor editor(source);
        const auto& first = editor.document().root.children.front();
        const auto& last = editor.document().root.children.back();
        editor.set_selection({{first.id, 0}, {last.id, last.inline_content().source.size()}});
        expect(bool(editor.selected_markdown_cps() == utf8_to_cps(source))) << source;
    }
};

"clipboard_boundary_gap_copy_preserves_paragraph_boundary_only_ranges"_test = [] {
    struct Case { std::size_t start; std::size_t end; std::u32string expected; };
    const std::vector<Case> cases{
        {5, 0, U"\n\n"},
        {5, 2, U"\n\nbe"},
        {2, 0, U"pha\n\n"},
    };
    for (const auto& entry : cases)
    for (const auto reversed : {false, true}) {
        Editor editor("alpha\n\nbeta");
        const auto first = editor.document().root.children.front().id;
        const auto last = editor.document().root.children.back().id;
        const auto begin = TextPosition{first, entry.start};
        const auto end = TextPosition{last, entry.end};
        editor.set_selection(reversed ? TextSelection{end, begin} : TextSelection{begin, end});
        expect(bool(editor.selected_markdown_cps() == entry.expected));
    }
};

"clipboard_boundary_gap_copy_keeps_generated_empty_endpoint_separator"_test = [] {
    for (const auto with_text : {false, true})
    for (const auto reversed : {false, true}) {
        Editor editor;
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(fatal(editor.execute_command(enter)));
        if (with_text) expect(fatal(editor.execute_command(Command::InsertText(U"tail"))));
        const auto& blocks = editor.document().root.children;
        expect(fatal(blocks.size() == 2_u));
        if (blocks.size() != 2) continue;
        expect(blocks.front().inline_content().source.empty());
        const auto begin = TextPosition{blocks.front().id, 0};
        const auto end = TextPosition{blocks.back().id, blocks.back().inline_content().source.size()};
        editor.set_selection(reversed ? TextSelection{end, begin} : TextSelection{begin, end});
        const auto expected = with_text ? U"\ntail" : U"\n";
        expect(bool(editor.selected_markdown_cps() == expected));
    }
};

"clipboard_boundary_gap_atomic_edges_copy_only_selected_separator"_test = [] {
    Editor editor("![alt](asset.png)\n\ntext");
    const auto image = editor.document().root.children.front().id;
    const auto text = editor.document().root.children.back().id;
    editor.set_selection({{image, 1}, {text, 0}});
    expect(editor.selected_markdown_cps() == U"\n\n");
    editor.set_selection({{image, 1}, {text, 2}});
    expect(editor.selected_markdown_cps() == U"\n\nte");
};

"clipboard_boundary_gap_atx_paste_preserves_terminal_newline_and_caret"_test = [] {
    for (const auto clipboard : {U"X\n", U"X\r\n", U"X\nY\n"}) {
        Editor editor("# head-tail");
        const auto heading = editor.document().root.children.front().id;
        editor.set_selection(TextSelection::caret({heading, 4}));
        const auto before_selection = editor.selection();
        Command paste;
        paste.kind = CommandKind::Paste;
        paste.text = clipboard;
        expect(fatal(editor.execute_command(paste)));
        const auto& blocks = editor.document().root.children;
        expect(fatal(blocks.size() == 2_u));
        if (blocks.size() != 2) continue;
        expect(blocks.front().kind == BlockKind::Heading);
        expect(blocks.front().inline_content().source == U"headX");
        expect(blocks.back().kind == BlockKind::Paragraph);
        const auto multiline = std::u32string_view(clipboard).find(U'Y') != std::u32string_view::npos;
        expect(bool(blocks.back().inline_content().source == (multiline ? U"Y\n-tail" : U"-tail")));
        expect(editor.selection().active.container_id == blocks.back().id);
        expect(editor.selection().active.source_offset == (multiline ? 2u : 0u));
        const auto after = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        expect(editor.undo());
        expect(editor.markdown_utf8() == "# head-tail");
        expect(editor.selection() == before_selection);
        expect(editor.redo());
        expect(editor.markdown_utf8() == after);
        expect(editor.selection() == after_selection);
        expect(validate_document(editor.document()).empty());
    }
};

}; }
