#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

#include "editing/support/link_insertion.hpp"

suite editing_links_retarget_tests = [] {

"link_insert_retargets_existing_markdown_targets_without_rewriting_labels_or_titles"_test = [] {
    const std::vector<std::pair<std::string, std::u32string>> cases{
        {"pre [**label**](<old> 'old title') post", U"pre [**label**](<new> 'old title') post"},
        {"pre [label]( old\t'old title' ) post", U"pre [label]( new\t'old title' ) post"},
        {"pre [label]( 'old title') post", U"pre [label]( new 'old title') post"},
        {"pre [label](old) post", U"pre [label](new) post"},
        {"pre [label][r] post\n\n[r]: old 'title'", U"pre [label](new) post"},
        {"pre <https://old.test> post", U"pre [https://old.test](new) post"},
        {"pre https://old.test post", U"pre [https://old.test](new) post"},
    };
    for (const auto& [source, expected] : cases)
    for (const auto select_label : {false, true}) {
        Editor editor(source);
        const auto& first = first_text(editor);
        const auto nodes = link_nodes(first.inline_content);
        expect(fatal(nodes.size() == 1_u)) << source;
        const auto content = nodes.front()->delimiter_ranges().content;
        const auto selected = select_label
            ? range(first, content.start + 1, content.end - 1) : caret(first, content.start + 1);
        const auto id = first.id;
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(insert_link_command()))) << source;
        link_insert_locality();
        const auto& owner = find_document_block(editor.document(), id)->inline_content;
        expect(bool(owner.source == expected)) << cps_to_utf8(owner.source);
        const auto links = link_nodes(owner);
        expect(fatal(links.size() == 1_u));
        expect(links.front()->semantics().href == "new");
        if (source.find("[r]:") != std::string::npos)
            expect(editor.markdown_utf8().find("[r]: old 'title'") != std::string::npos);
        link_insert_history(editor, before, selected);
    }
};

"link_insert_html_retarget_preserves_attributes_and_source_quote_spelling"_test = [] {
    const std::vector<std::pair<std::string, std::u32string>> cases{
        {"<p>pre <A DATA-x='keep' HREF='old' title='unchanged'>label</A> post</p>",
            U"pre <A DATA-x='keep' HREF='new&amp;a&#39;b' title='unchanged'>label</A> post"},
        {"<p>pre <a href=old data-x='keep'>label</a> post</p>",
            U"pre <a href=\"new&amp;a&#39;b\" data-x='keep'>label</a> post"},
        {"<p>pre <a href data-x='keep'>label</a> post</p>",
            U"pre <a href=\"new&amp;a&#39;b\" data-x='keep'>label</a> post"},
        {"<p>pre <a data-x='keep'>label</a> post</p>",
            U"pre <a data-x='keep' href=\"new&amp;a&#39;b\">label</a> post"},
    };
    for (const auto& [source, expected] : cases) {
        Editor editor(source);
        const auto& first = first_text(editor);
        const auto id = first.id;
        const auto selected = caret(first, first.inline_content.source.find(U"label") + 1);
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        auto command = insert_link_command();
        command.href = U"new&a'b";
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(command)));
        link_insert_locality();
        const auto& owner = find_document_block(editor.document(), id)->inline_content;
        expect(bool(owner.source == expected)) << cps_to_utf8(owner.source);
        const auto links = link_nodes(owner);
        expect(fatal(links.size() == 1_u));
        expect(links.front()->semantics().href == "new&a'b");
        expect(editor.selection().active.source_offset == owner.source.find(U"</") );
        link_insert_history(editor, before, selected);
    }
};

"link_insert_titles_escape_table_pipes_and_physical_lines_and_retarget_idempotently"_test = [] {
    for (const auto original : {"label", "[label]()", "[label](old)", "[label](old 'previous')", "[label]( 'previous')"}) {
        Editor editor(std::string{"| "} + original + " |\n| --- |");
        const auto& first = first_text(editor);
        const auto id = first.id;
        const auto selected = range(first, 0, first.inline_content.source.size());
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        auto command = insert_link_command();
        command.title = U"a|b\r\n\"quoted\"";
        expect(fatal(editor.execute_command(command)));
        const auto& owner = find_document_block(editor.document(), id)->inline_content;
        const auto nodes = link_nodes(owner);
        expect(fatal(nodes.size() == 1_u));
        expect(nodes.front()->semantics().href == "new");
        expect(nodes.front()->semantics().title == std::optional<std::string>{"a|b\r\n\"quoted\""});
        Editor reloaded(editor.markdown_utf8());
        expect(reloaded.document().cached_editable_order.size() == 1_u);
        expect(link_nodes(first_text(reloaded).inline_content).size() == 1_u);
        link_insert_history(editor, before, selected);
    }
    for (const auto source : {"<p><a data-x='keep'>label</a></p>", "<p><a href>label</a></p>",
            "<p><a title>label</a></p>"}) {
        Editor html(source);
        const auto& owner = first_text(html);
        const auto id = owner.id;
        const auto selected = caret(owner, owner.inline_content.source.find(U"label") + 1);
        html.set_selection(selected);
        const auto before = html.markdown_utf8();
        auto command = insert_link_command();
        command.title = U"new title";
        expect(fatal(html.execute_command(command)));
        const auto& changed = find_document_block(html.document(), id)->inline_content;
        expect(bool(changed.source == (std::string_view(source).find("data-x") != std::string_view::npos
            ? U"<a data-x='keep' href=\"new\" title=\"new title\">label</a>"
            : std::string_view(source).find("href>") != std::string_view::npos
                ? U"<a href=\"new\" title=\"new title\">label</a>"
                : U"<a title=\"new title\" href=\"new\">label</a>")));
        const auto nodes = link_nodes(changed);
        expect(fatal(nodes.size() == 1_u));
        expect(nodes.front()->semantics().href == "new");
        expect(nodes.front()->semantics().title == std::optional<std::string>{"new title"});
        link_insert_history(html, before, selected);
    }
    Editor editor("[label](new 'keep')");
    const auto selected = caret(first_text(editor), 2);
    editor.set_selection(selected);
    const auto revision = editor.revision();
    expect(fatal(editor.execute_command(insert_link_command())));
    expect(editor.markdown_utf8() == "[label](new 'keep')");
    expect(editor.revision() == revision);
    expect(!editor.has_undo());
    expect(editor.selection().active.source_offset == 6_u);
    for (const auto original : {"[label](https://e.test/?x=&AMP; 'old &COPY;')",
            "[label](a\\(b\\) (old (title)))"}) {
        Editor unchanged(original);
        unchanged.set_selection(caret(first_text(unchanged), 2));
        auto command = insert_link_command();
        const auto entity_form = std::string_view(original).find("&AMP;") != std::string_view::npos;
        command.href = entity_form ? U"https://e.test/?x=&" : U"a(b)";
        command.title = entity_form ? U"old ©" : U"old (title)";
        const auto revision = unchanged.revision();
        expect(fatal(unchanged.execute_command(command)));
        expect(unchanged.markdown_utf8() == original);
        expect(unchanged.revision() == revision);
        expect(!unchanged.has_undo());
    }
};

"link_insert_semantic_destinations_titles_and_image_alts_preserve_literal_entities"_test = [] {
    for (const auto initial : {"label", "[label](old)", "<p>label</p>"})
    for (const auto image : {false, true}) {
        Editor editor(initial);
        const auto& first = first_text(editor);
        const auto id = first.id;
        const auto selected = range(first, 0, first.inline_content.source.size());
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        auto command = insert_link_command();
        command.kind = image ? CommandKind::InsertImage : CommandKind::InsertLink;
        command.href = command.path = U"https://e.test/?v=&copy;&amp;";
        command.title = U"literal &copy; &#10; &amp;";
        command.alt = U"literal &copy; &#10; &amp;";
        expect(fatal(editor.execute_command(command)));
        const auto& owner = find_document_block(editor.document(), id)->inline_content;
        const auto wanted = owner.syntax_mode == InlineSyntaxMode::HtmlText ? InlineCstKind::HtmlElement
            : image ? InlineCstKind::Image : InlineCstKind::Link;
        const auto node = std::ranges::find(owner.tree.nodes, wanted, &InlineCstNode::kind);
        expect(fatal(node != owner.tree.nodes.end()));
        expect(node->semantics().href == "https://e.test/?v=&copy;&amp;");
        if (image) expect(node->semantics().alt == "literal &copy; &#10; &amp;");
        else expect(node->semantics().title == std::optional<std::string>{"literal &copy; &#10; &amp;"});
        link_insert_history(editor, before, selected);
    }
};

"link_insert_opaque_code_and_html_label_source_is_not_escaped_as_markdown_text"_test = [] {
    for (const auto initial : {"`[x`", "`x|y`", "`x]y`", "``a`[b``",
            "pre <img src='old.png' alt='['> post", "pre <span title='a|[b'>label</span> post"}) {
        Editor editor(initial);
        const auto& first = first_text(editor);
        const auto id = first.id;
        const auto selected = range(first, 0, first.inline_content.source.size());
        const auto label = first.inline_content.source;
        const auto visible = inline_visible_text(first.inline_content);
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command(insert_link_command())));
        const auto& owner = find_document_block(editor.document(), id)->inline_content;
        expect(bool(owner.source == U"[" + label + U"](new)")) << cps_to_utf8(owner.source);
        expect(bool(inline_visible_text(owner) == visible));
        const auto links = link_nodes(owner);
        expect(fatal(links.size() == 1_u));
        expect(links.front()->semantics().href == "new");
        link_insert_history(editor, before, selected);
    }
    Editor table("| `x\\|y` |\n| --- |");
    const auto id = first_text(table).id;
    const auto selected = range(first_text(table), 0, first_text(table).inline_content.source.size());
    table.set_selection(selected);
    const auto before = table.markdown_utf8();
    expect(fatal(table.execute_command(insert_link_command())));
    expect(bool(find_document_block(table.document(), id)->inline_content.source == U"[`x\\|y`](new)"));
    Editor reloaded(table.markdown_utf8());
    expect(reloaded.document().cached_editable_order.size() == 1_u);
    link_insert_history(table, before, selected);
};

};
