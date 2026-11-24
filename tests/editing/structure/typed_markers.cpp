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

suite editing_structure_typed_markers_tests = [] {

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

};
