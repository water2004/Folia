#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"

import folia.markdown.ast;
import folia.markdown.dialect;
import folia.markdown.inline_document;
import folia.markdown.inline_cst;
import folia.markdown.text_edit;
import folia.markdown.parser;
import folia.markdown.block_source;
import folia.markdown.utf;
import folia.markdown.html_entities;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.core.theme;

#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

suite nul_display_projection_tests = [] {

"embedded_nul_is_replaced_only_in_derived_text_and_attributes"_test = [] {
    const std::string source("abc\0de\0", 7);
    const std::u32string expected = U"abc\uFFFDde\uFFFD";
    const auto parsed = parse_text(1, source);
    expect(fatal(parsed.document.root.children.size() == 1));
    if (parsed.document.root.children.size() != 1) return;
    const auto& inline_document = parsed.document.root.children.front().inline_content;
    expect(fatal(inline_document.source == std::u32string({U'a', U'b', U'c', U'\0', U'd', U'e', U'\0'})));
    expect(fatal(inline_visible_text(inline_document) == expected));
    expect(fatal(serialize_markdown(parsed.document) == source));

    const auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(projection.projected_html.find('\0') == std::string::npos));
    expect(fatal(projection.projected_html.find("abc\xef\xbf\xbd" "de\xef\xbf\xbd") != std::string::npos));

    const auto model = build_render_model(
        parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
    expect(fatal(model.blocks.size() == 1));
    if (model.blocks.size() == 1) {
        std::u32string rendered;
        std::size_t source_units = 0;
        for (const auto& item : model.blocks.front().inline_items) {
            if (item.kind != InlineRenderItem::Kind::Text) continue;
            expect(fatal(item.text.find(U'\0') == std::u32string::npos));
            expect(fatal(item.text.size() == item.source_span.source_range.length()));
            source_units += item.source_span.source_range.length();
            rendered += item.text;
        }
        expect(fatal(rendered == expected));
        expect(fatal(source_units == source.size()));
    }

    const std::string attribute_value("a\0b", 3);
    auto image = folia::test::commonmark::projection_detail::element(
        "img", {{"alt", attribute_value}, {"src", "image.png"}});
    const auto attribute_html = folia::test::commonmark::projection_detail::render_html(image);
    expect(fatal(attribute_html.find('\0') == std::string::npos));
    expect(fatal(attribute_html.find("alt=\"a\xef\xbf\xbd" "b\"") != std::string::npos));

    auto raw = folia::test::commonmark::projection_detail::raw_html_node(std::string("x\0y", 3));
    const auto raw_html = folia::test::commonmark::projection_detail::render_html(raw);
    expect(fatal(raw_html == "x\xef\xbf\xbdy"));
};

"embedded_nul_inside_code_span_uses_the_same_display_mapping"_test = [] {
    const std::string source("`a\0b`", 5);
    const auto parsed = parse_text(2, source);
    expect(fatal(parsed.document.root.children.size() == 1));
    if (parsed.document.root.children.size() != 1) return;
    const auto& inline_document = parsed.document.root.children.front().inline_content;
    expect(fatal(inline_document.source == std::u32string({U'`', U'a', U'\0', U'b', U'`'})));
    expect(fatal(inline_visible_text(inline_document) == U"a\uFFFDb"));
    expect(fatal(serialize_markdown(parsed.document) == source));
    const auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(projection.projected_html.find("<code>a\xef\xbf\xbd" "b</code>") != std::string::npos));
    expect(fatal(projection.projected_html.find('\0') == std::string::npos));
    const auto model = build_render_model(
        parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
    expect(fatal(model.blocks.size() == 1));
    if (model.blocks.size() == 1) {
        bool found = false;
        for (const auto& item : model.blocks.front().inline_items) {
            if (item.kind != InlineRenderItem::Kind::Text) continue;
            found = true;
            expect(fatal(item.text == U"a\uFFFDb"));
            expect(fatal(item.source_span.source_range == SourceRange{1, 4}));
        }
        expect(fatal(found));
    }
};

"embedded_nul_in_block_code_is_replaced_in_render_text_only"_test = [] {
    const std::string source("    a\0b\n", 8);
    const auto parsed = parse_text(3, source);
    expect(fatal(parsed.document.root.children.size() == 1));
    if (parsed.document.root.children.size() != 1) return;
    expect(fatal(parsed.document.root.children.front().kind == BlockKind::CodeBlock));
    expect(fatal(serialize_markdown(parsed.document) == source));
    const auto model = build_render_model(
        parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
    expect(fatal(model.blocks.size() == 1));
    if (model.blocks.size() == 1) {
        const auto& special = model.blocks.front().special();
        expect(special.code_text == U"a\uFFFDb") << "derived indented code: " << cps_to_utf8(special.code_text);
        expect(special.raw_source.find(U'\0') != std::u32string::npos);
        expect(special.content_to_source.size() == special.code_text.size() + 1);
    }
};

};
