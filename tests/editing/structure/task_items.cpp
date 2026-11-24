#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.core.command;
import folia.markdown.dialect;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_ids;
import folia.markdown.document_text;
import folia.core.editor;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.core.input;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.core.theme;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

suite editing_structure_task_items_tests = [] {

"typed_task_markers_keep_mixed_items_in_one_list_with_recorded_edits"_test = [] {
    Editor single;
    expect(fatal(bool(single.execute_document_insert_text(single.selection(), U"- ").has_value())));
    const auto single_before_markdown = single.markdown_utf8();
    const auto single_before = single.selection();
    reset_core_operation_counters();
    auto upgraded = single.execute_document_insert_text(single.selection(), U"[ ] ");
    expect(fatal(bool(upgraded.has_value())));
    if (!upgraded) return;
    auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(single.document().root.children.front().kind == BlockKind::TaskList)));
    expect(fatal(bool(single.selection().active.source_offset == 0u)));
    const auto single_after_markdown = single.markdown_utf8();
    const auto single_after = single.selection();
    expect(fatal(bool(single.undo())));
    expect(fatal(bool(single.markdown_utf8() == single_before_markdown)));
    expect(fatal(bool(single.selection() == single_before)));
    expect(fatal(bool(single.redo())));
    expect(fatal(bool(single.markdown_utf8() == single_after_markdown)));
    expect(fatal(bool(single.selection() == single_after)));

    Editor middle("- before\n- current\n- after");
    const auto middle_before_markdown = middle.markdown_utf8();
    const BlockNode* current = nullptr;
    walk_blocks(middle.document().root, [&](const BlockNode& block) {
        if (!current && block.kind == BlockKind::Paragraph
            && block.inline_content().source == U"current") {
            current = &block;
        }
    });
    expect(fatal(bool(current != nullptr)));
    if (!current) return;
    const auto middle_before = TextSelection::caret(
        {current->id, 0, TextAffinity::Downstream});
    middle.set_selection(middle_before);
    reset_core_operation_counters();
    auto split = middle.execute_document_insert_text(middle.selection(), U"[x] ");
    expect(fatal(bool(split.has_value())));
    if (!split) return;
    counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(middle.document().root.children.size() == 1u)));
    if (middle.document().root.children.size() == 1u) {
        const auto& list = middle.document().root.children.front();
        expect(fatal(bool(list.kind == BlockKind::List)));
        expect(fatal(bool(list.children.size() == 3u)));
        if (list.children.size() == 3u) {
            expect(fatal(bool(list.children[0].kind == BlockKind::ListItem)));
            expect(fatal(bool(list.children[1].kind == BlockKind::TaskListItem)));
            expect(fatal(bool(list.children[2].kind == BlockKind::ListItem)));
        }
    }
    const auto middle_after_markdown = middle.markdown_utf8();
    expect(fatal(bool(middle_after_markdown == "- before\n- [x] current\n- after")));
    const auto middle_after = middle.selection();
    expect(fatal(bool(middle.undo())));
    expect(fatal(bool(middle.markdown_utf8() == middle_before_markdown)));
    expect(fatal(bool(middle.selection() == middle_before)));
    expect(fatal(bool(middle.redo())));
    expect(fatal(bool(middle.markdown_utf8() == middle_after_markdown)));
    expect(fatal(bool(middle.selection() == middle_after)));
};

"ordered_task_item_upgrade_and_mixed_item_toggle_keep_item_semantics"_test = [] {
    const std::string ordered_source = "3. before\n4. current\n5. after";
    Editor ordered(ordered_source);
    const BlockNode* current = nullptr;
    walk_blocks(ordered.document().root, [&](const BlockNode& block) {
        if (!current && block.kind == BlockKind::Paragraph
            && block.inline_content().source == U"current") current = &block;
    });
    expect(fatal(current != nullptr));
    if (!current) return;
    const auto before_selection = TextSelection::caret(
        {current->id, 0, TextAffinity::Downstream});
    ordered.set_selection(before_selection);
    reset_core_operation_counters();
    auto upgraded = ordered.execute_document_insert_text(ordered.selection(), U"[x] ");
    expect(fatal(upgraded.has_value()));
    if (!upgraded) return;
    auto counters = read_core_operation_counters();
    expect(fatal(counters.full_document_parses == 0u));
    expect(fatal(counters.full_document_serializations == 0u));
    expect(fatal(counters.full_tree_transaction_diffs == 0u));
    const std::string ordered_after = "3. before\n4. [x] current\n5. after";
    expect(fatal(ordered.markdown_utf8() == ordered_after));
    expect(fatal(ordered.document().root.children.size() == 1u));
    if (ordered.document().root.children.size() == 1u) {
        const auto& list = ordered.document().root.children.front();
        expect(fatal(list.kind == BlockKind::List));
        expect(fatal(list.list_special().ordered));
        expect(fatal(list.children.size() == 3u));
        if (list.children.size() == 3u) {
            expect(fatal(list.children[0].kind == BlockKind::ListItem));
            expect(fatal(list.children[1].kind == BlockKind::TaskListItem));
            expect(fatal(list.children[2].kind == BlockKind::ListItem));
        }
    }
    const auto after_selection = ordered.selection();
    expect(fatal(ordered.undo()));
    expect(fatal(ordered.markdown_utf8() == ordered_source));
    expect(fatal(ordered.selection() == before_selection));
    expect(fatal(ordered.redo()));
    expect(fatal(ordered.markdown_utf8() == ordered_after));
    expect(fatal(ordered.selection() == after_selection));

    const std::string mixed_source = "- [@] regular\n- [x] task\n";
    Editor mixed(mixed_source);
    const BlockNode* task_text = nullptr;
    walk_blocks(mixed.document().root, [&](const BlockNode& block) {
        if (!task_text && block.kind == BlockKind::Paragraph
            && block.inline_content().source == U"task") task_text = &block;
    });
    expect(fatal(task_text != nullptr));
    if (!task_text) return;
    mixed.set_selection(TextSelection::caret(
        {task_text->id, 0, TextAffinity::Downstream}));
    const auto toggle_selection = mixed.selection();
    reset_core_operation_counters();
    auto toggled = mixed.execute_document_toggle_task_checkbox(mixed.selection());
    expect(fatal(toggled.has_value()));
    if (!toggled) return;
    counters = read_core_operation_counters();
    expect(fatal(counters.full_document_parses == 0u));
    expect(fatal(counters.full_document_serializations == 0u));
    expect(fatal(counters.full_tree_transaction_diffs == 0u));
    expect(fatal(mixed.markdown_utf8() == "- [@] regular\n- [ ] task\n"));
    expect(fatal(mixed.document().root.children.size() == 1u));
    if (mixed.document().root.children.size() == 1u) {
        const auto& list = mixed.document().root.children.front();
        expect(fatal(list.kind == BlockKind::List));
        expect(fatal(list.children.size() == 2u));
        if (list.children.size() == 2u) {
            expect(fatal(list.children[0].kind == BlockKind::ListItem));
            expect(fatal(list.children[1].kind == BlockKind::TaskListItem));
        }
    }
    expect(fatal(mixed.undo()));
    expect(fatal(mixed.markdown_utf8() == mixed_source));
    expect(fatal(mixed.selection() == toggle_selection));
    expect(fatal(mixed.redo()));
    expect(fatal(mixed.markdown_utf8() == "- [@] regular\n- [ ] task\n"));
};

"enter_in_mixed_task_item_inserts_an_unchecked_task_item"_test = [] {
    const std::string source =
        "- [@] regular\n"
        "- [x] task\n"
        "- [@] after\n";
    Editor editor(source);
    const BlockNode* paragraph = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (!paragraph && block.kind == BlockKind::Paragraph
            && block.inline_content().source == U"task") paragraph = &block;
    });
    expect(fatal(paragraph != nullptr));
    if (!paragraph) return;
    editor.set_selection(TextSelection::caret(
        {paragraph->id, paragraph->inline_content().source.size(), TextAffinity::Downstream}));
    const auto before_selection = editor.selection();
    reset_core_operation_counters();
    auto transaction = editor.execute_document_enter(editor.selection());
    expect(fatal(transaction.has_value()));
    if (!transaction) return;
    const auto counters = read_core_operation_counters();
    expect(fatal(counters.full_document_parses == 0u));
    expect(fatal(counters.full_document_serializations == 0u));
    expect(fatal(counters.full_tree_transaction_diffs == 0u));
    const std::string after =
        "- [@] regular\n"
        "- [x] task\n"
        "- [ ] \n"
        "- [@] after\n";
    expect(fatal(editor.markdown_utf8() == after));
    expect(fatal(editor.document().root.children.size() == 1u));
    if (editor.document().root.children.size() == 1u) {
        const auto& list = editor.document().root.children.front();
        expect(fatal(list.kind == BlockKind::List));
        expect(fatal(list.children.size() == 4u));
        if (list.children.size() == 4u) {
            expect(fatal(list.children[2].kind == BlockKind::TaskListItem));
            expect(fatal(!list.children[2].item_special().checked));
        }
    }
    const auto after_selection = editor.selection();
    expect(fatal(editor.undo()));
    expect(fatal(editor.markdown_utf8() == source));
    expect(fatal(editor.selection() == before_selection));
    expect(fatal(editor.redo()));
    expect(fatal(editor.markdown_utf8() == after));
    expect(fatal(editor.selection() == after_selection));
};

};
