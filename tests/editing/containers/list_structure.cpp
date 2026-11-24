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

#include "editing/support/container_editing.hpp"

suite editing_containers_list_structure_tests = [] {

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
