#include "support/folia_test.hpp"

import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.ids;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.instrumentation;
import folia.core.theme;
import folia.core.editor;
import folia.core.command;
import folia.markdown.text_edit;
import folia.core.render_builder;
import folia.core.render_model;
import folia.core.render_fragments;
import folia.core.text_measurer;
import folia.core.block_layout;
import folia.core.selection_geometry;
import folia.core.hit_test;
import folia.core.types;
import folia.markdown.selection;

using namespace folia;
using namespace boost::ut;

namespace {
std::u32string flow_spelling(RenderBlock const& block) {
    std::u32string result;
    for (auto const& item : block.inline_items)
        result += item.special().display_text.empty() ? item.text : item.special().display_text;
    return result;
}

std::string list_source(std::size_t count, std::string const& prefix = "- ") {
    std::string result;
    for (std::size_t i = 0; i < count; ++i) result += prefix + "item **bold** and text\n";
    return result;
}

RenderModelUpdate fragment_update(EditorDocumentChange const& change) {
    RenderModelUpdate update;
    update.structural = change.structural;
    update.structural_locality_known = change.structural_locality_known;
    update.structural_anchors = change.structural_anchors;
    update.tree_deltas = change.tree_deltas;
    for (auto const& operation : change.text_operations)
        update.changed_owners.push_back((change.forward ? operation.forward : operation.inverse).container_id);
    return update;
}

void expect_fragment_navigation(Editor const& editor, RenderModel const& model) {
    expect(model.editable_order == editor.document().cached_editable_order);
    expect(model.editable_index == editor.document().cached_editable_index);
    std::size_t next_owner = 0;
    for (auto const& interval : model.owner_intervals) {
        expect(interval.begin == next_owner);
        expect(interval.fragment < model.fragments.size());
        next_owner = interval.end;
        for (auto ordinal = interval.begin; ordinal < interval.end; ++ordinal)
            expect(model.fragment_for_owner(model.editable_order[ordinal]) == interval.fragment);
    }
    expect(next_owner == model.editable_order.size());
    for (auto const& fragment : model.fragments)
        expect(fragment.leaf_count <= render_fragment_leaf_limit);
    Builder reference(default_theme_profile());
    configure_render_dependencies(reference, editor.symbols());
    std::u32string actual;
    for (std::size_t index = 0; index < model.blocks.size(); ++index) {
        if (index) actual += U'\n';
        actual += flow_spelling(model.blocks[index]);
    }
    expect(actual == flow_spelling(reference.build_block(editor.document().root.children.front())));
}
}

suite render_fragment_tests = [] {
    "fragment owner intervals cover mixed blocks and skip unowned fragments"_test = [] {
        auto parsed = parse_text(1, "# title\n\n---\n\n> quote\n\n"
            "| A | B |\n| --- | --- |\n| C | |\n\n"
            "```cpp\nint x;\n```\n\n$$\nx\n$$\n");
        auto model = build_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
        std::size_t expected_count = 0;
        for (std::size_t index = 0; index < model.fragments.size(); ++index) {
            auto const& fragment = model.fragments[index];
            auto const* root = find_document_block(parsed.document, fragment.source_root);
            if (!expect(fatal(root != nullptr))) return;
            visit_render_fragment(*root, fragment_range(fragment), [&](auto const& node, auto) {
                if (is_editable_block_owner(node.kind)) {
                    expect(model.fragment_for_owner(node.id) == index);
                    ++expected_count;
                } else {
                    expect(!model.fragment_for_owner(node.id));
                }
            });
        }
        expect(expected_count == model.editable_order.size());
        // Physical blocks without an editable owner do not consume a rank.
        // A later fragment must still resolve to its actual physical slot.
        auto fragments = model.fragments;
        fragments.insert(fragments.begin(), RenderFragment{});
        auto intervals = index_render_fragment_owners(parsed.document, fragments);
        expect(intervals.size() == model.owner_intervals.size());
        expect(render_fragment_at(intervals, 0) == 1u);
    };

    "flow_fragments_preserve_nested_flow_spelling_and_owner_coordinates"_test = [] {
        for (auto const& source : std::vector<std::string>{
            list_source(193),
            list_source(193, "98. "),
            list_source(193, "- [x] "),
            list_source(193, "- item\n\n- "),
            std::string{"- parent\n"} + list_source(193, "  - ") + "- tail\n",
            std::string{"> [!NOTE]\n"} + list_source(193, "> - "),
            list_source(193, "> > - "),
        }) {
            auto parsed = parse_text(1, source);
            expect(fatal(parsed.document.root.children.size() == 1u));
            Builder builder(default_theme_profile());
            configure_render_dependencies(builder, parsed.symbols);
            auto whole = builder.build_block(parsed.document.root.children.front());
            auto model = build_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
            expect(fatal(model.blocks.size() > 1u));
            std::u32string joined;
            std::unordered_set<std::uint64_t> ids;
            for (std::size_t i = 0; i < model.blocks.size(); ++i) {
                auto const& fragment = model.fragments[i];
                auto const& block = model.blocks[i];
                if (i) joined += U'\n'; // a physical boundary replaces one block-flow break
                joined += flow_spelling(block);
                expect(ids.insert(block.id.v).second);
                expect(fragment.source_root == parsed.document.root.children.front().id);
                expect(block.fragment_continues == (i + 1 < model.blocks.size()));
                if (i) expect(block.block_style.margin_top == 0.0f);
                if (i + 1 < model.blocks.size()) expect(block.block_style.margin_bottom == 0.0f);
                std::size_t leaves = 0;
                visit_render_fragment(parsed.document.root.children.front(), fragment_range(fragment),
                    [&](auto const& node, auto) {
                        if (!is_fragment_flow_container(node.kind) || node.children.empty()) ++leaves;
                        if (is_editable_block_owner(node.kind))
                            expect(model.fragment_for_owner(node.id) == i);
                    });
                expect(leaves <= render_fragment_leaf_limit);
            }
            expect(joined == flow_spelling(whole));
            expect(model.owner_intervals.back().end == parsed.document.cached_editable_order.size());
        }
    };

    "fragment_boundaries_preserve_caret_positions_and_hit_testing"_test = [] {
        auto parsed = parse_text(1, list_source(130));
        Builder builder(default_theme_profile());
        auto whole = builder.build_block(parsed.document.root.children.front());
        auto model = build_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
        StubMeasurer measurer{8.0f};
        auto expected = layout_blocks(std::vector<RenderBlock>{whole}, 700.0f, 1.0f, measurer,
            std::nullopt, LogicalPoint{0, 0}, parsed.outline);
        auto fragmented = layout_blocks(model.blocks, 700.0f, 1.0f, measurer,
            std::nullopt, LogicalPoint{0, 0}, parsed.outline);
        expect(expected.total_height == fragmented.total_height);
        for (auto ordinal : {0u, 63u, 64u, 65u, 127u, 128u, 129u}) {
            auto owner = parsed.document.cached_editable_order[ordinal];
            TextPosition position{owner, 2, TextAffinity::Downstream};
            auto before = compute_caret_geometry(expected, position);
            auto after = compute_caret_geometry(fragmented, position);
            expect(fatal(before.has_value() && after.has_value()));
            expect(before->rect.x == after->rect.x);
            expect(before->rect.y == after->rect.y);
            auto point = LogicalPoint{after->rect.x + 1, after->rect.y + 1};
            auto old_hit = hit_test_layout_tree(expected, point);
            auto new_hit = hit_test_layout_tree(fragmented, point);
            expect(fatal(old_hit.has_value() && new_hit.has_value()));
            expect(old_hit->position == new_hit->position);
        }
    };

    "flow_fragment_virtualization_is_bounded_inside_one_large_list"_test = [] {
        auto parsed = parse_text(1, list_source(8193));
        auto model = build_virtualized_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
        expect(fatal(parsed.document.root.children.size() == 1u));
        expect(model.blocks.size() == (8193u + render_fragment_leaf_limit - 1) / render_fragment_leaf_limit);
        expect(model.materialized_block_indices.empty());
        auto middle = model.blocks.size() / 2;
        reset_core_operation_counters();
        materialize_render_model_range(model, parsed.document, parsed.symbols, default_theme_profile(), middle, middle + 1);
        expect(model.materialized_block_indices.size() == 1u);
        expect(model.blocks[middle].materialized);
        expect(flow_spelling(model.blocks[middle]).size() < 4096u);
        expect(read_core_operation_counters().full_document_block_index_scans == 0u);
        auto fingerprint = model.blocks[middle].presentation_key;
        release_render_model_blocks_outside(model, parsed.document, default_theme_profile(), 0, 0);
        expect(model.materialized_block_indices.empty());
        expect(model.blocks[middle].inline_items.empty());
        expect(model.blocks[middle].child_blocks.empty());
        expect(model.blocks[middle].presentation_key == fingerprint);
    };

    "inline_edit_invalidates_one_fragment_without_scanning_other_list_items"_test = [] {
        auto parsed = parse_text(1, list_source(4097));
        auto model = build_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
        auto owner = parsed.document.cached_editable_order[2048];
        auto changed_index = model.fragment_for_owner(owner).value();
        std::vector<std::uint64_t> before;
        for (auto const& block : model.blocks) before.push_back(block.presentation_key);
        auto* leaf = find_document_block(parsed.document, owner);
        expect(fatal(leaf != nullptr));
        leaf->inline_content.source += U" edited";
        InlineParseContext context;
        context.allocate_id = [&] { return NodeId{parsed.document.next_node_id++}; };
        reparse_inline_document(leaf->inline_content, context);
        ++parsed.document.revision;
        RenderModelUpdate update;
        update.changed_owners.push_back(owner);
        reset_core_operation_counters();
        model = build_render_model_incremental(parsed.document, parsed.outline, parsed.symbols,
            default_theme_profile(), std::move(model), update);
        auto counters = read_core_operation_counters();
        expect(model.incremental_update);
        expect(model.rebuilt_block_count == 1u);
        expect(model.changed_block_indices == std::vector<std::size_t>{changed_index});
        expect(counters.render_source_key_derivations == 1u);
        expect(counters.render_source_nodes_hashed <= 2u * render_fragment_leaf_limit + 1u);
        expect(counters.render_source_characters_hashed < 4096u);
        expect(counters.full_document_block_index_scans == 0u);
        for (std::size_t i = 0; i < model.blocks.size(); ++i)
            expect((model.blocks[i].presentation_key != before[i]) == (i == changed_index));
    };

    "list_structure_edits_preserve_suffix_fragment_boundaries"_test = [] {
        Editor editor(list_source(4097));
        auto model = build_render_model(editor.document(), editor.outline(), editor.symbols(), default_theme_profile());
        auto owner = editor.document().cached_editable_order[2048];
        std::vector<NodeId> suffix_ids;
        std::vector<std::uint64_t> suffix_keys;
        for (std::size_t i = 35; i < model.blocks.size(); ++i) {
            suffix_ids.push_back(model.blocks[i].id);
            suffix_keys.push_back(model.blocks[i].presentation_key);
        }
        auto apply_change = [&] {
            auto change = editor.take_last_document_change();
            expect(fatal(change.has_value()));
            RenderModelUpdate update;
            update.structural = change->structural;
            update.structural_locality_known = change->structural_locality_known;
            update.structural_anchors = change->structural_anchors;
            update.tree_deltas = change->tree_deltas;
            for (auto const& operation : change->text_operations)
                update.changed_owners.push_back((change->forward ? operation.forward : operation.inverse).container_id);
            reset_core_operation_counters();
            model = build_render_model_incremental(editor.document(), editor.outline(), editor.symbols(),
                default_theme_profile(), std::move(model), update);
            auto counters = read_core_operation_counters();
            expect(model.rebuilt_block_count <= 3u);
            expect(counters.render_source_nodes_hashed <= 6u * render_fragment_leaf_limit + 3u);
            expect(counters.render_source_characters_hashed < 12000u);
            expect(counters.render_fragment_nodes_planned < 512u);
            for (std::size_t i = 0; i < suffix_ids.size(); ++i) {
                auto found = std::ranges::find(model.blocks, suffix_ids[i], &RenderBlock::id);
                expect(fatal(found != model.blocks.end()));
                expect(found->presentation_key == suffix_keys[i]);
            }
        };
        editor.set_selection(TextSelection::caret({owner, 4, TextAffinity::Downstream}));
        expect(fatal(editor.execute_command(Command{.kind = CommandKind::InsertNewline})));
        apply_change();
        expect(fatal(editor.undo()));
        apply_change();
        expect(fatal(editor.redo()));
        apply_change();
    };

    "tail_fragment_splices_bound_planning_and_navigation_for_nested_lists_and_history"_test = [] {
        for (auto const& source : std::vector<std::string>{
            list_source(4097),
            list_source(4097, "> - "),
            std::string{"- parent\n"} + list_source(4097, "  - "),
        }) {
            Editor editor(source);
            auto model = build_render_model(editor.document(), editor.outline(), editor.symbols(), default_theme_profile());
            auto tail = editor.document().cached_editable_order.back();
            auto const* leaf = find_document_block(editor.document(), tail);
            expect(fatal(leaf != nullptr));
            editor.set_selection(TextSelection::caret({tail, leaf->inline_content.source.size(), TextAffinity::Downstream}));
            auto project = [&] {
                auto change = editor.take_last_document_change();
                expect(fatal(change.has_value()));
                auto update = fragment_update(*change);
                reset_core_operation_counters();
                model = build_render_model_incremental(editor.document(), editor.outline(), editor.symbols(),
                    default_theme_profile(), std::move(model), update);
                auto work = read_core_operation_counters();
                expect(work.render_fragment_nodes_planned < 512u);
                expect(work.render_navigation_entries_updated < 512u);
                expect(work.render_source_nodes_hashed < 512u);
                expect(work.full_document_block_index_scans == 0u);
                expect(model.rebuilt_block_count <= 3u);
                expect_fragment_navigation(editor, model);
            };
            expect(fatal(editor.execute_command(Command{.kind = CommandKind::InsertNewline})));
            project();
            expect(fatal(editor.undo()));
            project();
            expect(fatal(editor.redo()));
            project();
        }
    };

    "local_fragment_splices_balance_repeated_insertions_and_merge_underflow"_test = [] {
        Editor editor(list_source(257));
        auto model = build_render_model(editor.document(), editor.outline(), editor.symbols(), default_theme_profile());
        auto original_count = model.fragments.size();
        auto owner = editor.document().cached_editable_order[128];
        auto project = [&] {
            auto change = editor.take_last_document_change();
            expect(fatal(change.has_value()));
            model = build_render_model_incremental(editor.document(), editor.outline(), editor.symbols(),
                default_theme_profile(), std::move(model), fragment_update(*change));
        };
        for (std::size_t index = 0; index < 80; ++index) {
            editor.set_selection(TextSelection::caret({owner, 4, TextAffinity::Downstream}));
            expect(fatal(editor.execute_command(Command{.kind = CommandKind::InsertNewline})));
            project();
        }
        expect(model.fragments.size() <= original_count + 3u);
        expect_fragment_navigation(editor, model);
        for (std::size_t index = 0; index < 80; ++index) {
            expect(fatal(editor.undo()));
            project();
        }
        expect(model.fragments.size() <= original_count + 1u);
        expect_fragment_navigation(editor, model);
    };

    "fragment_decorations_continue_only_at_cut_container_edges"_test = [] {
        auto parsed = parse_text(1, std::string{"> [!WARNING]\n"} + list_source(130, "> - "));
        auto model = build_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
        expect(fatal(model.blocks.size() == 3u));
        expect(!model.blocks.front().flow_continues_before);
        expect(model.blocks.front().flow_continues_after);
        expect(model.blocks[1].flow_continues_before);
        expect(model.blocks[1].flow_continues_after);
        expect(model.blocks.back().flow_continues_before);
        expect(!model.blocks.back().flow_continues_after);
    };

    "fragment_identity_reuse_requires_reconciling_changed_semantic_roots"_test = [] {
        auto parsed = parse_text(1, list_source(130));
        auto model = build_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
        std::vector<NodeId> identities;
        for (auto const& fragment : model.fragments) identities.push_back(fragment.id);
        auto previous_root = parsed.document.root.children.front().id;
        BlockNode quote;
        quote.id = NodeId{parsed.document.next_node_id++};
        quote.kind = BlockKind::BlockQuote;
        quote.children.push_back(std::move(parsed.document.root.children.front()));
        parsed.document.root.children.front() = std::move(quote);
        rebuild_document_block_index(parsed.document);
        ++parsed.document.revision;
        RenderModelUpdate update;
        update.structural = true;
        update.structural_locality_known = false;
        model = build_render_model_incremental(parsed.document, parsed.outline, parsed.symbols,
            default_theme_profile(), std::move(model), update);
        expect(fatal(model.fragments.size() == identities.size()));
        for (std::size_t index = 0; index < identities.size(); ++index) {
            expect(model.fragments[index].id == identities[index]);
            expect(model.fragments[index].source_root != previous_root);
        }
        expect(!model.incremental_update);
    };
};
