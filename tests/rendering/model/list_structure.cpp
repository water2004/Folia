#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.core.layout_tree;
import folia.core.types;
import folia.markdown.symbols;
import folia.markdown.text_edit;
import folia.markdown.parser;
import folia.markdown.dialect;
import folia.core.document_edit;
import folia.markdown.document;
import folia.markdown.document_symbols;
import folia.markdown.inline_parser;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.callout;
import folia.core.text_measurer;
import folia.core.block_layout;
import folia.core.hit_test;
import folia.core.selection_geometry;
import folia.markdown.selection;

using namespace folia;
using namespace boost::ut;

#include "support/render_layout_test_support.hpp"

suite rendering_model_list_structure_tests = [] {

"mixed_task_list_renders_checkbox_only_for_task_items"_test = [] {
    auto dialect = default_dialect();
    dialect.gfm.task_lists = true;
    auto parsed = parse_text(
        1,
        "- [@] regular\n- [x] task\n",
        dialect);
    auto model = build_render_model(
        parsed.document,
        parsed.outline,
        parsed.symbols,
        default_theme_profile());
    expect(fatal(model.blocks.size() == 1));
    if (model.blocks.empty()) return;
    std::size_t checkboxes = 0;
    std::size_t bullets = 0;
    for (const auto& item : model.blocks.front().inline_items) {
        if (item.kind != InlineRenderItem::Kind::Marker) continue;
        if (item.special().marker_role == MarkerRole::TaskCheckbox) {
            ++checkboxes;
            expect(fatal(item.special().task_checked));
        }
        if (item.special().marker_role == MarkerRole::ListBullet) ++bullets;
    }
    expect(fatal(checkboxes == 1));
    expect(fatal(bullets == 1));
};

"ordered_task_item_keeps_the_ordered_list_and_checkbox_markers"_test = [] {
    auto dialect = default_dialect();
    dialect.gfm.task_lists = true;
    auto parsed = parse_text(1, "3. [x] done\n", dialect);
    auto model = build_render_model(
        parsed.document,
        parsed.outline,
        parsed.symbols,
        default_theme_profile());
    expect(fatal(model.blocks.size() == 1));
    if (model.blocks.empty()) return;
    bool task_marker = false;
    for (const auto& item : model.blocks.front().inline_items) {
        if (item.kind == InlineRenderItem::Kind::Marker
            && item.special().marker_role == MarkerRole::TaskCheckbox) {
            task_marker = true;
            expect(fatal(item.special().task_checked));
            expect(fatal(item.special().display_text == U"3. [x] "));
        }
    }
    expect(fatal(task_marker));
};

"list_nested_quote_keeps_structural_layout_metadata"_test = [] {
    auto model = build_model("- item\n  > quoted");
    expect(fatal(bool(model.blocks.size() == 1u)));
    expect(fatal(bool(model.blocks.front().kind == RenderBlockKind::Text)));
    auto quote = find_render_block(model.blocks.front(), RenderBlockKind::Quote);
    expect(fatal(bool(quote != nullptr)));
    if (quote) {
        expect(fatal(bool(flow_indent_for(model.blocks.front(), quote->id) == 2u)));
        expect(fatal(bool(!quote->child_blocks.empty())));
        auto marker = std::find_if(model.blocks.front().inline_items.begin(), model.blocks.front().inline_items.end(), [&](auto const& item) {
            return item.special().marker_role == MarkerRole::ListBullet && item.source_span.container_id.v != 0;
        });
        expect(fatal(bool(marker != model.blocks.front().inline_items.end())));
    }
    auto indent = std::find_if(model.blocks.front().inline_items.begin(), model.blocks.front().inline_items.end(), [](auto const& item) {
        return item.special().marker_role == MarkerRole::Structural && !item.special().display_text.empty()
            && std::all_of(item.special().display_text.begin(), item.special().display_text.end(), [](char32_t value) { return value == U' '; });
    });
    expect(fatal(bool(indent != model.blocks.front().inline_items.end())));
};

"quote_only_list_item_maps_its_marker_to_editable_content"_test = [] {
    auto model = build_model("- > quoted\n");
    expect(fatal(bool(model.blocks.size() == 1u)));
    if (model.blocks.empty()) return;
    auto const& list = model.blocks.front();
    auto quote = find_render_block(list, RenderBlockKind::Quote);
    expect(fatal(bool(quote != nullptr)));
    if (!quote) return;
    auto editable = first_render_leaf(*quote);
    expect(fatal(bool(editable != nullptr)));
    if (!editable) return;
    auto editableOwner = editable->source_span.container_id;
    auto marker = std::find_if(list.inline_items.begin(), list.inline_items.end(), [](auto const& item) {
        return item.special().marker_role == MarkerRole::ListBullet;
    });
    expect(fatal(bool(marker != list.inline_items.end())));
    if (marker != list.inline_items.end())
        expect(fatal(bool(marker->source_span.container_id == editableOwner)));
    auto indent = std::find_if(list.inline_items.begin(), list.inline_items.end(), [&](auto const& item) {
        return item.source_span.container_id == editableOwner
            && item.special().marker_role == MarkerRole::Structural
            && item.special().display_text == U"  ";
    });
    expect(fatal(bool(indent != list.inline_items.end())));
};

"list_render_model_retains_nested_quote_identity_and_indent"_test = [] {
    auto m = build_model("- first\n- second\n  > quoted\n- third\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    if (m.blocks.empty()) return;
    auto const& list = m.blocks[0];
    auto quote = find_render_block(list, RenderBlockKind::Quote);
    expect(fatal(bool(quote != nullptr)));
    if (quote) {
        expect(fatal(bool(flow_indent_for(list, quote->id) == 2u)));
        expect(fatal(bool(!quote->child_blocks.empty())));
        if (!quote->child_blocks.empty()) {
            auto const quoteContent = quote->child_blocks.front().source_span;
            auto trailingBreak = std::find_if(list.inline_items.begin(), list.inline_items.end(), [&](auto const& item) {
                return item.special().marker_role == MarkerRole::Structural && item.text == U"\n"
                    && item.source_span.container_id == quoteContent.container_id
                    && item.source_span.source_range.start == quoteContent.source_range.end;
            });
            expect(fatal(bool(trailingBreak != list.inline_items.end())));
        }
    }
};

"nested_list_markers_use_semantic_depth"_test = [] {
    auto m = build_model("1. first\n2. second\n   - nested\n   - nested again\n3. third\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    if (m.blocks.empty()) return;
    std::vector<std::u32string> bullets;
    for (auto const& item : m.blocks[0].inline_items) {
        if (item.special().marker_role == MarkerRole::ListBullet) {
            bullets.push_back(item.special().display_text);
        }
    }
    expect(fatal(bool((bullets.size()) == (2u))));
    if (bullets.size() == 2) {
        expect(fatal(bool((bullets[0]) == (U"   \u2022 "))));
        expect(fatal(bool((bullets[1]) == (U"   \u2022 "))));
    }
};

};
