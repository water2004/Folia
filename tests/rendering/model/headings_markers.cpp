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

suite rendering_model_headings_markers_tests = [] {

"test_empty_document_yields_editable_blank_model"_test = [] {
    EditorDocument doc = EditorDocument::empty(1);
    Outline o = Outline::empty(1);
    auto m = build_render_model(doc, o, DocumentSymbolIndex{}, default_theme_profile());
    expect(fatal(bool((m.blocks.size()) == (1u))));
    expect(fatal(bool(m.blocks[0].kind == RenderBlockKind::Blank)));
    expect(fatal(bool((m.revision) == (1ull))));
};

"builds_heading_with_marker_and_text"_test = [] {
    auto m = build_model("# Title");
    expect(fatal(bool(!m.blocks.empty())));
    bool has_opening = false;
    for (const auto& it : m.blocks[0].inline_items) {
        expect(fatal(bool(it.source_span.source_range.end >= it.source_span.source_range.start)));
        if (it.kind == InlineRenderItem::Kind::Marker && it.special().marker_role == MarkerRole::Heading) {
            has_opening = true;
            expect(fatal(bool(it.special().generated_boundary_affinity == TextAffinity::Downstream)));
        }
    }
    bool has_text = false;
    for (const auto& it : m.blocks[0].inline_items) if (it.kind == InlineRenderItem::Kind::Text) has_text = true;
    expect(fatal(bool(has_opening)));
    expect(fatal(bool(has_text)));
    expect(fatal(bool(m.blocks[0].text_heading_level == 1u)));
    expect(fatal(bool(m.blocks[0].estimated_characters >= 7u)));
};

"builds_setext_heading_with_a_structural_trailing_marker"_test = [] {
    auto m = build_model("Section\n-------\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    bool headingText = false;
    bool underline = false;
    if (!m.blocks.empty()) {
        expect(fatal(bool(m.blocks[0].kind == RenderBlockKind::Text)));
        for (auto const& item : m.blocks[0].inline_items) {
            if (item.kind == InlineRenderItem::Kind::Text && item.style.heading_level && *item.style.heading_level == 2) headingText = true;
            if (item.kind == InlineRenderItem::Kind::Marker && item.special().marker_role == MarkerRole::Heading && item.text == U"\n-------") {
                underline = true;
                expect(fatal(bool(item.special().generated_boundary_affinity == TextAffinity::Upstream)));
            }
        }
    }
    expect(fatal(bool(headingText)));
    expect(fatal(bool(underline)));
};

"atx_heading_link_reaches_the_render_model_as_a_link"_test = [] {
    auto m = build_model("### [#](https://example.com)可做转义的字符\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    if (!m.blocks.empty()) expect(fatal(bool(std::any_of(m.blocks[0].inline_items.begin(), m.blocks[0].inline_items.end(), [](auto const& item) {
            return item.kind == InlineRenderItem::Kind::Link
                && item.special().semantic().href == "https://example.com";
        }))));
};

"builds_strong_with_open_and_close_markers"_test = [] {
    auto m = build_model("**bold**");
    int marker_count = 0;
    for (const auto& it : m.blocks[0].inline_items) {
        if (it.kind == InlineRenderItem::Kind::Marker && it.text == U"**") ++marker_count;
    }
    expect(fatal(bool((marker_count) == (2))));
};

"render_markers_are_exact_source_delimiters"_test = [] {
    struct Case {
        std::string source;
        std::u32string marker;
    };
    const std::vector<Case> cases{
        {"*word*", U"*"},
        {"_word_", U"_"},
        {"**word**", U"**"},
        {"__word__", U"__"},
        {"~~word~~", U"~~"},
    };
    for (auto const& test : cases) {
        auto model = build_model(test.source);
        expect(fatal(bool((model.blocks.size()) == (1u))));
        if (model.blocks.empty()) continue;
        std::vector<InlineRenderItem const*> markers;
        for (auto const& item : model.blocks.front().inline_items) {
            if (item.kind == InlineRenderItem::Kind::Marker) markers.push_back(&item);
        }
        expect(fatal(bool((markers.size()) == (2u))));
        if (markers.size() != 2) continue;
        expect(fatal(bool((markers[0]->text) == (test.marker))));
        expect(fatal(bool((markers[1]->text) == (test.marker))));
        expect(fatal(bool((markers[0]->source_span.source_range.start) == (0u))));
        expect(fatal(bool((markers[0]->source_span.source_range.end) == (test.marker.size()))));
        expect(fatal(bool((markers[1]->source_span.source_range.start) == (test.source.size() - test.marker.size()))));
        expect(fatal(bool((markers[1]->source_span.source_range.end) == (test.source.size()))));
        expect(fatal(bool(markers[0]->special().marker_owner.has_value())));
        expect(fatal(bool(markers[1]->special().marker_owner == markers[0]->special().marker_owner)));
    }
};

"render_nested_delimiters_keep_monotonic_exact_ranges"_test = [] {
    auto model = build_model("_outer **inner** tail_");
    expect(fatal(bool((model.blocks.size()) == (1u))));
    if (model.blocks.empty()) return;
    std::vector<InlineRenderItem const*> markers;
    for (auto const& item : model.blocks.front().inline_items) {
        if (item.kind == InlineRenderItem::Kind::Marker) markers.push_back(&item);
    }
    expect(fatal(bool((markers.size()) == (4u))));
    if (markers.size() == 4) {
        expect(fatal(bool((markers[0]->text) == (std::u32string(U"_")))));
        expect(fatal(bool((markers[0]->source_span.source_range.start) == (0u))));
        expect(fatal(bool((markers[1]->text) == (std::u32string(U"**")))));
        expect(fatal(bool((markers[1]->source_span.source_range.start) == (7u))));
        expect(fatal(bool((markers[2]->text) == (std::u32string(U"**")))));
        expect(fatal(bool((markers[2]->source_span.source_range.start) == (14u))));
        expect(fatal(bool((markers[3]->text) == (std::u32string(U"_")))));
        expect(fatal(bool((markers[3]->source_span.source_range.start) == (21u))));
    }
    auto const& items = model.blocks.front().inline_items;
    for (std::size_t index = 1; index < items.size(); ++index) {
        if (items[index - 1].source_span.container_id == items[index].source_span.container_id) {
            expect(fatal(bool(items[index - 1].source_span.source_range.end <= items[index].source_span.source_range.start)));
        }
    }
};

"thematic_break_remains_an_atomic_render_block"_test = [] {
    auto model = build_model("---");
    expect(fatal(bool((model.blocks.size()) == (1u))));
    expect(fatal(bool(model.blocks[0].kind == RenderBlockKind::ThematicBreak)));
    expect(fatal(bool(model.blocks[0].inline_items.empty())));
    expect(fatal(bool((model.blocks[0].source_span.source_range.start) == (0u))));
    expect(fatal(bool((model.blocks[0].source_span.source_range) == (SourceRange{0, 1}))));
    expect(fatal(bool((model.blocks[0].content_span.source_range) == (SourceRange{0, 1}))));
    StubMeasurer measurer(8.0f);
    auto layout = layout_blocks(model.blocks, 800.0f, 1.0f, measurer, std::nullopt, LogicalPoint(0, 0), Outline::empty(1));
    expect(fatal(bool((layout.blocks.size()) == (1u))));
    expect(fatal(bool(layout.blocks[0].kind.kind == LayoutBlockKind::ThematicBreak)));
    expect(fatal(bool((layout.blocks[0].source_span.source_range) == (SourceRange{0, 1}))));
};

};
