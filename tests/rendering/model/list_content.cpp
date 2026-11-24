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
import folia.markdown.dialect;
import folia.markdown.text_edit;
import folia.markdown.parser;
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

suite rendering_model_list_content_tests = [] {

"mixed_list_flow_does_not_promote_plain_text_to_nested_heading_style"_test = [] {
    auto model = build_model("1. item\n   ## nested heading\n   body\n");
    expect(fatal(bool(model.blocks.size() == 1u)));
    if (model.blocks.empty()) return;
    auto const& list = model.blocks.front();
    expect(fatal(bool(list.text_heading_level == 0u)));
    expect(fatal(bool(std::ranges::any_of(list.inline_items, [](auto const& item) {
        return item.style.heading_level && *item.style.heading_level == 2u;
    }))));
    expect(fatal(bool(std::ranges::any_of(list.inline_items, [](auto const& item) {
        auto const& text = item.special().display_text.empty() ? item.text : item.special().display_text;
        return !text.empty() && !item.style.heading_level;
    }))));
};

"identifier_underscores_stay_plain_and_following_heading_stays_top_level"_test = [] {
    auto model = build_model(
        "## 3.1 gtsi\n\n"
        "1. 位选通 0x1_fffff_ffff 共33bit\n"
        "   trig_out(FS17);\n"
        "## 3.2 next\n");
    expect(fatal(bool(model.blocks.size() == 3u)));
    if (model.blocks.size() != 3) return;
    expect(fatal(bool(model.blocks[0].text_heading_level == 2u)));
    expect(fatal(bool(model.blocks[1].text_heading_level == 0u)));
    expect(fatal(bool(model.blocks[2].text_heading_level == 2u)));
    expect(fatal(bool(std::ranges::none_of(model.blocks[1].inline_items, [](auto const& item) {
        return item.style.bold || item.style.italic || item.style.heading_level.has_value();
    }))));
};

"builds_ordered_list_with_source_markers_and_line_breaks"_test = [] {
    auto m = build_model("9. alpha\n10. beta\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    std::u32string markers;
    for (auto const& item : m.blocks[0].inline_items) {
        if (item.kind == InlineRenderItem::Kind::Marker) markers += item.text;
    }
    expect(fatal(bool(markers.find(U"9. ") != std::u32string::npos)));
    expect(fatal(bool(markers.find(U"10. ") != std::u32string::npos)));
    expect(fatal(bool(markers.find(U"\n") != std::u32string::npos)));
};

"ordered_list_display_numbers_are_sequential"_test = [] {
    auto m = build_model("1. first\n8. second\n3. third\n");
    std::vector<std::u32string> source;
    std::vector<std::u32string> display;
    for (auto const& item : m.blocks[0].inline_items) {
        if (item.kind != InlineRenderItem::Kind::Marker
            || item.special().marker_role != MarkerRole::ListNumber) continue;
        source.push_back(item.text);
        display.push_back(item.special().display_text);
    }
    expect(fatal(bool((source.size()) == (3u))));
    expect(fatal(bool(source[0] == U"1. ")));
    expect(fatal(bool(source[1] == U"8. ")));
    expect(fatal(bool(source[2] == U"3. ")));
    expect(fatal(bool(display[0] == U"1. ")));
    expect(fatal(bool(display[1] == U"2. ")));
    expect(fatal(bool(display[2] == U"3. ")));
};

"unordered_list_preserves_source_markers_and_presents_bullets"_test = [] {
    auto m = build_model("- alpha\n* beta\n+ gamma\n");
    // CommonMark starts a new list when the bullet glyph changes.
    expect(fatal(bool((m.blocks.size()) == (3u))));
    std::vector<std::u32string> source_markers;
    std::vector<std::u32string> display_markers;
    for (auto const& block : m.blocks) {
        expect(fatal(bool(block.block_style.padding_left == 20.0f)));
        for (auto const& item : block.inline_items) {
            if (item.kind != InlineRenderItem::Kind::Marker
                || item.special().marker_role != MarkerRole::ListBullet) continue;
            source_markers.push_back(item.text);
            display_markers.push_back(item.special().display_text);
            expect(fatal(bool(item.source_span.source_range.empty())));
        }
    }
    expect(fatal(bool((source_markers.size()) == (3u))));
    expect(fatal(bool(source_markers[0] == U"- ")));
    expect(fatal(bool(source_markers[1] == U"* ")));
    expect(fatal(bool(source_markers[2] == U"+ ")));
    for (auto const& marker : display_markers) expect(fatal(bool(marker == U"\u2022 ")));
};

"task_list_markers_expose_checkbox_state_without_losing_source_spelling"_test = [] {
    auto model = build_model("- [ ] pending\n- [x] complete\n- [X] complete too\n");
    expect(fatal(bool(model.blocks.size() == 1u)));
    if (model.blocks.empty()) return;
    std::vector<InlineRenderItem const*> markers;
    for (auto const& item : model.blocks.front().inline_items) {
        if (item.kind == InlineRenderItem::Kind::Marker
            && item.special().marker_role == MarkerRole::TaskCheckbox) markers.push_back(&item);
    }
    expect(fatal(bool(markers.size() == 3u)));
    if (markers.size() != 3u) return;
    expect(fatal(bool(markers[0]->text == U"- [ ] ")));
    expect(fatal(bool(markers[1]->text == U"- [x] ")));
    expect(fatal(bool(markers[2]->text == U"- [X] ")));
    expect(fatal(bool(!markers[0]->special().task_checked)));
    expect(fatal(bool(markers[1]->special().task_checked)));
    expect(fatal(bool(markers[2]->special().task_checked)));
    for (auto const* marker : markers) {
        expect(fatal(bool(marker->source_span.source_range.empty())));
        expect(fatal(bool(marker->special().generated_boundary_affinity == TextAffinity::Downstream)));
    }
};

"nested_task_lists_keep_checkbox_projection_state_in_the_unified_flow"_test = [] {
    auto model = build_model("> - [x] parent\n>   - [ ] child\n");
    expect(fatal(bool(model.blocks.size() == 1u)));
    if (model.blocks.empty()) return;
    std::vector<bool> states;
    for (auto const& item : model.blocks.front().inline_items) {
        if (item.kind == InlineRenderItem::Kind::Marker
            && item.special().marker_role == MarkerRole::TaskCheckbox) {
            states.push_back(item.special().task_checked);
        }
    }
    expect(fatal(bool(states.size() == 2u)));
    if (states.size() == 2u) {
        expect(fatal(bool(states[0])));
        expect(fatal(bool(!states[1])));
    }
};

"nested_list_blocks_reach_the_render_model"_test = [] {
    auto m = build_model("- parent\n  - child\n    1. grandchild\n\n  > quoted\n\n  ![diagram](image.png)\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    std::u32string text;
    bool image = false;
    bool block_image = false;
    bool quote = false;
    for (auto const& item : m.blocks[0].inline_items) {
        if (item.kind == InlineRenderItem::Kind::Image) {
            image = true;
            block_image = item.special().semantic().block_image;
        }
        else if (item.kind == InlineRenderItem::Kind::Link) {
            for (auto const& child : item.special().semantic().children) text += child.text;
        }
        else text += item.text;
    }
    quote = find_render_block(m.blocks[0], RenderBlockKind::Quote) != nullptr;
    expect(fatal(bool(text.find(U"parent") != std::u32string::npos)));
    expect(fatal(bool(text.find(U"child") != std::u32string::npos)));
    expect(fatal(bool(text.find(U"grandchild") != std::u32string::npos)));
    expect(fatal(bool(text.find(U"quoted") != std::u32string::npos)));
    expect(fatal(bool(image)));
    expect(fatal(bool(block_image)));
    expect(fatal(bool(quote)));
};

};
