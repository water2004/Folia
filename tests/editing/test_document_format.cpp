#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.core.command;
import folia.core.document_format;
import folia.core.editor;
import folia.markdown.utf;
import folia.markdown.inline_cst;
import folia.markdown.selection;
import folia.markdown.serializer;
import folia.core.source_editor;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

suite document_format_tests = [] {

"explicit formatting canonicalizes block syntax but not inline source"_test = [] {
    Editor editor(
        "Title\n=====\n\n"
        "+ __one__\n"
        "+ [two](<https://example.test/a_(b)>)\n\n"
        "3) three\n"
        "4) four\n\n"
        "|left|middle|right|\n"
        "|:--|:-:|--:|\n"
        "|_a_|`b`|$c$|");

    Command format;
    format.kind = CommandKind::FormatDocument;
    expect(fatal(editor.execute_command(format)));
    expect(editor.markdown_utf8() ==
        "# Title\n\n"
        "- __one__\n"
        "- [two](<https://example.test/a_(b)>)\n\n"
        "3. three\n"
        "4. four\n\n"
        "| left | middle | right |\n"
        "| :--- | :---: | ---: |\n"
        "| _a_ | `b` | $c$ |") << editor.markdown_utf8();
};

"formatting preserves incomplete inline source and raw syntax islands"_test = [] {
    constexpr std::string_view source =
        "Heading\n-------\n\n"
        "**open [title]( \\*literal\\* &amp;\n\n"
        "~~~cpp\n"
        "# not a heading\n"
        "~~~\n\n"
        "$$\n"
        "a_{n+1}=a_n\n"
        "$$\n\n"
        "<div align=\"center\">\n"
        "  <strong>*literal html*</strong>\n"
        "</div>";
    Editor editor{std::string(source)};

    Command format;
    format.kind = CommandKind::FormatDocument;
    expect(fatal(editor.execute_command(format)));
    const auto formatted = editor.markdown_utf8();
    expect(formatted.starts_with("## Heading\n\n**open [title]( \\*literal\\* &amp;"));
    expect(formatted.contains("~~~cpp\n# not a heading\n~~~"));
    expect(formatted.contains("$$\na_{n+1}=a_n\n$$"));
    expect(formatted.contains(
        "<div align=\"center\">\n  <strong>*literal html*</strong>\n</div>"));

    const auto& paragraph = editor.document().root.children[1];
    expect(paragraph.inline_content.source == U"**open [title]( \\*literal\\* &amp;");
    expect(flatten_tokens(paragraph.inline_content.tree, paragraph.inline_content.source)
        == paragraph.inline_content.source);
};

"formatting is idempotent and one document history transaction"_test = [] {
    Editor editor("Name\n====\n\n+ first\n+ second");
    const auto heading_id = editor.document().root.children.front().id;
    const auto before_selection = TextSelection::caret(
        {heading_id, 2, TextAffinity::Downstream});
    editor.set_selection(before_selection);
    const auto before = editor.markdown_utf8();

    Command format;
    format.kind = CommandKind::FormatDocument;
    expect(fatal(editor.execute_command(format)));
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(after == "# Name\n\n- first\n- second");
    expect(after_selection == before_selection);
    expect(!editor.execute_command(format));

    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == before);
    expect(editor.selection() == before_selection);
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
};

"source formatting maps the flat selection and undoes as one edit"_test = [] {
    constexpr std::u32string_view source = U"Name\n====\n\n+ item";
    auto before_selection = SourceSelection::caret(source.find(U"item") + 2);
    auto formatted = format_markdown_source(source, before_selection);
    expect(formatted.source == U"# Name\n\n- item");
    expect(formatted.selection == SourceSelection::caret(formatted.source.find(U"item") + 2));

    Editor editor{cps_to_utf8(source)};
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(before_selection);
    Command format;format.kind=CommandKind::FormatDocument;
    expect(fatal(editor.execute_command(format)));
    expect(editor.source_editor()->source() == U"# Name\n\n- item");
    expect(editor.source_editor()->selection() == formatted.selection);
    expect(fatal(editor.undo()));
    expect(editor.source_editor()->source() == source);
    expect(editor.source_editor()->selection() == before_selection);
    expect(fatal(editor.redo()));
    expect(editor.source_editor()->source() == U"# Name\n\n- item");
};

"nested extension containers format recursively without flattening"_test = [] {
    Editor editor(
        "> [!note] title\n"
        "> + item\n"
        ">   + nested\n\n"
        "[^ref]: first\n"
        "    + nested footnote");
    Command format;
    format.kind = CommandKind::FormatDocument;
    expect(fatal(editor.execute_command(format)));
    const auto once = editor.markdown_utf8();
    expect(once.contains("> [!NOTE] title")) << once;
    expect(once.contains("> - item")) << once;
    expect(once.contains("[^ref]: first")) << once;
    expect(once.contains("- nested footnote")) << once;
    expect(!editor.execute_command(format)) << once;
    expect(editor.markdown_utf8() == once) << once;
};

};
