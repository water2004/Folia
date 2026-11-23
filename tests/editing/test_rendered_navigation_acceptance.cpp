#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

void expect_rendered_navigation(Editor& editor, TextSelection from, DocumentMove movement,
    bool extend, NodeId owner, std::size_t offset) {
    editor.set_selection(from);
    const auto before = editor.markdown_utf8();
    const auto revision = editor.revision();
    reset_core_operation_counters();
    expect(fatal(editor.execute_document_move(movement, extend)));
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_document_block_index_scans == 0_u);
    expect(editor.selection().active.container_id == owner);
    expect(editor.selection().active.source_offset == offset);
    if (extend) expect(editor.selection().anchor == from.anchor);
    else expect(editor.selection().is_caret());
    expect(editor.markdown_utf8() == before);
    expect(editor.revision() == revision);
    expect(!editor.has_undo());
}

suite rendered_navigation_acceptance_tests = [] {

"rendered_navigation_vertical_moves_between_editable_flow_owners"_test = [] {
    Editor editor("alpha\n\n> beta\n>\n> gamma\n\n```txt\ncode\nlast\n```\n\nomega");
    const auto order = editor.document().cached_editable_order;
    expect(fatal(order.size() == 5_u));
    if (order.size() != 5) return;
    const auto* code = find_document_block(editor.document(), order[3]);
    const auto mapping = code->block_source.tree().content_to_source;
    struct Step { std::size_t owner; std::size_t offset; DocumentMove move; std::size_t target; std::size_t destination; };
    const std::vector<Step> steps{
        {0, 2, DocumentMove::Down, 1, 2},
        {1, 1, DocumentMove::Up, 0, 1},
        {1, 3, DocumentMove::Down, 2, 3},
        {2, 2, DocumentMove::Down, 3, mapping[2]},
        {3, mapping[7], DocumentMove::Down, 4, 2},
        {4, 3, DocumentMove::Up, 3, mapping[8]},
        {3, mapping[1], DocumentMove::Up, 2, 1},
        {0, 2, DocumentMove::Up, 0, 0},
        {4, 2, DocumentMove::Down, 4, 5},
    };
    for (const auto& step : steps)
    for (const auto extend : {false, true}) {
        expect_rendered_navigation(editor, TextSelection::caret({order[step.owner], step.offset}),
            step.move, extend, order[step.target], step.destination);
    }
};

"rendered_navigation_understands_lf_crlf_and_cr_in_inline_and_raw_lines"_test = [] {
    for (const auto ending : {std::string{"\n"}, std::string{"\r\n"}, std::string{"\r"}})
    for (const auto raw : {false, true}) {
        const auto body = std::string{"abcdef"} + ending + "xy" + ending + "12345";
        Editor editor(raw ? "```txt" + ending + body + ending + "```" : body);
        const auto* owner = find_document_block(editor.document(), editor.document().cached_editable_order.front());
        expect(fatal(owner != nullptr));
        if (!owner) continue;
        const auto id = owner->id;
        const auto second = std::size_t{6} + ending.size();
        const auto third = second + 2 + ending.size();
        auto at = [&](std::size_t inline_offset) {
            if (!raw) return inline_offset;
            // Raw content preserves the line ending spelling as well. Map a
            // visible content offset to its authoritative fenced source.
            return owner->block_source.tree().content_to_source[inline_offset];
        };
        for (const auto extend : {false, true}) {
            expect_rendered_navigation(editor, TextSelection::caret({id, at(second + 1)}),
                DocumentMove::LineStart, extend, id, at(second));
            expect_rendered_navigation(editor, TextSelection::caret({id, at(second + 1)}),
                DocumentMove::LineEnd, extend, id, at(second + 2));
            expect_rendered_navigation(editor, TextSelection::caret({id, at(second + 1)}),
                DocumentMove::Up, extend, id, at(1));
            expect_rendered_navigation(editor, TextSelection::caret({id, at(second + 1)}),
                DocumentMove::Down, extend, id, at(third + 1));
            expect_rendered_navigation(editor, TextSelection::caret({id, at(4)}),
                DocumentMove::Down, extend, id, at(second + 2));
        }
    }
};

"rendered_navigation_line_and_document_jumps_start_from_the_active_endpoint"_test = [] {
    Editor editor("first\n\nalpha\nbeta\n\nlast");
    const auto order = editor.document().cached_editable_order;
    expect(fatal(order.size() == 3_u));
    if (order.size() != 3) return;
    for (const auto reversed : {false, true}) {
        const TextPosition first{order[1], 1, TextAffinity::Downstream};
        const TextPosition second{order[1], 8, TextAffinity::Upstream};
        const auto selection = reversed ? TextSelection{second, first} : TextSelection{first, second};
        expect_rendered_navigation(editor, selection, DocumentMove::LineStart, false, order[1], reversed ? 0u : 6u);
        expect_rendered_navigation(editor, selection, DocumentMove::LineEnd, false, order[1], reversed ? 5u : 10u);
        expect_rendered_navigation(editor, selection, DocumentMove::DocumentStart, false, order.front(), 0);
        expect_rendered_navigation(editor, selection, DocumentMove::DocumentEnd, false, order.back(), 4);
        expect_rendered_navigation(editor, selection, DocumentMove::Left, false, order[1], 1);
        expect_rendered_navigation(editor, selection, DocumentMove::Right, false, order[1], 8);
    }
    // Reaching the requested endpoint may leave the active offset unchanged,
    // but collapsing an existing range is still a successful selection edit.
    expect_rendered_navigation(editor,
        {{order[0], 2}, {order.back(), 4}}, DocumentMove::DocumentEnd, false, order.back(), 4);
    expect_rendered_navigation(editor,
        {{order.back(), 2}, {order.front(), 0}}, DocumentMove::DocumentStart, false, order.front(), 0);
};

"rendered_navigation_vertical_columns_never_split_graphemes"_test = [] {
    Editor editor("abcd\n\ne\u0301\U0001f642z");
    const auto order = editor.document().cached_editable_order;
    expect(fatal(order.size() == 2_u));
    if (order.size() != 2) return;
    expect_rendered_navigation(editor, TextSelection::caret({order.front(), 1}),
        DocumentMove::Down, false, order.back(), 0);
    expect_rendered_navigation(editor, TextSelection::caret({order.front(), 3}),
        DocumentMove::Down, true, order.back(), 3);
};

};

} // namespace
