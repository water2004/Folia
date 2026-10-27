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

suite editor_block_semantic_tests = [] {

"structural_prefix_backspace_records_uniform_tree_operations"_test = [] {
    auto exercise = [](Editor& editor, NodeId owner, std::string_view label) {
        const auto before_markdown = editor.markdown_utf8();
        const auto before = TextSelection::caret({owner, 0, TextAffinity::Downstream});
        editor.set_selection(before);
        reset_core_operation_counters();
        auto transaction = editor.execute_document_delete_backward(editor.selection());
        expect(fatal(bool(transaction.has_value()))) << label << " transaction";
        if (!transaction) return;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << label;
        expect(fatal(bool(counters.full_document_serializations == 0u))) << label;
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << label;
        expect(fatal(bool(!transaction->operations.empty()))) << label;
        const auto after_markdown = editor.markdown_utf8();
        const auto after = editor.selection();
        expect(fatal(bool(editor.undo()))) << label;
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << label;
        expect(fatal(bool(editor.selection() == before))) << label;
        expect(fatal(bool(editor.redo()))) << label;
        expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << label;
        expect(fatal(bool(editor.selection() == after))) << label;
    };

    Editor heading("# title");
    exercise(heading, heading.document().root.children.front().id, "heading");

    Editor quote("> quote");
    exercise(quote, quote.document().root.children.front().children.front().id, "quote");

    Editor callout("> [!NOTE] title\n> body");
    exercise(
        callout,
        callout.document().root.children.front().children.front().id,
        "callout title");

    Editor untitled_callout("> [!NOTE]\n> body");
    exercise(
        untitled_callout,
        untitled_callout.document().root.children.front().children.front().id,
        "untitled callout");

    for (auto markdown : {
             std::string{"- item"},
             std::string{"- one\n- two\n- three"},
             std::string{"> - item"}}) {
        Editor list(markdown);
        const BlockNode* selected = nullptr;
        walk_blocks(list.document().root, [&](const BlockNode& block) {
            if (block.kind != BlockKind::Paragraph) return;
            if (!selected || block.inline_content.source == U"two") selected = &block;
        });
        expect(fatal(bool(selected != nullptr))) << markdown;
        if (selected) exercise(list, selected->id, markdown);
    }

    Editor nested_list("- parent\n  - child");
    const BlockNode* nested_child = nullptr;
    walk_blocks(nested_list.document().root, [&](const BlockNode& block) {
        if (block.kind == BlockKind::Paragraph && block.inline_content.source == U"child") {
            nested_child = &block;
        }
    });
    expect(fatal(bool(nested_child != nullptr)));
    if (nested_child) exercise(nested_list, nested_child->id, "nested list");
};

"cross_block_selection_delete_records_source_and_pruning_operations"_test = [] {
    auto exercise = [](Editor& editor, TextSelection selection, std::string_view label) {
        const auto before_markdown = editor.markdown_utf8();
        editor.set_selection(selection);
        reset_core_operation_counters();
        auto transaction = editor.execute_document_delete_selection(editor.selection());
        expect(fatal(bool(transaction.has_value()))) << label;
        if (!transaction) return;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << label << " parses";
        expect(fatal(bool(counters.full_document_serializations == 0u))) << label << " serializations";
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << label << " tree diffs";
        expect(fatal(bool(counters.full_document_text_projections == 0u))) << label << " projections";
        expect(fatal(bool(counters.full_document_block_index_scans == 0u))) << label << " index scans";
        expect(fatal(bool(counters.incremental_document_block_index_repairs == 1u))) << label << " index repairs";
        expect(fatal(bool(!transaction->operations.empty()))) << label << " operations";
        expect(fatal(document_indexes_are_exact(editor.document()))) << label << " indexes after";
        const auto after_markdown = editor.markdown_utf8();
        const auto after = editor.selection();
        const auto undo_succeeded = editor.undo();
        expect(fatal(bool(undo_succeeded))) << label << " undo";
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << label << " undo source";
        expect(fatal(bool(editor.selection() == selection))) << label << " undo selection";
        expect(fatal(document_indexes_are_exact(editor.document()))) << label << " undo indexes";
        const auto redo_succeeded = editor.redo();
        expect(fatal(bool(redo_succeeded))) << label << " redo";
        expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << label << " redo source";
        expect(fatal(bool(editor.selection() == after))) << label << " redo selection";
        expect(fatal(document_indexes_are_exact(editor.document()))) << label << " redo indexes";
    };

    Editor paragraphs("alpha\n\nbeta\n\ngamma");
    exercise(
        paragraphs,
        TextSelection{
            {paragraphs.document().root.children.front().id, 2, TextAffinity::Downstream},
            {paragraphs.document().root.children.back().id, 3, TextAffinity::Downstream}},
        "paragraphs");

    Editor nested("alpha\n\n> beta\n\n- gamma\n\nomega");
    const BlockNode* first = nullptr;
    const BlockNode* last = nullptr;
    walk_blocks(nested.document().root, [&](const BlockNode& block) {
        if (block.kind != BlockKind::Paragraph) return;
        if (block.inline_content.source == U"alpha") first = &block;
        if (block.inline_content.source == U"omega") last = &block;
    });
    expect(fatal(bool(first != nullptr && last != nullptr)));
    if (first && last) {
        exercise(
            nested,
            TextSelection{
                {first->id, 2, TextAffinity::Downstream},
                {last->id, 2, TextAffinity::Downstream}},
            "nested containers");
    }

    Editor callout("> [!NOTE] title\n> body");
    const auto callout_id = callout.document().root.children.front().id;
    const auto title_id = callout.document().root.children.front().children.front().id;
    const auto body_id = callout.document().root.children.front().children[1].id;
    exercise(
        callout,
        TextSelection{
            {title_id, 2, TextAffinity::Downstream},
            {body_id, 2, TextAffinity::Downstream}},
        "callout title to body");
    const auto* remaining_callout = find_block(callout.document().root, callout_id);
    expect(fatal(bool(remaining_callout != nullptr)));
    expect(fatal(bool(remaining_callout && callout_title_block(*remaining_callout) != nullptr)));

    Editor raw("```\nalpha\n```\n\nomega");
    const auto code_id = raw.document().root.children.front().id;
    const auto paragraph_id = raw.document().root.children.back().id;
    const auto code_start = block_source_editable_range(
        raw.document().root.children.front().block_source).start;
    exercise(
        raw,
        TextSelection{
            {code_id, code_start, TextAffinity::Downstream},
            {paragraph_id, 2, TextAffinity::Downstream}},
        "raw source boundary");
};

"selection_replacement_composes_delete_and_insert_source_edits"_test = [] {
    Editor editor("alpha");
    const auto owner_id = first_text(editor).id;
    const TextSelection before_selection{
        {owner_id, 1, TextAffinity::Downstream},
        {owner_id, 4, TextAffinity::Upstream}};
    editor.set_selection(before_selection);
    reset_core_operation_counters();
    auto transaction = editor.execute_document_insert_text(editor.selection(), U"X");
    expect(fatal(bool(transaction.has_value())));
    if (!transaction) return;
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(counters.inline_reparses == 2u)));
    expect(fatal(bool(transaction->operations.size() == 2u)));
    expect(fatal(bool(editor.markdown_utf8() == "aXa")));
    const auto after_selection = editor.selection();

    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == "alpha")));
    expect(fatal(bool(editor.selection() == before_selection)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == "aXa")));
    expect(fatal(bool(editor.selection() == after_selection)));
};

"typed_block_markers_record_source_and_tree_transformations"_test = [] {
    struct Case {
        std::u32string marker;
        BlockKind expected;
    };
    const std::vector<Case> cases{
        {U"# ", BlockKind::Heading},
        {U"> ", BlockKind::BlockQuote},
        {U"- ", BlockKind::List},
        {U"1. ", BlockKind::List},
    };
    for (const auto& entry : cases) {
        Editor editor;
        for (std::size_t index = 0; index + 1 < entry.marker.size(); ++index) {
            expect(fatal(bool(editor.execute_document_insert_text(
                editor.selection(), std::u32string_view(entry.marker).substr(index, 1)).has_value())));
        }
        const auto before_markdown = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        reset_core_operation_counters();
        auto transaction = editor.execute_document_insert_text(
            editor.selection(), std::u32string_view(entry.marker).substr(entry.marker.size() - 1, 1));
        expect(fatal(bool(transaction.has_value()))) << cps_to_utf8(entry.marker);
        if (!transaction) continue;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << cps_to_utf8(entry.marker);
        expect(fatal(bool(counters.full_document_serializations == 0u))) << cps_to_utf8(entry.marker);
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << cps_to_utf8(entry.marker);
        expect(fatal(bool(editor.document().root.children.front().kind == entry.expected)))
            << cps_to_utf8(entry.marker);
        expect(fatal(bool(editor.selection().active.source_offset == 0u)))
            << cps_to_utf8(entry.marker);
        const auto after_markdown = editor.markdown_utf8();
        const auto after_selection = editor.selection();

        expect(fatal(bool(editor.undo()))) << cps_to_utf8(entry.marker);
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << cps_to_utf8(entry.marker);
        expect(fatal(bool(editor.selection() == before_selection))) << cps_to_utf8(entry.marker);
        expect(fatal(bool(editor.redo()))) << cps_to_utf8(entry.marker);
        expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << cps_to_utf8(entry.marker);
        expect(fatal(bool(editor.selection() == after_selection))) << cps_to_utf8(entry.marker);
    }
};

"typed_list_markers_coalesce_adjacent_lists_with_reversible_moves"_test = [] {
    const std::vector<std::string> cases{
        "- before\n\nafter",
        "before\n\n- after",
    };
    for (const auto& markdown : cases) {
        Editor editor(markdown);
        const auto before_markdown = editor.markdown_utf8();
        const auto paragraph = std::ranges::find_if(
            editor.document().root.children,
            [](const BlockNode& block) {
                return block.kind == BlockKind::Paragraph
                    && !block.inline_content.source.empty();
            });
        expect(fatal(bool(paragraph != editor.document().root.children.end()))) << markdown;
        if (paragraph == editor.document().root.children.end()) continue;
        const auto before_selection = TextSelection::caret(
            {paragraph->id, 0, TextAffinity::Downstream});
        editor.set_selection(before_selection);
        reset_core_operation_counters();
        auto transaction = editor.execute_document_insert_text(editor.selection(), U"- ");
        expect(fatal(bool(transaction.has_value()))) << markdown;
        if (!transaction) continue;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << markdown;
        expect(fatal(bool(editor.document().root.children.size() == 1u))) << markdown;
        if (editor.document().root.children.size() != 1u) continue;
        expect(fatal(bool(editor.document().root.children.front().kind == BlockKind::List))) << markdown;
        expect(fatal(bool(editor.document().root.children.front().children.size() == 2u))) << markdown;
        const auto after_markdown = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        expect(fatal(bool(editor.undo()))) << markdown;
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << markdown;
        expect(fatal(bool(editor.selection() == before_selection))) << markdown;
        expect(fatal(bool(editor.redo()))) << markdown;
        expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << markdown;
        expect(fatal(bool(editor.selection() == after_selection))) << markdown;
    }
};

"typed_regular_markers_coalesce_with_task_lists_by_marker_grammar"_test = [] {
    const struct Case {
        std::string before;
        std::string after;
        BlockKind parent_kind;
        BlockKind first_item_kind;
        BlockKind second_item_kind;
    } cases[]{
        {"- [x] before\n\nafter", "- [x] before\n\n- after",
            BlockKind::TaskList, BlockKind::TaskListItem, BlockKind::ListItem},
        {"before\n\n- [x] after", "- before\n\n- [x] after",
            BlockKind::List, BlockKind::ListItem, BlockKind::TaskListItem},
    };
    for (const auto& test : cases) {
        Editor editor(test.before);
        const auto diagnostic = [&](std::string_view stage) {
            const auto& roots = editor.document().root.children;
            const auto selection = editor.selection();
            std::string message = "case.before=[" + test.before + "] stage="
                + std::string(stage) + " actualmarkdown=[" + editor.markdown_utf8() + "]"
                + " root_count=" + std::to_string(roots.size())
                + " selection.anchor=" + std::to_string(selection.anchor.source_offset)
                + " selection.active=" + std::to_string(selection.active.source_offset);
            if (roots.size() == 1u) {
                const auto& root_list = roots.front();
                message += " root_list_kind="
                    + std::to_string(static_cast<int>(root_list.kind))
                    + " list_item_count=" + std::to_string(root_list.children.size());
                for (std::size_t index = 0; index < root_list.children.size(); ++index) {
                    message += " item" + std::to_string(index) + "_kind="
                        + std::to_string(static_cast<int>(root_list.children[index].kind));
                }
            }
            return message;
        };
        const auto paragraph = std::ranges::find_if(
            editor.document().root.children,
            [](const BlockNode& block) {
                return block.kind == BlockKind::Paragraph
                    && !block.inline_content.source.empty();
            });
        expect(fatal(paragraph != editor.document().root.children.end())) << diagnostic("find-paragraph");
        if (paragraph == editor.document().root.children.end()) continue;
        editor.set_selection(TextSelection::caret(
            {paragraph->id, 0, TextAffinity::Downstream}));
        const auto before_selection = editor.selection();
        reset_core_operation_counters();
        auto transaction = editor.execute_document_insert_text(editor.selection(), U"- ");
        expect(fatal(transaction.has_value())) << diagnostic("insert-marker");
        if (!transaction) continue;
        auto counters = read_core_operation_counters();
        expect(fatal(counters.full_tree_transaction_diffs == 0u)) << diagnostic("transaction-counter");
        expect(fatal(editor.document().root.children.size() == 1u)) << diagnostic("root-count");
        if (editor.document().root.children.size() != 1u) continue;
        const auto& list = editor.document().root.children.front();
        expect(fatal(list.kind == test.parent_kind)) << diagnostic("list-kind");
        expect(fatal(list.children.size() == 2u)) << diagnostic("item-count");
        if (list.children.size() == 2u) {
            expect(fatal(list.children[0].kind == test.first_item_kind)) << diagnostic("item0-kind");
            expect(fatal(list.children[1].kind == test.second_item_kind)) << diagnostic("item1-kind");
        }
        expect(editor.markdown_utf8() == test.after) << diagnostic("after-markdown");
        if (editor.markdown_utf8() != test.after) return;
        const auto after_selection = editor.selection();
        expect(fatal(editor.undo())) << diagnostic("undo");
        expect(fatal(editor.markdown_utf8() == test.before)) << diagnostic("undo-markdown");
        expect(fatal(editor.selection() == before_selection)) << diagnostic("undo-selection");
        expect(fatal(editor.redo())) << diagnostic("redo");
        expect(fatal(editor.markdown_utf8() == test.after)) << diagnostic("redo-markdown");
        expect(fatal(editor.selection() == after_selection)) << diagnostic("redo-selection");
    }
};

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
            && block.inline_content.source == U"current") {
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
            && block.inline_content.source == U"current") current = &block;
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
            && block.inline_content.source == U"task") task_text = &block;
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
            && block.inline_content.source == U"task") paragraph = &block;
    });
    expect(fatal(paragraph != nullptr));
    if (!paragraph) return;
    editor.set_selection(TextSelection::caret(
        {paragraph->id, paragraph->inline_content.source.size(), TextAffinity::Downstream}));
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

}; // suite editor_block_semantic_tests
