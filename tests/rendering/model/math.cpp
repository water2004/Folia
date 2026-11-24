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

suite rendering_model_math_tests = [] {

"builds_inline_math"_test = [] {
    auto m = build_model("$x^2$");
    expect(fatal(bool(!m.blocks.empty())));
    bool has_math = false;
    for (const auto& it : m.blocks[0].inline_items)
        if (it.kind == InlineRenderItem::Kind::Math) has_math = true;
    expect(fatal(bool(has_math)));
};

"inline_math_only_line_preserves_full_visual_source_range"_test = [] {
    auto m = build_model("$11111$");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    expect(fatal(bool((m.blocks[0].source_span.source_range.start) == (0u))));
    expect(fatal(bool((m.blocks[0].source_span.source_range.end) == (7u))));
    expect(fatal(bool((m.blocks[0].content_span.source_range.start) == (0u))));
    expect(fatal(bool((m.blocks[0].content_span.source_range.end) == (7u))));
    expect(fatal(bool((m.blocks[0].inline_items.size()) == (1u))));
    expect(fatal(bool(m.blocks[0].inline_items[0].kind == InlineRenderItem::Kind::Math)));
    expect(fatal(bool((m.blocks[0].inline_items[0].source_span.source_range.start) == (0u))));
    expect(fatal(bool((m.blocks[0].inline_items[0].source_span.source_range.end) == (7u))));
};

"inline_math_at_either_text_edge_preserves_paragraph_end"_test = [] {
    auto leading = build_model("$x$ tail");
    expect(fatal(bool((leading.blocks.size()) == (1u))));
    expect(fatal(bool((leading.blocks[0].content_span.source_range.start) == (0u))));
    expect(fatal(bool((leading.blocks[0].content_span.source_range.end) == (8u))));
    auto trailing = build_model("lead $x$");
    expect(fatal(bool((trailing.blocks.size()) == (1u))));
    expect(fatal(bool((trailing.blocks[0].content_span.source_range.start) == (0u))));
    expect(fatal(bool((trailing.blocks[0].content_span.source_range.end) == (8u))));
};

"builds_paren_inline_math_with_exact_source_range"_test = [] {
    auto m = build_model("\\(x\\) after");
    auto it = std::find_if(m.blocks[0].inline_items.begin(), m.blocks[0].inline_items.end(), [](auto const& item) {
        return item.kind == InlineRenderItem::Kind::Math;
    });
    expect(fatal(bool(it != m.blocks[0].inline_items.end())));
    expect(fatal(bool(it->special().math_delim == MathDelimiter::InlineParen)));
    expect(fatal(bool((it->source_span.source_range.start) == (0u))));
    expect(fatal(bool((it->source_span.source_range.end) == (5u))));
};

"list_inline_math_reaches_the_render_model_with_exact_ranges"_test = [] {
    auto m = build_model("- before $x^2$ after\n- next $y$\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    std::vector<TextSpan> math_ranges;
    for (auto const& item : m.blocks[0].inline_items) {
        if (item.kind == InlineRenderItem::Kind::Math) math_ranges.push_back(item.source_span);
    }
    expect(fatal(bool((math_ranges.size()) == (2u))));
    expect(fatal(bool((math_ranges[0].source_range.start) == (7u))));
    expect(fatal(bool((math_ranges[0].source_range.end) == (12u))));
    expect(fatal(bool((math_ranges[1].source_range.start) == (5u))));
    expect(fatal(bool((math_ranges[1].source_range.end) == (8u))));
    expect(fatal(bool(math_ranges[0].container_id != math_ranges[1].container_id)));
};

"math_inside_emphasis_renders_without_inheriting_text_style"_test = [] {
    auto m = build_model("**before $x$ after** *\\(y\\)* ~~$z$~~\n- **$q$**\n");
    std::vector<InlineRenderItem> math;
    for (auto const& block : m.blocks) {
        for (auto const& item : block.inline_items) {
            if (item.kind == InlineRenderItem::Kind::Math) math.push_back(item);
        }
    }
    expect(fatal(bool((math.size()) == (4u))));
    for (auto const& item : math) {
        expect(fatal(bool(!item.style.bold)));
        expect(fatal(bool(!item.style.italic)));
    }
    expect(fatal(bool(!math[0].style.strikethrough)));
    expect(fatal(bool(!math[1].style.strikethrough)));
    expect(fatal(bool(math[2].style.strikethrough)));
    expect(fatal(bool(!math[3].style.strikethrough)));
    bool bold_text = false;
    for (auto const& item : m.blocks[0].inline_items) {
        if (item.kind == InlineRenderItem::Kind::Text && item.style.bold) bold_text = true;
    }
    expect(fatal(bool(bold_text)));
};

"display_math_keeps_an_explicit_block_container"_test = [] {
    auto model = build_model("$$\nx + y\n$$");
    expect(fatal(bool(model.blocks.size() == 1u)));
    expect(fatal(bool(model.blocks.front().kind == RenderBlockKind::Math)));
    expect(fatal(bool(model.blocks.front().block_style.background.has_value())));
};

};
