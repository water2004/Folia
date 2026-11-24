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

const BlockNode* boundary_text(const Editor& editor, std::u32string_view source) {
    const BlockNode* result = nullptr;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (const auto* text = editable_inline_document(block); text && text->source == source)
            result = &block;
    });
    return result;
}

std::vector<std::u32string> boundary_texts(const Editor& editor) {
    std::vector<std::u32string> result;
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (const auto* text = editable_inline_document(block)) result.push_back(text->source);
    });
    return result;
}

void boundary_locality() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
    expect(counts.full_document_block_index_scans == 0_u);
}

void boundary_history(Editor& editor, std::string before, TextSelection selection) {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(fatal(validate_document(editor.document()).empty()));
    expect(fatal(document_indexes_are_exact(editor.document())));
    Editor reloaded(after);
    expect(bool(boundary_texts(reloaded) == boundary_texts(editor))) << after;
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == before);
    expect(editor.selection() == selection);
    expect(document_indexes_are_exact(editor.document()));
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    expect(document_indexes_are_exact(editor.document()));
}

suite rendered_boundary_operation_tests = [] {

"rendered_boundary_delete_joins_text_across_flow_ancestors"_test = [] {
    struct Case {
        std::string source;
        bool backward;
        std::u32string active;
        std::u32string merged;
        std::vector<std::u32string> remaining;
    };
    const std::vector<Case> cases{
        {"left\n\n> right", false, U"left", U"leftright", {U"leftright"}},
        {"left\n\n> right\n>\n> tail", false, U"left", U"leftright", {U"leftright", U"tail"}},
        {"> left\n\nright", true, U"right", U"leftright", {U"leftright"}},
        {"> left\n>\n> middle\n\nright", true, U"right", U"middleright", {U"left", U"middleright"}},
        {"left\n\n- right\n- tail", false, U"left", U"leftright", {U"leftright", U"tail"}},
        {"left\n\n- right\n  - child\n- tail", false, U"left", U"leftright", {U"leftright", U"child", U"tail"}},
        {"- left\n- middle\n\nright", true, U"right", U"middleright", {U"left", U"middleright"}},
        {"- left\n  - middle\n\nright", true, U"right", U"middleright", {U"left", U"middleright"}},
        {"left\n\n- [x] right\n- [ ] tail", false, U"left", U"leftright", {U"leftright", U"tail"}},
        {"> - left\n\nright", true, U"right", U"leftright", {U"leftright"}},
        {"> left\n\n> > right", false, U"left", U"leftright", {U"leftright"}},
        {"left\n\n[^n]: right", false, U"left", U"leftright", {U"leftright"}},
        {"[^n]: left\n\nright", true, U"right", U"leftright", {U"leftright"}},
        {"left\n\n> [!NOTE]\n> right", false, U"left", U"leftright", {U"leftright"}},
        {"> [!NOTE]\n> left\n\nright", true, U"right", U"leftright", {U"leftright"}},
    };
    for (const auto& entry : cases) {
        Editor editor(entry.source);
        const auto* active = boundary_text(editor, entry.active);
        expect(fatal(active != nullptr)) << entry.source;
        if (!active) continue;
        const auto before = editor.markdown_utf8();
        const auto selection = TextSelection::caret({active->id,
            entry.backward ? 0u : active->inline_content().source.size(), TextAffinity::Downstream});
        editor.set_selection(selection);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = entry.backward
            ? CommandKind::DeleteBackward : CommandKind::DeleteForward}))) << entry.source;
        boundary_locality();
        expect(bool(boundary_texts(editor) == entry.remaining)) << entry.source << editor.markdown_utf8();
        const auto* merged = boundary_text(editor, entry.merged);
        expect(fatal(merged != nullptr)) << entry.source;
        if (merged) {
            expect(editor.selection().active.container_id == merged->id);
            expect(editor.selection().active.source_offset == entry.merged.size() - 5u);
        }
        boundary_history(editor, before, selection);
    }
};

"rendered_boundary_delete_exposes_code_and_table_neighbors_without_mutation"_test = [] {
    const std::vector<std::string> sources{
        "before\n\n```cpp\nvalue\n```\n\nafter",
        "before\n\n    value\n\nafter",
        "before\n\n| a | b |\n| --- | --- |\n| c | d |\n\nafter",
    };
    for (const auto& source : sources) {
        Editor original(source);
        const auto order = original.document().cached_editable_order;
        for (std::size_t index = 0; index + 1 < order.size(); ++index)
        for (const auto backward : {false, true}) {
            Editor editor(source);
            const auto& current_order = editor.document().cached_editable_order;
            const auto active_index = backward ? index + 1 : index;
            const auto target_index = backward ? index : index + 1;
            const auto* active = find_document_block(editor.document(), current_order[active_index]);
            const auto* target = find_document_block(editor.document(), current_order[target_index]);
            expect(fatal(active != nullptr && target != nullptr));
            if (!active || !target) continue;
            const auto active_range = *editable_block_source_range(*active);
            const auto target_range = *editable_block_source_range(*target);
            const auto before = editor.markdown_utf8();
            const auto revision = editor.revision();
            const auto state = editor.content_state();
            const auto selection = TextSelection::caret({active->id,
                backward ? active_range.start : active_range.end, TextAffinity::Downstream});
            const auto expected = TextSelection::caret({target->id,
                backward ? target_range.end : target_range.start,
                backward ? TextAffinity::Upstream : TextAffinity::Downstream});
            editor.set_selection(selection);
            reset_core_operation_counters();
            expect(fatal(editor.execute_command({.kind = backward
                ? CommandKind::DeleteBackward : CommandKind::DeleteForward}))) << source << index << backward;
            boundary_locality();
            expect(editor.selection() == expected) << source << index << backward;
            expect(editor.markdown_utf8() == before);
            expect(editor.revision() == revision);
            expect(editor.content_state() == state);
            expect(!editor.has_undo());
            expect(document_indexes_are_exact(editor.document()));
        }
    }
};

};

} // namespace
