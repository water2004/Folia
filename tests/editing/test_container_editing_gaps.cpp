#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.core.document_edit;
import folia.core.command;
import folia.core.editor;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

const BlockNode* gap_text(const Editor& editor, std::u32string_view source) {
    const BlockNode* result = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (!result && block.kind == BlockKind::Paragraph
            && block.inline_content.source == source) result = &block;
    });
    return result;
}

std::vector<std::u32string> gap_text_order(const EditorDocument& document) {
    std::vector<std::u32string> result;
    walk_blocks(document.root, [&](const BlockNode& block) {
        if (block.kind == BlockKind::Paragraph) result.push_back(block.inline_content.source);
    });
    return result;
}

void gap_history(Editor& editor, const std::string& before, const TextSelection& selection) {
    expect(fatal(validate_document(editor.document()).empty()));
    expect(fatal(document_indexes_are_exact(editor.document())));
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == before) << editor.markdown_utf8() << " != " << before;
    expect(editor.selection() == selection);
    expect(fatal(document_indexes_are_exact(editor.document())));
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    expect(fatal(validate_document(editor.document()).empty()));
    expect(fatal(document_indexes_are_exact(editor.document())));
}

std::string gap_quote(std::string_view source) {
    std::string result = "> ";
    for (auto value : source) {
        result += value;
        if (value == '\n') result += "> ";
    }
    return result;
}

} // namespace

suite container_editing_gap_tests = [] {

"container_gap_delete_atomic_neighbor_from_nonempty_text"_test = [] {
    for (const auto atomic : {"![alt](image.png)", "***", "[TOC]"}) {
        for (const auto backward : {false, true}) {
            for (const auto quoted : {false, true}) {
                auto markdown = backward ? std::string(atomic) + "\n\ntext"
                    : std::string("text\n\n") + atomic;
                if (quoted) markdown = gap_quote(markdown);
                Editor editor(markdown);
                const auto* text = gap_text(editor, U"text");
                expect(fatal(text != nullptr)) << markdown;
                const auto id = text->id;
                const auto selection = caret(*text, backward ? 0 : 4);
                editor.set_selection(selection);
                const auto before = editor.markdown_utf8();
                reset_core_operation_counters();
                auto transaction = backward
                    ? editor.execute_document_delete_backward(selection)
                    : editor.execute_document_delete_forward(selection);
                expect(fatal(transaction.has_value())) << markdown;
                expect(read_core_operation_counters().full_document_parses == 0u);
                expect(read_core_operation_counters().full_document_serializations == 0u);
                const auto* remaining = find_block(editor.document().root, id);
                expect(fatal(remaining != nullptr));
                expect(bool(remaining->inline_content.source == U"text"));
                expect(editor.selection().active.container_id == id);
                expect(editor.selection().active.source_offset == (backward ? 0u : 4u));
                std::size_t atomics = 0;
                walk_blocks(editor.document().root, [&](const BlockNode& block) {
                    if (block.kind == BlockKind::ImageBlock || block.kind == BlockKind::ThematicBreak
                        || block.kind == BlockKind::Toc) ++atomics;
                });
                expect(atomics == 0u);
                gap_history(editor, before, selection);
            }
        }
    }
};

"container_gap_outdent_preserves_following_sibling_order_and_history"_test = [] {
    for (const auto source : {
             "- outer\n  - before\n  - chosen\n  - after\n  - last\n- tail",
             "> - outer\n>   - before\n>   - chosen\n>   - after\n>   - last\n> - tail",
             "- outer\n  - [x] before\n  - [x] chosen\n  - [ ] after\n  - last\n- tail",
             "1. outer\n\n   3) before\n   4) chosen\n   5) after\n   6) last\n2. tail"}) {
        for (const auto backspace : {false, true}) {
            Editor editor(source);
            const auto* chosen = gap_text(editor, U"chosen");
            expect(fatal(chosen != nullptr)) << source;
            const auto chosen_id = chosen->id;
            const auto selection = caret(*chosen, backspace ? 0 : 2);
            editor.set_selection(selection);
            const auto before = editor.markdown_utf8();
            const auto order = gap_text_order(editor.document());
            const auto transaction = backspace
                ? editor.execute_document_delete_backward(selection)
                : editor.execute_document_outdent_list_item(selection);
            expect(fatal(transaction.has_value())) << source;
            expect(bool(gap_text_order(editor.document()) == order)) << editor.markdown_utf8();
            const auto* chosen_item = find_parent_block(editor.document().root, chosen_id);
            expect(fatal(chosen_item != nullptr));
            expect(fatal(chosen_item->children.size() == 2u)) << editor.markdown_utf8();
            expect(chosen_item->children.back().children.size() == 2u);
            expect(bool(chosen_item->children.back().children.front().children.front().inline_content.source == U"after"));
            Editor reloaded(editor.markdown_utf8());
            expect(bool(gap_text_order(reloaded.document()) == order)) << editor.markdown_utf8();
            const auto* reloaded_chosen = gap_text(reloaded, U"chosen");
            expect(fatal(reloaded_chosen != nullptr)) << editor.markdown_utf8();
            const auto* reloaded_item = find_parent_block(reloaded.document().root, reloaded_chosen->id);
            expect(fatal(reloaded_item != nullptr && reloaded_item->children.size() == 2u)) << editor.markdown_utf8();
            expect(reloaded_item->children.back().children.size() == 2u);
            expect(editor.selection() == selection);
            gap_history(editor, before, selection);
        }
    }
};

"container_gap_list_style_commands_convert_in_place_before_toggling_off"_test = [] {
    const std::vector<std::string> sources{
        "+ alpha\n+ beta", "3) alpha\n4) beta", "- [x] alpha\n- [ ] beta"};
    for (std::size_t initial = 0; initial < sources.size(); ++initial) {
        for (std::size_t requested = 0; requested < sources.size(); ++requested) {
            if (initial == requested) continue;
            Editor editor(sources[initial]);
            const auto* text = gap_text(editor, U"alpha");
            expect(fatal(text != nullptr));
            const auto selection = caret(*text, 2);
            const auto list_id = editor.document().root.children.front().id;
            const auto first_item_id = editor.document().root.children.front().children.front().id;
            const auto before = editor.markdown_utf8();
            editor.set_selection(selection);
            reset_core_operation_counters();
            expect(fatal(editor.execute_document_toggle_list(
                selection, requested == 1, requested == 2).has_value()));
            expect(read_core_operation_counters().full_document_parses == 0u);
            expect(read_core_operation_counters().full_document_serializations == 0u);
            const auto& list = editor.document().root.children.front();
            expect(fatal(list.id == list_id));
            expect(list.kind == (requested == 2 ? BlockKind::TaskList : BlockKind::List));
            expect(list.list_special().ordered == (requested == 1));
            expect(fatal(list.children.size() == 2u));
            expect(list.children.front().id == first_item_id);
            expect(list.children.front().kind == (requested == 2 ? BlockKind::TaskListItem : BlockKind::ListItem));
            expect(bool(gap_text_order(editor.document()) == std::vector<std::u32string>{U"alpha", U"beta"}));
            expect(editor.selection() == selection);
            gap_history(editor, before, selection);

            expect(fatal(editor.execute_document_toggle_list(
                editor.selection(), requested == 1, requested == 2).has_value()));
            expect(editor.document().root.children.front().kind == BlockKind::Paragraph);
            expect(bool(gap_text_order(editor.document()) == std::vector<std::u32string>{U"alpha", U"beta"}));
        }
    }
};

"container_gap_delete_joins_next_list_item_and_keeps_its_descendants"_test = [] {
    for (const auto source : {
             "- alpha\n- beta\n  - child\n- tail",
             "> - alpha\n> - beta\n>   - child\n> - tail",
             "- [x] alpha\n- [ ] beta\n  - child\n- tail",
             "3) alpha\n4) beta\n   - child\n5) tail"}) {
        Editor editor(source);
        const auto* text = gap_text(editor, U"alpha");
        expect(fatal(text != nullptr)) << source;
        const auto id = text->id;
        const auto selection = caret(*text, 5);
        const auto before = editor.markdown_utf8();
        editor.set_selection(selection);
        reset_core_operation_counters();
        expect(fatal(editor.execute_document_delete_forward(selection).has_value())) << source;
        expect(read_core_operation_counters().full_document_parses == 0u);
        expect(read_core_operation_counters().full_document_serializations == 0u);
        expect(bool(gap_text_order(editor.document())
            == std::vector<std::u32string>{U"alphabeta", U"child", U"tail"}));
        const auto* item = find_parent_block(editor.document().root, id);
        expect(fatal(item != nullptr));
        expect(fatal(item->children.size() == 2u));
        expect(item->children.back().kind == BlockKind::List);
        expect(editor.selection().active.container_id == id);
        expect(editor.selection().active.source_offset == 5u);
        gap_history(editor, before, selection);
    }
};

"container_gap_list_style_and_indent_preserve_nested_structure_on_reload"_test = [] {
    for (const auto source : {
             "+ alpha\n\n  > quote\n\n  continuation\n+ beta",
             "- [x] alpha\n\n  - nested\n\n  continuation\n- [ ] beta"}) {
        Editor editor(source);
        const auto* text = gap_text(editor, U"alpha");
        expect(fatal(text != nullptr));
        const auto selection = caret(*text, 2);
        const auto before = editor.markdown_utf8();
        const auto order = gap_text_order(editor.document());
        editor.set_selection(selection);
        expect(fatal(editor.execute_document_toggle_list(selection, true, false).has_value()));
        Editor reloaded(editor.markdown_utf8());
        expect(bool(gap_text_order(reloaded.document()) == order)) << editor.markdown_utf8();
        expect(reloaded.document().root.children.size() == 1u) << editor.markdown_utf8();
        expect(reloaded.document().root.children.front().kind == BlockKind::List);
        expect(reloaded.document().root.children.front().list_special().ordered);
        expect(reloaded.document().root.children.front().children.front().children.size()
            == editor.document().root.children.front().children.front().children.size());
        gap_history(editor, before, selection);
    }

    Editor editor("5) first\n6) second");
    const auto* text = gap_text(editor, U"second");
    expect(fatal(text != nullptr));
    const auto selection = caret(*text, 2);
    const auto before = editor.markdown_utf8();
    editor.set_selection(selection);
    expect(fatal(editor.execute_document_indent_list_item(selection).has_value()));
    const auto& nested = editor.document().root.children.front().children.front().children.back();
    Editor reloaded(editor.markdown_utf8());
    const auto& reloaded_nested = reloaded.document().root.children.front().children.front().children.back();
    expect(nested.kind == reloaded_nested.kind);
    expect(nested.list_special().start == reloaded_nested.list_special().start);
    expect(nested.list_special().delimiter == reloaded_nested.list_special().delimiter);
    gap_history(editor, before, selection);
};

"container_gap_outdent_keeps_trailing_flow_after_nested_siblings"_test = [] {
    Editor editor("- outer\n  - before\n  - chosen\n  - after\n\n  continuation\n- tail");
    const auto* text = gap_text(editor, U"chosen");
    expect(fatal(text != nullptr));
    const auto selection = caret(*text, 2);
    const auto before = editor.markdown_utf8();
    const auto order = gap_text_order(editor.document());
    editor.set_selection(selection);
    expect(fatal(editor.execute_document_outdent_list_item(selection).has_value()));
    expect(bool(gap_text_order(editor.document()) == order)) << editor.markdown_utf8();
    // Resolve by the retained selection; the original vector reference may
    // have moved during outdent.
    const auto* item = find_parent_block(editor.document().root, selection.active.container_id);
    expect(fatal(item != nullptr));
    expect(item->children.size() == 3u) << editor.markdown_utf8();
    Editor reloaded(editor.markdown_utf8());
    expect(bool(gap_text_order(reloaded.document()) == order));
    gap_history(editor, before, selection);
};

"container_gap_inserting_first_table_row_transfers_header_role"_test = [] {
    for (const auto edit : {DocumentTableEdit::InsertRowAbove, DocumentTableEdit::InsertRowAt}) {
        Editor editor("| H1 | H2 |\n| --- | --- |\n| A | B |");
        const auto& table = editor.document().root.children.front();
        const auto header_id = table.children.front().id;
        const auto selection = caret(table.children.front().children.front(), 1);
        const auto before = editor.markdown_utf8();
        editor.set_selection(selection);
        expect(fatal(editor.execute_document_table_edit(selection, edit).has_value()));
        const auto& changed = editor.document().root.children.front();
        expect(fatal(changed.children.size() == 3u));
        expect(changed.children.front().table_special().table_header_row);
        expect(!changed.children[1].table_special().table_header_row);
        expect(changed.children[1].id == header_id);
        expect(bool(changed.children[1].children.front().inline_content.source == U"H1"));
        Editor reloaded(editor.markdown_utf8());
        const auto& loaded = reloaded.document().root.children.front();
        expect(loaded.children.size() == changed.children.size());
        for (std::size_t index = 0; index < changed.children.size(); ++index) {
            expect(loaded.children[index].table_special().table_header_row
                == changed.children[index].table_special().table_header_row);
        }
        gap_history(editor, before, selection);
    }
};

"container_gap_block_insertion_replaces_selected_text_at_its_boundary"_test = [] {
    for (const auto kind : {CommandKind::InsertCodeBlock, CommandKind::InsertMathBlock,
             CommandKind::InsertTable, CommandKind::InsertToc}) {
        for (const auto reverse : {false, true}) {
            Editor editor("prefix abc suffix");
            const auto& owner = first_text(editor);
            auto selection = range(owner, 7, 10);
            if (reverse) std::swap(selection.anchor, selection.active);
            editor.set_selection(selection);
            const auto before = editor.markdown_utf8();
            Command command;
            command.kind = kind;
            command.rows = 1;
            command.cols = 2;
            reset_core_operation_counters();
            expect(fatal(editor.execute_document_insert_atomic_block(selection, command).has_value()));
            expect(read_core_operation_counters().full_document_parses == 0u);
            expect(read_core_operation_counters().full_document_serializations == 0u);
            expect(fatal(editor.document().root.children.size() == 3u));
            const auto& blocks = editor.document().root.children;
            expect(bool(blocks.front().inline_content.source == U"prefix "));
            expect(bool(blocks.back().inline_content.source == U" suffix"));
            if (kind == CommandKind::InsertCodeBlock || kind == CommandKind::InsertMathBlock) {
                expect(blocks[1].kind == (kind == CommandKind::InsertCodeBlock
                    ? BlockKind::CodeBlock : BlockKind::MathBlock));
                expect(bool(block_source_content(blocks[1].block_source) == U"abc"));
            } else {
                expect(blocks[1].kind == (kind == CommandKind::InsertTable ? BlockKind::Table : BlockKind::Toc));
            }
            gap_history(editor, before, selection);
        }
    }
};

"container_gap_code_conversion_preserves_selected_markdown_and_safe_fences"_test = [] {
    for (const auto source : {"# **alpha**", "prefix ``` suffix", "alpha\n\nbeta"}) {
        Editor editor(source);
        const auto& first = first_text(editor);
        auto selection = range(first, 0, first.inline_content.source.size());
        if (std::string_view(source).starts_with("prefix")) selection = range(first, 7, 10);
        else if (editor.document().root.children.size() > 1) {
            selection = {{first.id, 2, TextAffinity::Downstream},
                {editor.document().root.children.back().id, 3, TextAffinity::Upstream}};
        }
        editor.set_selection(selection);
        const auto selected = editor.selected_markdown_cps();
        const auto before = editor.markdown_utf8();
        Command command;
        command.kind = CommandKind::InsertCodeBlock;
        command.lang = U"cpp";
        expect(fatal(editor.execute_document_insert_atomic_block(selection, command).has_value()));
        const BlockNode* code = nullptr;
        walk_blocks(editor.document().root, [&](const BlockNode& block) {
            if (block.kind == BlockKind::CodeBlock) code = &block;
        });
        expect(fatal(code != nullptr));
        expect(bool(block_source_content(code->block_source) == selected));
        expect(code->block_source.tree().language == std::optional<std::string>{"cpp"});
        const auto after = editor.markdown_utf8();
        Editor reloaded(after);
        const BlockNode* reloaded_code = nullptr;
        walk_blocks(reloaded.document().root, [&](const BlockNode& block) {
            if (block.kind == BlockKind::CodeBlock) reloaded_code = &block;
        });
        expect(fatal(reloaded_code != nullptr));
        expect(bool(block_source_content(reloaded_code->block_source) == selected));
        expect(reloaded.markdown_utf8() == after);
        gap_history(editor, before, selection);
    }
};

"container_gap_block_insertion_from_table_cell_uses_flow_parent"_test = [] {
    for (const auto kind : {CommandKind::InsertCodeBlock, CommandKind::InsertMathBlock,
             CommandKind::InsertTable, CommandKind::InsertToc}) {
        Editor editor("| A | B |\n| --- | --- |\n| C | D |");
        const auto& table = editor.document().root.children.front();
        const auto table_id = table.id;
        const auto selection = caret(table.children.front().children.front(), 1);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        Command command;
        command.kind = kind;
        command.rows = 1;
        command.cols = 2;
        expect(fatal(editor.execute_document_insert_atomic_block(selection, command).has_value()));
        expect(fatal(editor.document().root.children.size() == 2u));
        const auto* retained = find_block(editor.document().root, table_id);
        expect(fatal(retained != nullptr));
        expect(retained->children.size() == 2u);
        for (const auto& row : retained->children) {
            expect(row.kind == BlockKind::TableRow);
            expect(row.children.size() == 2u);
            for (const auto& cell : row.children) expect(cell.kind == BlockKind::TableCell);
        }
        expect(bool(retained->children.front().children.front().inline_content.source == U"A"));
        gap_history(editor, before, selection);
    }
};

"container_gap_code_insertion_from_cross_cell_selection_keeps_rectangular_table"_test = [] {
    Editor editor("| A | B |\n| --- | --- |\n| C | D |");
    const auto& table = editor.document().root.children.front();
    const auto table_id = table.id;
    const auto selection = TextSelection{
        {table.children.front().children.front().id, 0, TextAffinity::Downstream},
        {table.children.front().children.back().id, 1, TextAffinity::Upstream}};
    editor.set_selection(selection);
    const auto before = editor.markdown_utf8();
    Command command;
    command.kind = CommandKind::InsertCodeBlock;
    expect(fatal(editor.execute_document_insert_atomic_block(selection, command).has_value()));
    expect(fatal(editor.document().root.children.size() == 2u));
    const auto& code = editor.document().root.children.back();
    expect(code.kind == BlockKind::CodeBlock);
    expect(bool(block_source_content(code.block_source) == U"A\tB"));
    const auto* retained = find_block(editor.document().root, table_id);
    expect(fatal(retained != nullptr));
    expect(retained->children.size() == 2u);
    for (const auto& row : retained->children) expect(row.children.size() == 2u);
    expect(retained->children.front().children.front().inline_content.source.empty());
    expect(retained->children.front().children.back().inline_content.source.empty());
    gap_history(editor, before, selection);
};

"container_gap_indent_and_outdent_apply_to_selected_sibling_range"_test = [] {
    for (const auto source : {"- anchor\n- one\n- two\n- tail",
             "5) anchor\n6) one\n7) two\n8) tail",
             "- [x] anchor\n- [x] one\n- [ ] two\n- [ ] tail"}) {
        for (const auto reverse : {false, true}) {
            Editor editor(source);
            const auto* one = gap_text(editor, U"one");
            const auto* two = gap_text(editor, U"two");
            expect(fatal(one != nullptr && two != nullptr));
            auto selection = TextSelection{{one->id, 0, TextAffinity::Downstream},
                {two->id, 3, TextAffinity::Upstream}};
            if (reverse) std::swap(selection.anchor, selection.active);
            editor.set_selection(selection);
            const auto before = editor.markdown_utf8();
            const auto order = gap_text_order(editor.document());
            expect(fatal(editor.execute_document_indent_list_item(selection).has_value()));
            const auto& list = editor.document().root.children.front();
            expect(fatal(list.children.size() == 2u));
            expect(fatal(list.children.front().children.size() == 2u));
            expect(list.children.front().children.back().children.size() == 2u);
            expect(editor.selection() == selection);
            expect(bool(gap_text_order(editor.document()) == order));
            gap_history(editor, before, selection);

            const auto indented = editor.markdown_utf8();
            expect(fatal(editor.execute_document_outdent_list_item(selection).has_value()));
            expect(editor.document().root.children.front().children.size() == 4u);
            expect(editor.selection() == selection);
            expect(bool(gap_text_order(editor.document()) == order));
            Editor reloaded(editor.markdown_utf8());
            expect(reloaded.document().root.children.front().children.size() == 4u);
            expect(bool(gap_text_order(reloaded.document()) == order));
            gap_history(editor, indented, selection);
        }
    }
};

"container_gap_list_range_excludes_terminal_item_start_in_both_directions"_test = [] {
    for (const auto reverse : {false, true}) {
        Editor editor("- anchor\n- one\n- two\n- tail");
        const auto* one = gap_text(editor, U"one");
        const auto* tail = gap_text(editor, U"tail");
        expect(fatal(one != nullptr && tail != nullptr));
        const auto tail_id = tail->id;
        auto selection = TextSelection{{one->id, 0, TextAffinity::Downstream},
            {tail_id, 0, TextAffinity::Downstream}};
        if (reverse) std::swap(selection.anchor, selection.active);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_document_indent_list_item(selection).has_value()));
        const auto& list = editor.document().root.children.front();
        expect(fatal(list.children.size() == 2u));
        expect(list.children.back().children.front().id == tail_id);
        expect(list.children.front().children.back().children.size() == 2u);
        gap_history(editor, before, selection);
    }
    Editor first("- one\n- two");
    const auto& list = first.document().root.children.front();
    const auto selection = TextSelection{
        {list.children.front().children.front().id, 0, TextAffinity::Downstream},
        {list.children.back().children.front().id, 3, TextAffinity::Upstream}};
    first.set_selection(selection);
    const auto before = first.markdown_utf8();
    expect(!first.execute_document_indent_list_item(selection).has_value());
    expect(first.markdown_utf8() == before);
    expect(!first.has_undo());
};

"container_gap_outdent_range_keeps_unselected_tail_under_last_item"_test = [] {
    for (const auto reverse : {false, true}) {
        Editor editor("- parent\n  - one\n  - two\n  - tail\n- end");
        const auto* one = gap_text(editor, U"one");
        const auto* two = gap_text(editor, U"two");
        expect(fatal(one != nullptr && two != nullptr));
        auto selection = TextSelection{{one->id, 0, TextAffinity::Downstream},
            {two->id, 3, TextAffinity::Upstream}};
        if (reverse) std::swap(selection.anchor, selection.active);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        const auto order = gap_text_order(editor.document());
        expect(fatal(editor.execute_document_outdent_list_item(selection).has_value()));
        const auto& list = editor.document().root.children.front();
        expect(fatal(list.children.size() == 4u));
        expect(list.children[1].children.size() == 1u);
        expect(fatal(list.children[2].children.size() == 2u));
        expect(list.children[2].children.back().children.size() == 1u);
        expect(bool(list.children[2].children.back().children.front().children.front().inline_content.source == U"tail"));
        expect(bool(gap_text_order(editor.document()) == order));
        expect(editor.selection() == selection);
        gap_history(editor, before, selection);
    }
};

"container_gap_list_moves_reuse_existing_same_style_nested_destination"_test = [] {
    {
        Editor editor("- parent\n  - chosen\n    - child\n  - after\n- tail");
        const auto* text = gap_text(editor, U"chosen");
        expect(fatal(text != nullptr));
        const auto selection = caret(*text, 1);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_document_outdent_list_item(selection).has_value()));
        const auto* item = find_parent_block(editor.document().root, selection.active.container_id);
        expect(fatal(item != nullptr && item->children.size() == 2u));
        expect(item->children.back().children.size() == 2u);
        Editor reloaded(editor.markdown_utf8());
        const auto* reloaded_text = gap_text(reloaded, U"chosen");
        expect(fatal(reloaded_text != nullptr));
        const auto* reloaded_item = find_parent_block(reloaded.document().root, reloaded_text->id);
        expect(fatal(reloaded_item != nullptr && reloaded_item->children.size() == 2u));
        expect(reloaded_item->children.back().children.size() == 2u);
        gap_history(editor, before, selection);
    }
    {
        Editor editor("5) anchor\n6) one\n7) two");
        const auto* one = gap_text(editor, U"one");
        expect(fatal(one != nullptr));
        editor.set_selection(caret(*one, 1));
        expect(fatal(editor.execute_document_indent_list_item(editor.selection()).has_value()));
        const auto* two = gap_text(editor, U"two");
        expect(fatal(two != nullptr));
        const auto selection = caret(*two, 1);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_document_indent_list_item(selection).has_value()));
        const auto& list = editor.document().root.children.front();
        expect(fatal(list.children.size() == 1u));
        expect(fatal(list.children.front().children.size() == 2u));
        expect(list.children.front().children.back().children.size() == 2u);
        Editor reloaded(editor.markdown_utf8());
        expect(reloaded.document().root.children.front().children.front().children.size() == 2u);
        gap_history(editor, before, selection);
    }
};

"container_gap_tail_indent_and_outdent_do_not_scan_document"_test = [] {
    std::string markdown;
    for (std::size_t index = 0; index < 4096; ++index) markdown += "- item " + std::to_string(index) + "\n";
    Editor editor(markdown);
    const auto& last = editor.document().root.children.front().children.back().children.front();
    const auto selection = caret(last, 3);
    editor.set_selection(selection);
    const auto before = editor.markdown_utf8();
    for (const auto outdent : {false, true}) {
        reset_core_operation_counters();
        const auto transaction = outdent ? editor.execute_document_outdent_list_item(selection)
            : editor.execute_document_indent_list_item(selection);
        expect(fatal(transaction.has_value()));
        const auto counts = read_core_operation_counters();
        expect(counts.full_document_parses == 0u);
        expect(counts.full_document_serializations == 0u);
        expect(counts.full_document_block_index_scans == 0u);
        expect(counts.block_tree_lookup_nodes == 0u) << counts.block_tree_lookup_nodes;
    }
    expect(bool(gap_text_order(editor.document()).size() == 4096u));
    expect(editor.document().root.children.front().children.size() == 4096u);
    expect(editor.markdown_utf8() == before);
    expect(fatal(document_indexes_are_exact(editor.document())));
};


};
