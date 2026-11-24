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

suite rendering_model_html_images_quotes_tests = [] {

"inline_and_block_images_keep_distinct_flow_alignment"_test = [] {
    auto inline_model = build_model("before ![inline](inline.png) after");
    expect(fatal(bool(inline_model.blocks.size() == 1u)));
    auto inline_image = std::ranges::find(
        inline_model.blocks.front().inline_items,
        InlineRenderItem::Kind::Image,
        &InlineRenderItem::kind);
    expect(fatal(bool(inline_image != inline_model.blocks.front().inline_items.end())));
    if (inline_image != inline_model.blocks.front().inline_items.end())
        expect(fatal(bool(!inline_image->special().semantic().block_image)));

    auto nested_model = build_model("> - ![block](block.gif)");
    expect(fatal(bool(nested_model.blocks.size() == 1u)));
    auto nested_image = std::ranges::find(
        nested_model.blocks.front().inline_items,
        InlineRenderItem::Kind::Image,
        &InlineRenderItem::kind);
    expect(fatal(bool(nested_image != nested_model.blocks.front().inline_items.end())));
    if (nested_image != nested_model.blocks.front().inline_items.end())
        expect(fatal(bool(nested_image->special().semantic().block_image)));
};

"linked_html_images_keep_text_only_link_decoration"_test = [] {
    auto model = build_model(
        "<p align=\"center\">\n"
        "  <a href=\"https://example.com\">\n"
        "    <img src=\"badge.svg\">\n"
        "  </a>\n"
        "</p>\n\n"
        "[linked text](https://example.com)\n");

    bool linked_image = false;
    bool decorated_text = false;
    bool decorated_whitespace = false;
    auto visit_items = [&](auto&& self, const std::vector<InlineRenderItem>& items, bool inside_link) -> void {
        for (auto const& item : items) {
            if (item.kind == InlineRenderItem::Kind::Image && inside_link) {
                linked_image = true;
                expect(bool(!item.style.link));
            }
            if (item.kind == InlineRenderItem::Kind::Text && inside_link) {
                const auto whitespace = !item.text.empty() && std::ranges::all_of(item.text, [](char32_t ch) {
                    return ch == U' ' || ch == U'\t' || ch == U'\n' || ch == U'\r';
                });
                if (whitespace && item.style.link) decorated_whitespace = true;
                if (!whitespace && item.style.link) decorated_text = true;
            }
            if (item.kind == InlineRenderItem::Kind::Link) {
                self(self, item.special().semantic().children, true);
            }
        }
    };
    auto visit_block = [&](auto&& self, const RenderBlock& block) -> void {
        visit_items(visit_items, block.inline_items, false);
        for (auto const& child : block.child_blocks) self(self, child);
    };
    for (auto const& block : model.blocks) visit_block(visit_block, block);

    expect(fatal(bool(linked_image))) << "linked image reaches the render model";
    expect(fatal(bool(decorated_text))) << "link text keeps its decoration";
    expect(fatal(bool(!decorated_whitespace))) << "link indentation remains undecorated";
};

"html_paragraph_alignment_and_formatting_whitespace_project_semantically"_test = [] {
    auto text_model = build_model(
        "<p align=\"center\">\n"
        "  面向高并发场景的现代化外置登录与材质平台\n"
        "</p>");
    expect(fatal(text_model.blocks.size() == 1u));
    if (text_model.blocks.empty()) return;
    expect(fatal(text_model.blocks.front().block_style.text_alignment
        == std::optional<TextAlignment>{TextAlignment::Center}));
    expect(fatal(text_model.blocks.front().inline_items.size() == 1u));
    if (!text_model.blocks.front().inline_items.empty()) {
        expect(fatal(text_model.blocks.front().inline_items.front().text
            == U"面向高并发场景的现代化外置登录与材质平台"));
        expect(fatal(text_model.blocks.front().inline_items.front().source_span.source_range.start == 3u));
    }

    auto badges = build_model(
        "<p align=\"center\">\n"
        "  <a href='one'>\n    <img src='one.svg'>\n  </a>\n"
        "  <a href='two'>\n    <img src='two.svg'>\n  </a>\n"
        "  <img src='three.svg'>\n"
        "</p>");
    expect(fatal(badges.blocks.size() == 1u));
    if (badges.blocks.empty()) return;
    std::size_t links = 0;
    std::size_t images = 0;
    std::size_t separating_spaces = 0;
    for (const auto& item : badges.blocks.front().inline_items) {
        if (item.kind == InlineRenderItem::Kind::Link) {
            ++links;
            for (const auto& child : item.special().semantic().children) {
                if (child.kind == InlineRenderItem::Kind::Image) ++images;
                expect(!(child.kind == InlineRenderItem::Kind::Text
                    && child.text == U" ")) << "media-only anchors discard formatting indentation";
            }
        } else if (item.kind == InlineRenderItem::Kind::Image) {
            ++images;
        } else if (item.kind == InlineRenderItem::Kind::Text && item.text == U" ") {
            ++separating_spaces;
        }
    }
    expect(fatal(links == 2u));
    expect(fatal(images == 3u));
    expect(fatal(separating_spaces == 2u));
};

"adjacent_html_paragraphs_keep_independent_alignment_in_one_html_block"_test = [] {
    const auto model = build_model(
        "<p align='center'>first</p>\n<p align='right'>second</p>");
    expect(fatal(model.blocks.size() == 1u));
    const auto& children = model.blocks.front().child_blocks;
    expect(fatal(children.size() == 2u));
    expect(children[0].block_style.text_alignment == std::optional{TextAlignment::Center});
    expect(children[1].block_style.text_alignment == std::optional{TextAlignment::End});
    expect(fatal(!children[0].inline_items.empty()));
    expect(fatal(!children[1].inline_items.empty()));
    expect(children[0].inline_items.front().text == U"first");
    expect(children[1].inline_items.front().text == U"second");
};

"blockquote_render_model_contains_text_and_quote_style"_test = [] {
    auto m = build_model("> quoted text\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    expect(fatal(bool(m.blocks[0].kind == RenderBlockKind::Quote)));
    expect(fatal(bool((m.blocks[0].child_blocks.size()) == (1u))));
    expect(fatal(bool(m.blocks[0].block_style.border_left.has_value())));
    bool hasText = false;
    if (!m.blocks[0].child_blocks.empty()) {
        expect(fatal(bool(!m.blocks[0].child_blocks[0].inline_items.empty())));
        for (auto const& item : m.blocks[0].child_blocks[0].inline_items) {
            hasText = hasText || (item.kind == InlineRenderItem::Kind::Text && item.text == U"quoted text");
        }
    }
    expect(fatal(bool(hasText)));
};

"blockquote_render_model_preserves_nested_child_blocks"_test = [] {
    auto m = build_model("> paragraph\n> > nested\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    expect(fatal(bool(m.blocks[0].kind == RenderBlockKind::Quote)));
    expect(fatal(bool((m.blocks[0].child_blocks.size()) == (2u))));
    if (m.blocks[0].child_blocks.size() >= 2) {
        expect(fatal(bool(m.blocks[0].child_blocks[0].kind == RenderBlockKind::Text)));
        expect(fatal(bool(m.blocks[0].child_blocks[1].kind == RenderBlockKind::Quote)));
        expect(fatal(bool((m.blocks[0].child_blocks[0].flow_local_indent_columns) == (2u))));
        expect(fatal(bool((m.blocks[0].child_blocks[1].flow_local_indent_columns) == (2u))));
        expect(fatal(bool((m.blocks[0].child_blocks[1].child_blocks.size()) == (1u))));
        if (!m.blocks[0].child_blocks[1].child_blocks.empty())
            expect(fatal(bool((m.blocks[0].child_blocks[1].child_blocks[0].flow_local_indent_columns) == (2u))));
    }
};

};
