#include <algorithm>
#include <cstddef>
#include <iterator>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.container_source;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.core.editor;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

using PrefixSnapshot = std::vector<std::pair<NodeId, ContainerSource>>;

const BlockNode* first_paragraph(const EditorDocument& document) {
    const BlockNode* result = nullptr;
    walk_blocks(document.root, [&](const BlockNode& block) {
        if (!result && block.kind == BlockKind::Paragraph) result = &block;
    });
    return result;
}

const BlockNode* paragraph_with(const EditorDocument& document, std::u32string_view text) {
    const BlockNode* result = nullptr;
    walk_blocks(document.root, [&](const BlockNode& block) {
        if (!result && block.kind == BlockKind::Paragraph
            && block.inline_content.source == text) result = &block;
    });
    return result;
}

PrefixSnapshot prefix_snapshot(const EditorDocument& document) {
    PrefixSnapshot result;
    walk_blocks(document.root, [&](const BlockNode& block) {
        if (block.has_container_source()) {
            result.emplace_back(block.id, block.container_source());
        }
    });
    return result;
}

bool inline_sources_are_lossless(const BlockNode& block) {
    if ((block.kind == BlockKind::Paragraph || block.kind == BlockKind::Heading
         || block.kind == BlockKind::CalloutTitle || block.kind == BlockKind::TableCell)
        && (!tokens_partition_source(block.inline_content.tree, block.inline_content.source.size())
            || !roots_partition_source(block.inline_content.tree, block.inline_content.source.size())
            || flatten_tokens(block.inline_content.tree, block.inline_content.source)
                != block.inline_content.source
            || serialize_lossless(block.inline_content.tree, block.inline_content.source)
                != block.inline_content.source)) {
        return false;
    }
    for (const auto& child : block.children) {
        if (!inline_sources_are_lossless(child)) return false;
    }
    return true;
}

void append_signature(std::string& out, const BlockNode& block) {
    out += std::to_string(static_cast<int>(block.kind));
    out.push_back('(');
    if (block.kind == BlockKind::Paragraph || block.kind == BlockKind::Heading
        || block.kind == BlockKind::CalloutTitle || block.kind == BlockKind::TableCell) {
        out += cps_to_utf8(block.inline_content.source);
    } else if (block.kind == BlockKind::CodeBlock || block.kind == BlockKind::MathBlock) {
        out += cps_to_utf8(block.block_source.source());
    } else if (block.kind == BlockKind::List) {
        out += block.list_special().ordered ? "ordered" : "bullet";
        out += cps_to_utf8(std::u32string(1, block.list_special().delimiter));
        out += std::to_string(block.list_special().start);
    } else if (block.kind == BlockKind::ListItem || block.kind == BlockKind::TaskListItem) {
        out += cps_to_utf8(block.item_special().marker);
    }
    for (const auto& child : block.children) append_signature(out, child);
    out.push_back(')');
}

std::string signature(const EditorDocument& document) {
    std::string result;
    append_signature(result, document.root);
    return result;
}

bool prefix_anchors_are_in_range(const EditorDocument& document) {
    bool valid = true;
    walk_blocks(document.root, [&](const BlockNode& container) {
        if (!container.has_container_source()) return;
        for (const auto& line : container.container_source().lines) {
            if (line.anchor.part != ContainerAnchorPart::Source) continue;
            const auto* owner = find_block(document.root, line.anchor.owner_id);
            if (!owner) {
                valid = false;
                continue;
            }
            const auto* inline_document = editable_inline_document(*owner);
            const auto* raw_source = editable_raw_block_source(*owner);
            const auto size = inline_document ? inline_document->source.size()
                : raw_source ? raw_source->size() : std::size_t{0};
            if ((!inline_document && !raw_source) || line.anchor.offset > size) valid = false;
        }
    });
    return valid;
}

void expect_local_edit_counters(CoreOperationCounters counters, std::string_view label) {
    expect(fatal(counters.full_document_parses == 0u)) << label << " full parse";
    expect(fatal(counters.full_document_serializations == 0u)) << label << " full serialization";
    expect(fatal(counters.full_tree_transaction_diffs == 0u)) << label << " full tree diff";
}

} // namespace

suite editor_container_source_history_tests = [] {

"character_edit_keeps_untouched_physical_prefixes_and_replays_metadata"_test = [] {
    const std::string source = ">foo\nbar\n>  baz\n";
    Editor editor(source);
    const auto* paragraph = paragraph_with(editor.document(), U"foo\nbar\n baz");
    expect(fatal(paragraph != nullptr));
    if (!paragraph) return;
    const auto owner = paragraph->id;
    const auto original_selection = TextSelection::caret(
        {owner, 1, TextAffinity::Downstream});
    editor.set_selection(original_selection);
    const auto original_prefixes = prefix_snapshot(editor.document());

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_insert_text(editor.selection(), U"X").has_value()));
    expect_local_edit_counters(read_core_operation_counters(), "character insertion");
    const auto edited_source = editor.markdown_utf8();
    const auto edited_selection = editor.selection();
    const auto edited_prefixes = prefix_snapshot(editor.document());
    expect(fatal(edited_source == ">fXoo\nbar\n>  baz\n"));
    expect(fatal(edited_selection.active.container_id == owner));
    expect(fatal(edited_selection.active.source_offset == 2u));
    expect(fatal(inline_sources_are_lossless(editor.document().root)));
    expect(fatal(prefix_anchors_are_in_range(editor.document())));

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_local_edit_counters(read_core_operation_counters(), "undo insertion");
    expect(fatal(editor.markdown_utf8() == source));
    expect(fatal(editor.selection() == original_selection));
    expect(fatal(prefix_snapshot(editor.document()) == original_prefixes));

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_local_edit_counters(read_core_operation_counters(), "redo insertion");
    expect(fatal(editor.markdown_utf8() == edited_source));
    expect(fatal(editor.selection() == edited_selection));
    expect(fatal(prefix_snapshot(editor.document()) == edited_prefixes));
};

"enter_split_and_cross_block_backspace_restore_nested_quote_prefixes"_test = [] {
    const std::string source = "  >foo\n>  bar\n>\n>baz\n";
    Editor editor(source);
    const auto* paragraph = first_paragraph(editor.document());
    expect(fatal(paragraph != nullptr));
    if (!paragraph) return;
    const auto paragraph_id = paragraph->id;
    const auto before_split_selection = TextSelection::caret(
        {paragraph_id, 2, TextAffinity::Downstream});
    editor.set_selection(before_split_selection);
    const auto before_prefixes = prefix_snapshot(editor.document());

    reset_core_operation_counters();
    const auto split = editor.execute_document_enter(editor.selection());
    expect(fatal(split.has_value()));
    expect_local_edit_counters(read_core_operation_counters(), "Enter split");
    if (!split) return;
    const auto split_source = editor.markdown_utf8();
    const auto after_split_selection = editor.selection();
    const auto split_prefixes = prefix_snapshot(editor.document());
    expect(fatal(after_split_selection.active.source_offset == 0u));
    expect(fatal(after_split_selection.active.container_id != paragraph_id));
    expect(fatal(inline_sources_are_lossless(editor.document().root)));
    expect(fatal(prefix_anchors_are_in_range(editor.document())));

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_local_edit_counters(read_core_operation_counters(), "split undo");
    expect(fatal(editor.markdown_utf8() == source));
    expect(fatal(editor.selection() == before_split_selection));
    expect(fatal(prefix_snapshot(editor.document()) == before_prefixes));

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_local_edit_counters(read_core_operation_counters(), "split redo");
    expect(fatal(editor.markdown_utf8() == split_source));
    expect(fatal(editor.selection() == after_split_selection));
    expect(fatal(prefix_snapshot(editor.document()) == split_prefixes));

    // Backspace at the new paragraph's start joins the two block-local source
    // owners. The prefix payload must move with that structural transaction.
    const BlockNode* final_owner = find_block(
        editor.document().root, after_split_selection.active.container_id);
    expect(fatal(final_owner != nullptr));
    if (!final_owner) return;
    const auto join_selection = TextSelection::caret(
        {final_owner->id, 0, TextAffinity::Downstream});
    editor.set_selection(join_selection);
    const auto pre_join_source = editor.markdown_utf8();
    const auto pre_join_prefixes = prefix_snapshot(editor.document());

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_delete_backward(editor.selection()).has_value()));
    expect_local_edit_counters(read_core_operation_counters(), "cross-block Backspace");
    const auto joined_source = editor.markdown_utf8();
    const auto joined_selection = editor.selection();
    const auto joined_prefixes = prefix_snapshot(editor.document());
    expect(fatal(inline_sources_are_lossless(editor.document().root)));

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_local_edit_counters(read_core_operation_counters(), "join undo");
    expect(fatal(editor.markdown_utf8() == pre_join_source));
    expect(fatal(editor.selection() == join_selection));
    expect(fatal(prefix_snapshot(editor.document()) == pre_join_prefixes));

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_local_edit_counters(read_core_operation_counters(), "join redo");
    expect(fatal(editor.markdown_utf8() == joined_source));
    expect(fatal(editor.selection() == joined_selection));
    expect(fatal(prefix_snapshot(editor.document()) == joined_prefixes));
};

"newline_insert_delete_and_history_keep_neighboring_quote_lines"_test = [] {
    const std::string source = ">foo\n>bar\n";
    Editor editor(source);
    const auto* paragraph = first_paragraph(editor.document());
    expect(fatal(paragraph != nullptr));
    if (!paragraph) return;
    editor.set_selection(TextSelection::caret(
        {paragraph->id, 1, TextAffinity::Downstream}));

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_insert_text(editor.selection(), U"\n").has_value()));
    expect_local_edit_counters(read_core_operation_counters(), "newline insertion");
    const auto inserted = editor.markdown_utf8();
    const auto inserted_selection = editor.selection();
    expect(fatal(inserted_selection.active.source_offset == 2u));
    expect(fatal(inline_sources_are_lossless(editor.document().root)));
    expect(fatal(prefix_anchors_are_in_range(editor.document())));

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_delete_backward(editor.selection()).has_value()));
    expect_local_edit_counters(read_core_operation_counters(), "newline deletion");
    const auto deleted_selection = editor.selection();
    expect(fatal(editor.markdown_utf8() == source));
    expect(fatal(deleted_selection.active.source_offset == 1u));
    const auto deleted_prefixes = prefix_snapshot(editor.document());

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_local_edit_counters(read_core_operation_counters(), "newline deletion undo");
    expect(fatal(editor.markdown_utf8() == inserted));
    expect(fatal(editor.selection() == inserted_selection));

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_local_edit_counters(read_core_operation_counters(), "newline insertion undo");
    expect(fatal(editor.markdown_utf8() == source));

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_local_edit_counters(read_core_operation_counters(), "newline insertion redo");
    expect(fatal(editor.markdown_utf8() == inserted));
    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_local_edit_counters(read_core_operation_counters(), "newline deletion redo");
    expect(fatal(editor.markdown_utf8() == source));
    expect(fatal(editor.selection() == deleted_selection));
    expect(fatal(prefix_snapshot(editor.document()) == deleted_prefixes));
};

"multiline_text_edits_keep_prefix_line_endings_and_history_exact"_test = [] {
    const std::vector<std::string> sources{
        "> first\r\n> second\r\n",
        "> first\r> second\r",
        "- first\r\n  second\r\n",
    };
    for (const auto& source : sources) {
        Editor editor(source);
        const auto* paragraph = first_paragraph(editor.document());
        expect(fatal(paragraph != nullptr)) << source;
        if (!paragraph) continue;
        const auto initial_source = editor.markdown_utf8();
        const auto initial_prefixes = prefix_snapshot(editor.document());
        const auto selection = TextSelection::caret(
            {paragraph->id, 1, TextAffinity::Downstream});
        editor.set_selection(selection);

        reset_core_operation_counters();
        expect(fatal(editor.execute_document_insert_text(editor.selection(), U"x\ny").has_value())) << source;
        expect_local_edit_counters(read_core_operation_counters(), "multiline insertion");
        const auto changed_source = editor.markdown_utf8();
        const auto changed_selection = editor.selection();
        const auto changed_prefixes = prefix_snapshot(editor.document());
        expect(fatal(inline_sources_are_lossless(editor.document().root))) << source;
        expect(fatal(prefix_anchors_are_in_range(editor.document()))) << source;

        reset_core_operation_counters();
        expect(fatal(editor.undo())) << source;
        expect_local_edit_counters(read_core_operation_counters(), "multiline undo");
        expect(fatal(editor.markdown_utf8() == initial_source)) << source;
        expect(fatal(prefix_snapshot(editor.document()) == initial_prefixes)) << source;

        reset_core_operation_counters();
        expect(fatal(editor.redo())) << source;
        expect_local_edit_counters(read_core_operation_counters(), "multiline redo");
        expect(fatal(editor.markdown_utf8() == changed_source)) << source;
        expect(fatal(editor.selection() == changed_selection)) << source;
        expect(fatal(prefix_snapshot(editor.document()) == changed_prefixes)) << source;

        // Save/reload checks run after the operation counters have been read:
        // serialization itself is intentionally outside the normal-edit gate.
        Editor reloaded(changed_source);
        expect(fatal(reloaded.markdown_utf8() == changed_source)) << source;
        expect(fatal(inline_sources_are_lossless(reloaded.document().root))) << source;
        expect(fatal(signature(reloaded.document()) == signature(editor.document()))) << source;
    }
};

"moving_a_list_item_between_nesting_levels_is_reversible_and_lossless"_test = [] {
    const std::string source = "- parent\n  - child\n    - grandchild\n- tail\n";
    Editor editor(source);
    const auto* paragraph = paragraph_with(editor.document(), U"grandchild");
    expect(fatal(paragraph != nullptr));
    if (!paragraph) return;
    const auto selection = TextSelection::caret(
        {paragraph->id, 3, TextAffinity::Downstream});
    editor.set_selection(selection);
    const auto initial_prefixes = prefix_snapshot(editor.document());

    reset_core_operation_counters();
    expect(fatal(editor.execute_document_outdent_list_item(editor.selection()).has_value()));
    expect_local_edit_counters(read_core_operation_counters(), "outdent list item");
    const auto moved_source = editor.markdown_utf8();
    const auto moved_selection = editor.selection();
    const auto moved_prefixes = prefix_snapshot(editor.document());
    expect(fatal(inline_sources_are_lossless(editor.document().root)));
    expect(fatal(prefix_anchors_are_in_range(editor.document())));
    Editor reloaded(moved_source);
    expect(fatal(reloaded.markdown_utf8() == moved_source));
    expect(fatal(inline_sources_are_lossless(reloaded.document().root)));
    expect(fatal(signature(reloaded.document()) == signature(editor.document())));

    reset_core_operation_counters();
    expect(fatal(editor.undo()));
    expect_local_edit_counters(read_core_operation_counters(), "outdent undo");
    expect(fatal(editor.markdown_utf8() == source));
    expect(fatal(prefix_snapshot(editor.document()) == initial_prefixes));

    reset_core_operation_counters();
    expect(fatal(editor.redo()));
    expect_local_edit_counters(read_core_operation_counters(), "outdent redo");
    expect(fatal(editor.markdown_utf8() == moved_source));
    expect(fatal(editor.selection() == moved_selection));
    expect(fatal(prefix_snapshot(editor.document()) == moved_prefixes));
};

"bounded_random_source_edits_replay_exactly_in_mixed_container_prefixes"_test = [] {
    struct Case {
        std::string markdown;
        std::string exact_sibling_line;
        std::uint32_t seed;
    };
    const std::vector<Case> cases{
        {"> alpha\nlazy\n>  sibling\n", ">  sibling\n", 0x93u},
        {"- alpha\n  beta\n  * sibling\n", "  * sibling\n", 0x238u},
    };

    struct State {
        std::string markdown;
        TextSelection selection;
        PrefixSnapshot prefixes;
    };
    for (const auto& test : cases) {
        Editor editor(test.markdown);
        const auto* paragraph = first_paragraph(editor.document());
        expect(fatal(paragraph != nullptr)) << test.markdown;
        if (!paragraph) continue;
        const auto owner_id = paragraph->id;
        // The first editable leaf can have a container-start caret rather
        // than a caret in this paragraph. Seed a valid paragraph-local caret.
        const auto initial_selection = TextSelection::caret(
            {owner_id, 0, TextAffinity::Downstream});
        editor.set_selection(initial_selection);
        std::vector<State> states{{editor.markdown_utf8(), initial_selection,
            prefix_snapshot(editor.document())}};
        std::vector<TextSelection> selections_before;
        std::mt19937 random(test.seed);
        constexpr std::size_t step_count = 16;

        for (std::size_t step = 0; step < step_count; ++step) {
            const auto current = editor.editable_source(owner_id);
            expect(fatal(current.has_value())) << test.markdown << " step " << step;
            if (!current) break;
            // Keep mutations near the beginning of the owner so line starts
            // for its continuation and the nested/sibling container remain
            // meaningful anchors after each source edit.
            const auto editable_limit = (std::min)(current->size(), std::size_t{4});
            const auto offset = editable_limit == 0 ? std::size_t{0}
                : static_cast<std::size_t>(random() % (editable_limit + 1));
            editor.set_selection(TextSelection::caret(
                {owner_id, offset, TextAffinity::Downstream}));
            selections_before.push_back(editor.selection());
            const auto operation = static_cast<unsigned>(random() % 4u);

            reset_core_operation_counters();
            bool changed = false;
            if (operation == 0u || (operation == 2u && offset == 0u)) {
                const std::u32string glyphs[]{U"x", U"y", U"!", U"?"};
                const auto& text = glyphs[random() % std::size(glyphs)];
                changed = editor.execute_document_insert_text(editor.selection(), text).has_value();
            } else if (operation == 1u) {
                changed = editor.execute_document_insert_text(editor.selection(), U"\n").has_value();
            } else if (operation == 2u) {
                changed = editor.execute_document_delete_backward(editor.selection()).has_value();
            } else if (operation == 3u && offset < current->size()) {
                changed = editor.execute_document_delete_forward(editor.selection()).has_value();
            } else {
                changed = editor.execute_document_insert_text(editor.selection(), U"z").has_value();
            }
            expect(fatal(changed)) << test.markdown << " step " << step;
            expect_local_edit_counters(read_core_operation_counters(), "random edit");
            expect(fatal(inline_sources_are_lossless(editor.document().root)))
                << test.markdown << " step " << step;
            expect(fatal(prefix_anchors_are_in_range(editor.document())))
                << test.markdown << " step " << step;

            const auto saved = editor.markdown_utf8();
            expect(fatal(saved.ends_with(test.exact_sibling_line)))
                << test.markdown << " step " << step << " changed sibling line spelling";
            states.push_back({saved, editor.selection(), prefix_snapshot(editor.document())});
        }
        if (selections_before.empty()) continue;

        // Each history replay is checked before serializing, so the counters
        // measure only the undo/redo operation itself.
        for (std::size_t index = states.size() - 1; index > 0; --index) {
            reset_core_operation_counters();
            expect(fatal(editor.undo())) << test.markdown << " undo " << index;
            expect_local_edit_counters(read_core_operation_counters(), "random undo");
            expect(fatal(editor.selection() == selections_before[index - 1]))
                << test.markdown << " undo selection " << index;
            expect(fatal(prefix_snapshot(editor.document()) == states[index - 1].prefixes))
                << test.markdown << " undo prefix records " << index;
            expect(fatal(inline_sources_are_lossless(editor.document().root)))
                << test.markdown << " undo CST " << index;
            expect(fatal(prefix_anchors_are_in_range(editor.document())))
                << test.markdown << " undo anchor range " << index;
            expect(fatal(editor.markdown_utf8() == states[index - 1].markdown))
                << test.markdown << " undo bytes " << index;
        }
        expect(fatal(editor.selection() == selections_before.front()))
            << test.markdown << " first transaction undo caret";
        for (std::size_t index = 1; index < states.size(); ++index) {
            reset_core_operation_counters();
            expect(fatal(editor.redo())) << test.markdown << " redo " << index;
            expect_local_edit_counters(read_core_operation_counters(), "random redo");
            expect(fatal(editor.selection() == states[index].selection))
                << test.markdown << " redo selection " << index;
            expect(fatal(prefix_snapshot(editor.document()) == states[index].prefixes))
                << test.markdown << " redo prefix records " << index;
            expect(fatal(inline_sources_are_lossless(editor.document().root)))
                << test.markdown << " redo CST " << index;
            expect(fatal(prefix_anchors_are_in_range(editor.document())))
                << test.markdown << " redo anchor range " << index;
            expect(fatal(editor.markdown_utf8() == states[index].markdown))
                << test.markdown << " redo bytes " << index;
        }
    }
};

}; // suite editor_container_source_history_tests
