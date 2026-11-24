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

suite editing_links_selection_tests = [] {

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
        expect(bool(find_document_block(editor.document(), first_id)->inline_content().source == U"al[pha](new)"));
        expect(bool(find_document_block(editor.document(), second_id)->inline_content().source == U"[ome](new)ga"));
        expect(link_nodes(find_document_block(editor.document(), first_id)->inline_content()).size() == 1_u);
        expect(link_nodes(find_document_block(editor.document(), second_id)->inline_content()).size() == 1_u);
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
        expect(bool(find_document_block(editor.document(), order[2])->inline_content().source
            == U"[![old](old.png)](new)"));
        expect(bool(find_document_block(editor.document(), order[3])->inline_content().source == U"[aa](new)"));
        expect(bool(find_document_block(editor.document(), order[4])->inline_content().source == U"[bb](new)"));
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
        expect(bool(find_document_block(editor.document(), order[0])->inline_content().source == U"left[](new)"));
        expect(bool(find_document_block(editor.document(), order[1])->inline_content().source == U"right"));
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
        const auto& owner = find_document_block(editor.document(), id)->inline_content();
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
        const auto left = first.inline_content().source.find(U"left") + 2;
        const auto right = first.inline_content().source.find(U"right") + 2;
        const TextPosition from{id, left}, to{id, right};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command(insert_link_command())));
        const auto& owner = find_document_block(editor.document(), id)->inline_content();
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
        const auto& owner = block->inline_content();
        expect(bool(owner.source == (owner.syntax_mode == InlineSyntaxMode::HtmlText
            ? U"<a href=\"new\"><img src='old.png' alt='pic'></a>"
            : U"[<img src='old.png' alt='pic'>](new)")));
        const auto links = link_nodes(owner);
        expect(fatal(links.size() == 1_u));
        expect(links.front()->semantics().href == "new");
        link_insert_history(editor, before, selected);
    }
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
            expect(bool(find_document_block(editor.document(), order[0])->inline_content().source
                == U"a![a bb val](new.png)"));
            expect(find_document_block(editor.document(), order[1])->inline_content().source.empty());
        } else {
            expect(bool(find_document_block(editor.document(), order[1])->inline_content().source == U"a"));
            expect(bool(find_document_block(editor.document(), order[2])->inline_content().source == U"bb"));
        }
        link_insert_history(editor, before, selected);
    }
};

};
