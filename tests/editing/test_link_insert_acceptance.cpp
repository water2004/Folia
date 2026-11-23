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

namespace {

Command insert_link_command() {
    Command command;
    command.kind = CommandKind::InsertLink;
    command.href = U"new";
    return command;
}

const BlockNode* link_owner(const Editor& editor, std::u32string_view source) {
    const BlockNode* result = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (const auto* owner = editable_inline_document(block); owner && owner->source == source)
            result = &block;
    });
    return result;
}

std::vector<const InlineCstNode*> link_nodes(const InlineDocument& owner) {
    std::vector<const InlineCstNode*> result;
    std::vector<const InlineCstNode*> pending;
    for (const auto& node : owner.tree.nodes) pending.push_back(&node);
    while (!pending.empty()) {
        const auto* node = pending.back();
        pending.pop_back();
        if (node->kind == InlineCstKind::Link || node->kind == InlineCstKind::Autolink
            || (node->kind == InlineCstKind::HtmlElement && node->semantics().html_tag == "a"))
            result.push_back(node);
        for (const auto& child : node->children) pending.push_back(&child);
    }
    return result;
}

void link_insert_locality() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
    expect(counts.full_document_block_index_scans == 0_u);
}

void link_insert_history(Editor& editor, const std::string& before, TextSelection selected) {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (const auto* owner = editable_inline_document(block))
            expect(bool(flatten_tokens(owner->tree, owner->source) == owner->source));
    });
    Editor reloaded(after);
    expect(reloaded.markdown_utf8() == after);
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == before);
    expect(editor.selection() == selected);
    expect(document_indexes_are_exact(editor.document()));
    expect(!editor.has_undo());
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    expect(document_indexes_are_exact(editor.document()));
}

suite link_insert_acceptance_tests = [] {

"link_insert_cross_owner_ranges_link_each_label_without_replacing_structure"_test = [] {
    const std::vector<std::string> sources{
        "alpha\n\nomega", "# alpha\n\n> omega", "- alpha\n- omega",
        "- [x] alpha\n- [ ] omega", "> [!NOTE]\n> alpha\n>\n> omega",
        "[^n]: alpha\n\nomega", "| alpha | omega |\n| --- | --- |",
    };
    for (const auto& source : sources)
    for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto* first = link_owner(editor, U"alpha");
        const auto* second = link_owner(editor, U"omega");
        expect(fatal(first != nullptr && second != nullptr));
        const auto first_id = first->id, second_id = second->id;
        const auto order = editor.document().cached_editable_order;
        const TextPosition from{first_id, 2}, to{second_id, 3};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(insert_link_command()))) << source;
        link_insert_locality();
        expect(bool(editor.document().cached_editable_order == order));
        expect(bool(find_document_block(editor.document(), first_id)->inline_content.source == U"al[pha](new)"));
        expect(bool(find_document_block(editor.document(), second_id)->inline_content.source == U"[ome](new)ga"));
        expect(link_nodes(find_document_block(editor.document(), first_id)->inline_content).size() == 1_u);
        expect(link_nodes(find_document_block(editor.document(), second_id)->inline_content).size() == 1_u);
        link_insert_history(editor, before, selected);
    }
};

"link_insert_mixed_raw_image_and_table_ranges_keep_each_source_boundary"_test = [] {
    for (const auto reversed : {false, true}) {
        Editor editor("alpha\n\n```cpp\nvalue\n```\n\n![old](old.png)\n\n| aa | bb |\n| --- | --- |\n\nomega");
        const auto order = editor.document().cached_editable_order;
        expect(fatal(order.size() == 6_u));
        const TextPosition from{order.front(), 2}, to{order.back(), 3};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(insert_link_command())));
        link_insert_locality();
        expect(bool(editor.document().cached_editable_order == order));
        expect(bool(find_document_block(editor.document(), order[1])->block_source.source()
            == U"```cpp\n[value](new)\n```"));
        expect(bool(find_document_block(editor.document(), order[2])->inline_content.source
            == U"[![old](old.png)](new)"));
        expect(bool(find_document_block(editor.document(), order[3])->inline_content.source == U"[aa](new)"));
        expect(bool(find_document_block(editor.document(), order[4])->inline_content.source == U"[bb](new)"));
        link_insert_history(editor, before, selected);
    }
    for (const auto reversed : {false, true}) {
        Editor editor("left\n\nright");
        const auto order = editor.document().cached_editable_order;
        const TextPosition from{order[0], 4}, to{order[1], 0};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command(insert_link_command())));
        expect(bool(find_document_block(editor.document(), order[0])->inline_content.source == U"left[](new)"));
        expect(bool(find_document_block(editor.document(), order[1])->inline_content.source == U"right"));
        link_insert_history(editor, before, selected);
    }
};

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

"link_insert_selected_existing_links_are_flattened_without_losing_label_formats_or_images"_test = [] {
    const std::vector<std::pair<std::string, std::u32string>> cases{
        {"pre [**old**](one) mid <https://old.test> ![pic](img) post",
            U"[pre **old** mid https://old.test ![pic](img) post](new)"},
        {"pre *[old](one)* post", U"[pre *old* post](new)"},
        {"pre ![pic [old](one)](img) post", U"[pre ![pic old](img) post](new)"},
        {"[![pic](img)](old)", U"[![pic](img)](new)"},
        {"pre <a href='old'><strong>old</strong></a> post",
            U"[pre <strong>old</strong> post](new)"},
    };
    for (const auto& [source, expected] : cases)
    for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto id = editor.document().cached_editable_order.front();
        const auto* block = find_document_block(editor.document(), id);
        const auto visible = *editable_block_source_range(*block);
        const TextPosition from{id, visible.start}, to{id, visible.end};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(insert_link_command()))) << source;
        link_insert_locality();
        const auto& owner = find_document_block(editor.document(), id)->inline_content;
        expect(bool(owner.source == expected)) << cps_to_utf8(owner.source);
        const auto links = link_nodes(owner);
        expect(fatal(links.size() == 1_u)) << cps_to_utf8(owner.source);
        expect(links.front()->semantics().href == "new");
        link_insert_history(editor, before, selected);
    }
};

"link_insert_partial_link_boundaries_expand_to_complete_labels"_test = [] {
    for (const auto reversed : {false, true}) {
        Editor editor("pre [left](one) mid [right](two) post");
        const auto& first = first_text(editor);
        const auto id = first.id;
        const auto left = first.inline_content.source.find(U"left") + 2;
        const auto right = first.inline_content.source.find(U"right") + 2;
        const TextPosition from{id, left}, to{id, right};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command(insert_link_command())));
        const auto& owner = find_document_block(editor.document(), id)->inline_content;
        expect(bool(owner.source == U"pre [left mid right](new) post"));
        expect(link_nodes(owner).size() == 1_u);
        link_insert_history(editor, before, selected);
    }
};

"link_insert_selected_html_images_retain_their_authoritative_markup"_test = [] {
    for (const auto initial : {"<img src='old.png' alt='pic'>", "> <img src='old.png' alt='pic'>",
            "<div><img src='old.png' alt='pic'></div>"})
    for (const auto reversed : {false, true}) {
        Editor editor(initial);
        NodeId id{};
        walk_blocks(editor.document().root, [&](const BlockNode& block) {
            if (block.kind == BlockKind::ImageBlock) id = block.id;
            else if (const auto* owner = editable_inline_document(block);
                owner && owner->source.find(U"<img") != std::u32string::npos) id = block.id;
        });
        expect(fatal(id.v != 0)) << initial;
        const auto visible = *editable_block_source_range(*find_document_block(editor.document(), id));
        const TextPosition from{id, visible.start}, to{id, visible.end};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(insert_link_command())));
        link_insert_locality();
        const auto* block = find_document_block(editor.document(), id);
        expect(fatal(block != nullptr && editable_inline_document(*block) != nullptr));
        const auto& owner = block->inline_content;
        expect(bool(owner.source == (owner.syntax_mode == InlineSyntaxMode::HtmlText
            ? U"<a href=\"new\"><img src='old.png' alt='pic'></a>"
            : U"[<img src='old.png' alt='pic'>](new)")));
        const auto links = link_nodes(owner);
        expect(fatal(links.size() == 1_u));
        expect(links.front()->semantics().href == "new");
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

"link_insert_image_replacement_across_raw_endpoints_keeps_unselected_literal_content"_test = [] {
    for (const auto table_first : {false, true})
    for (const auto reversed : {false, true}) {
        const std::string table = "| aa | bb |\n| --- | --- |";
        const std::string code = "```cpp\nvalue\n```";
        Editor editor(table_first ? table + "\n\n" + code : code + "\n\n" + table);
        const auto order = editor.document().cached_editable_order;
        const auto raw_id = table_first ? order.back() : order.front();
        const auto raw_start = editable_block_source_range(*find_document_block(editor.document(), raw_id))->start;
        const TextPosition from{order.front(), table_first ? 1u : raw_start + 2};
        const TextPosition to{table_first ? order.back() : order[1], table_first ? raw_start + 3 : 1u};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        Command command;
        command.kind = CommandKind::InsertImage;
        command.path = U"new.png";
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(command)));
        link_insert_locality();
        expect(bool(editor.document().cached_editable_order == order));
        const auto* raw = find_document_block(editor.document(), raw_id);
        expect(bool(raw->block_source.source() == (table_first ? U"```cpp\nue\n```"
            : U"```cpp\nva![lue a](new.png)\n```")));
        if (table_first) {
            expect(bool(find_document_block(editor.document(), order[0])->inline_content.source
                == U"a![a bb val](new.png)"));
            expect(find_document_block(editor.document(), order[1])->inline_content.source.empty());
        } else {
            expect(bool(find_document_block(editor.document(), order[1])->inline_content.source == U"a"));
            expect(bool(find_document_block(editor.document(), order[2])->inline_content.source == U"bb"));
        }
        link_insert_history(editor, before, selected);
    }
};

};

} // namespace
