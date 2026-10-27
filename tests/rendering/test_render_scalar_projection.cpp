#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
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

suite render_scalar_projection_tests = [] {

"inline_code_display_normalizes_newlines_and_keeps_source_spans"_test = [] {
    struct Case {
        std::string source;
        std::u32string expected_display;
        bool has_outer_padding_markers;
        bool has_crlf_segment;
    };
    const std::vector<Case> cases{
        {"` a\r\nb `", U"a b", true, true},
        {"` a\n b `", U"a  b", true, false},
        {"`  x  `", U" x ", true, false},
        {"`a\rb`", U"a b", false, false},
        {"`  `", U"  ", false, false},
        {"` \r\n `", U"   ", false, true},
        {"`x\ty`", U"x\ty", false, false},
    };

    for (const auto& test_case : cases) {
        auto model = build_model(test_case.source);
        expect(fatal(bool(model.blocks.size() == 1u)));
        if (model.blocks.size() != 1u) continue;

        auto const& items = model.blocks.front().inline_items;
        auto opening = std::find_if(items.begin(), items.end(), [](auto const& item) {
            return item.kind == InlineRenderItem::Kind::Marker && item.text == U"`"
                && item.special().marker_owner.has_value();
        });
        expect(fatal(bool(opening != items.end())));
        if (opening == items.end() || !opening->id) continue;
        const auto code_id = *opening->id;

        std::u32string displayed;
        std::vector<InlineRenderItem const*> code_items;
        std::vector<InlineRenderItem const*> padding_markers;
        auto has_crlf_segment = false;
        for (auto const& item : items) {
            if (item.id != code_id) continue;
            code_items.push_back(&item);
            if (item.kind == InlineRenderItem::Kind::Text && item.style.code == CodeStyle::Inline) {
                displayed += item.text;
                if (item.text == U" " && item.source_span.source_range.length() == 2) {
                    has_crlf_segment = true;
                    expect(fatal(bool(item.source_span.source_range.end
                        == item.source_span.source_range.start + 2)));
                }
            } else if (item.kind == InlineRenderItem::Kind::Marker
                && item.text == U" " && item.source_span.source_range.length() == 1) {
                padding_markers.push_back(&item);
            }
        }
        expect(fatal(bool(displayed == test_case.expected_display)));
        expect(fatal(bool(has_crlf_segment == test_case.has_crlf_segment)));
        expect(fatal(bool(padding_markers.size() == (test_case.has_outer_padding_markers ? 2u : 0u))));
        expect(fatal(bool(code_items.size() >= 2u)));

        std::size_t next_source_offset = 0;
        for (auto const* item : code_items) {
            expect(fatal(bool(item->source_span.container_id == model.blocks.front().id)));
            expect(fatal(bool(item->source_span.source_range.start == next_source_offset)));
            expect(fatal(bool(item->source_span.source_range.valid_for(test_case.source.size()))));
            next_source_offset = item->source_span.source_range.end;
        }
        expect(fatal(bool(next_source_offset == test_case.source.size())));
        for (auto const* marker : padding_markers) {
            expect(fatal(bool(marker->special().marker_owner == code_id)));
            expect(fatal(bool(marker->source_span.source_range.length() == marker->text.size())));
        }
        auto closing = std::find_if(opening + 1, items.end(), [&](auto const& item) {
            return item.id == code_id && item.kind == InlineRenderItem::Kind::Marker
                && item.text == U"`";
        });
        expect(fatal(bool(closing != items.end())));
        if (closing != items.end()) {
            expect(fatal(bool(opening->special().marker_owner == closing->special().marker_owner)));
            expect(fatal(bool(opening->source_span.source_range.start == 0u)));
            expect(fatal(bool(closing->source_span.source_range.end == test_case.source.size())));
        }
    }
};

"block semantic whitespace remains an editable hidden marker"_test = [] {
    auto paragraph = build_model("  aaa\n bbb\n");
    expect(fatal(bool(paragraph.blocks.size() == 1u)));
    if (paragraph.blocks.size() == 1u) {
        auto const& items = paragraph.blocks.front().inline_items;
        auto indent = std::find_if(items.begin(), items.end(), [](auto const& item) {
            return item.kind == InlineRenderItem::Kind::Marker
                && item.source_span.source_range == SourceRange{0, 2};
        });
        expect(fatal(bool(indent != items.end())));
        if (indent != items.end()) {
            expect(fatal(bool(indent->text == U"  ")));
            expect(fatal(bool(indent->special().visibility == MarkerVisibility::HiddenButEditable)));
            expect(fatal(bool(indent->source_span.container_id == paragraph.blocks.front().id)));
        }
        expect(fatal(bool(std::ranges::any_of(items, [](auto const& item) {
            return item.kind == InlineRenderItem::Kind::Text && item.text == U"aaa";
        }))));
    }

    auto heading = build_model("### foo \t\n");
    expect(fatal(bool(heading.blocks.size() == 1u)));
    if (heading.blocks.size() == 1u) {
        auto const& items = heading.blocks.front().inline_items;
        auto trailing = std::find_if(items.begin(), items.end(), [](auto const& item) {
            return item.kind == InlineRenderItem::Kind::Marker
                && item.source_span.source_range == SourceRange{3, 5};
        });
        expect(fatal(bool(trailing != items.end())));
        if (trailing != items.end()) {
            expect(fatal(bool(trailing->text == U" \t")));
            expect(fatal(bool(trailing->special().visibility == MarkerVisibility::HiddenButEditable)));
            expect(fatal(bool(trailing->source_span.container_id == heading.blocks.front().id)));
        }
        expect(fatal(bool(std::ranges::any_of(items, [](auto const& item) {
            return item.kind == InlineRenderItem::Kind::Text && item.text == U"foo";
        }))));
    }
};

};
