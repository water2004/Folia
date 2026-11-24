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
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

void inline_insert_locality() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
    expect(counts.full_document_block_index_scans == 0_u);
}

void inline_insert_history(Editor& editor, const std::string& before, TextSelection selected) {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (const auto* owner = editable_inline_document(block))
            expect(bool(flatten_tokens(owner->tree, owner->source) == owner->source));
    });
    const auto* active = find_document_block(editor.document(), after_selection.active.container_id);
    expect(fatal(active != nullptr));
    const auto visible = active ? editable_block_source_range(*active) : std::nullopt;
    expect(fatal(visible.has_value()));
    if (visible) {
        expect(after_selection.active.source_offset >= visible->start);
        expect(after_selection.active.source_offset <= visible->end);
    }
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

Command inline_insert_command(bool image) {
    Command command;
    command.kind = image ? CommandKind::InsertImage : CommandKind::InsertLink;
    command.href = command.path = U"new.png";
    return command;
}

suite inline_insert_acceptance_tests = [] {

"inline_insert_raw_selections_write_literal_syntax_inside_fences"_test = [] {
    const std::vector<std::string> sources{
        "```cpp\nvalue\n```", "~~~text\r\nvalue\r\n~~~", "```\rvalue\r```",
        "$$\nvalue\n$$", "```cpp\nvalue", "$$\nvalue", "    value",
        "> ```cpp\n> value\n> ```", "- ```cpp\n  value\n  ```",
    };
    for (const auto& source : sources)
    for (const auto image : {false, true})
    for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto id = editor.document().cached_editable_order.front();
        const auto* block = find_document_block(editor.document(), id);
        expect(fatal(block != nullptr && editable_raw_block_source(*block) != nullptr)) << source;
        if (!block || !editable_raw_block_source(*block)) continue;
        const auto kind = block->kind;
        const auto original_source = *editable_raw_block_source(*block);
        const auto start = original_source.find(U"value");
        expect(fatal(start != std::u32string::npos));
        const TextPosition from{id, start}, to{id, start + 5};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(inline_insert_command(image)))) << source;
        inline_insert_locality();
        block = find_document_block(editor.document(), id);
        expect(fatal(block != nullptr && block->kind == kind));
        const auto replacement = image ? U"![value](new.png)" : U"[value](new.png)";
        auto expected = original_source;
        expected.replace(start, 5, replacement);
        expect(bool(*editable_raw_block_source(*block) == expected)) << source;
        const auto after = editor.markdown_utf8();
        Editor reloaded(after);
        const auto* reloaded_block = find_document_block(reloaded.document(),
            reloaded.document().cached_editable_order.front());
        expect(fatal(reloaded_block != nullptr && reloaded_block->kind == kind));
        expect(bool(*editable_raw_block_source(*reloaded_block) == expected)) << after;
        inline_insert_history(editor, before, selected);
    }
};

"inline_insert_empty_and_unclosed_fences_keep_generated_content_in_the_body"_test = [] {
    struct Case { std::string before; std::string prefix; std::string suffix; };
    const std::vector<Case> cases{
        {"```cpp", "```cpp\n", ""},
        {"```cpp\n", "```cpp\n", "\n"},
        {"```cpp\n```", "```cpp\n", "\n```"},
        {"$$", "$$", ""},
        {"$$\n$$", "$$\n", "\n$$"},
    };
    for (const auto& entry : cases)
    for (const auto image : {false, true}) {
        Editor editor(entry.before);
        const auto id = editor.document().cached_editable_order.front();
        const auto* block = find_document_block(editor.document(), id);
        expect(fatal(block != nullptr && editable_raw_block_source(*block) != nullptr)) << entry.before;
        if (!block || !editable_raw_block_source(*block)) continue;
        const auto visible = *editable_block_source_range(*block);
        const auto selected = TextSelection::caret({id, visible.start});
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(inline_insert_command(image)))) << entry.before;
        inline_insert_locality();
        const auto expected = entry.prefix + (image ? "![](new.png)" : "[](new.png)") + entry.suffix;
        expect(editor.markdown_utf8() == expected) << entry.before << editor.markdown_utf8();
        inline_insert_history(editor, before, selected);
    }
};

"inline_insert_selected_atomic_images_keep_their_flow_position_and_label"_test = [] {
    for (const auto source : {"before\n\n![old](old.png)\n\nafter",
            "before\n\n> ![old](old.png)\n\nafter",
            "before\n\n- ![old](old.png)\n- tail\n\nafter"})
    for (const auto image : {false, true})
    for (const auto reversed : {false, true}) {
        Editor editor(source);
        NodeId id{};
        walk_blocks(editor.document().root, [&](const BlockNode& block) {
            if (block.kind == BlockKind::ImageBlock) id = block.id;
        });
        expect(fatal(id.v != 0));
        const auto order = editor.document().cached_editable_order;
        const auto path = document_block_path(editor.document(), id);
        const TextPosition from{id, 0}, to{id, 1};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(inline_insert_command(image)))) << source;
        inline_insert_locality();
        expect(bool(editor.document().cached_editable_order == order));
        expect(bool(document_block_path(editor.document(), id) == path));
        const auto* replacement = find_document_block(editor.document(), id);
        expect(fatal(replacement != nullptr && editable_inline_document(*replacement) != nullptr));
        expect(bool(replacement->inline_content().source
            == (image ? U"![old](new.png)" : U"[![old](old.png)](new.png)")));
        expect(editor.markdown_utf8().find("before") < editor.markdown_utf8().find("new.png"));
        expect(editor.markdown_utf8().find("new.png") < editor.markdown_utf8().find("after"));
        inline_insert_history(editor, before, selected);
    }
};

"inline_insert_image_replaces_cross_owner_ranges_in_one_transaction"_test = [] {
    for (const auto source : {"alpha\n\nomega", "alpha\n\n> omega\n>\n> tail",
            "- alpha\n- omega\n- tail", "# alpha\n\n> omega"})
    for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto first = editor.document().cached_editable_order[0];
        const auto second = editor.document().cached_editable_order[1];
        const TextPosition from{first, 2}, to{second, 3};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(inline_insert_command(true)))) << source;
        inline_insert_locality();
        const auto* replacement = find_document_block(editor.document(), first);
        expect(fatal(replacement != nullptr));
        expect(bool(replacement->inline_content().source == U"al![pha ome](new.png)ga"));
        if (before.find("tail") != std::string::npos)
            expect(editor.markdown_utf8().find("tail") != std::string::npos);
        inline_insert_history(editor, before, selected);
    }
    for (const auto reversed : {false, true}) {
        Editor editor("| aa | bb |\n| --- | --- |\n| cc | dd |");
        const auto order = editor.document().cached_editable_order;
        const TextPosition from{order[0], 1}, to{order[3], 1};
        const auto selected = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(inline_insert_command(true))));
        inline_insert_locality();
        const std::vector<std::u32string> expected{U"a![a bb cc d](new.png)", U"", U"", U"d"};
        expect(bool(editor.document().cached_editable_order == order));
        for (std::size_t index = 0; index < order.size(); ++index)
            expect(bool(find_document_block(editor.document(), order[index])->inline_content().source == expected[index]));
        Editor reloaded(editor.markdown_utf8());
        expect(reloaded.document().cached_editable_order.size() == 4_u);
        for (std::size_t index = 0; index < expected.size(); ++index)
            expect(bool(find_document_block(reloaded.document(),
                reloaded.document().cached_editable_order[index])->inline_content().source == expected[index]));
        inline_insert_history(editor, before, selected);
    }
};

"inline_insert_image_alt_text_flattens_physical_line_breaks_and_escapes_delimiters"_test = [] {
    for (const auto explicit_alt : {false, true}) {
        Editor editor("first\r\nsecond]\rthird|end");
        const auto id = editor.document().cached_editable_order.front();
        const auto* block = find_document_block(editor.document(), id);
        const auto selected = range(*block, 0, block->inline_content().source.size());
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        auto command = inline_insert_command(true);
        if (explicit_alt) command.alt = U"first\r\nsecond]\rthird|end";
        expect(fatal(editor.execute_command(command)));
        const auto& owner = find_document_block(editor.document(), id)->inline_content();
        expect(bool(owner.source == U"![first second\\] third\\|end](new.png)"));
        expect(fatal(owner.tree.nodes.size() == 1_u));
        expect(owner.tree.nodes.front().kind == InlineCstKind::Image);
        inline_insert_history(editor, before, selected);
    }
};

};

} // namespace
