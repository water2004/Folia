#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.serializer;
import folia.markdown.utf;

import folia.markdown.markdown_html;
import folia.markdown.dialect;
#include "support/commonmark_projection.hpp"
using namespace folia;
using namespace boost::ut;

#include "support/parser_test_support.hpp"

suite commonmark_html_projection_tests = [] {

"official_image_html_uses_existing_cst_semantics"_test = [] {
    const std::string source = "*<img src=\"foo\" title=\"*\"/>\n"; // CommonMark #475
    const auto parsed = parse_text(1, source);
    const auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(bool(folia::test::commonmark::inline_lossless(parsed.document.root))));
    expect(fatal(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source))));
    expect(fatal(bool(projection.projected_html
        == "<p>*<img src=\"foo\" title=\"*\"/></p>\n")));
    expect(fatal(bool(projection.limitations.empty())));
};

"official_comment_html_uses_delimiter_content_range"_test = [] {
    const std::string source = "foo <!-- this is a --\ncomment - with hyphens -->\n"; // CommonMark #625
    const auto parsed = parse_text(1, source);
    const auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(bool(folia::test::commonmark::inline_lossless(parsed.document.root))));
    expect(fatal(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source))));
    expect(fatal(bool(projection.projected_html
        == "<p>foo <!-- this is a --\ncomment - with hyphens --></p>\n")));
    expect(fatal(bool(projection.limitations.empty())));
};

"known_void_and_container_tags_project_attributes_and_html_text"_test = [] {
    const std::string source = "<span>*word*</span><br/><img src=\"x\" alt=\"*\">\n";
    const auto parsed = parse_text(1, source);
    const auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(bool(projection.projected_html
        == "<p><span><em>word</em></span><br/><img src=\"x\" alt=\"*\"></p>\n")));
};

"html_attribute_entity_spelling_reaches_the_shared_html_canonicalizer"_test = [] {
    const std::string source = "<a href=\"&amp;\">x</a>\n";
    const auto parsed = parse_text(1, source);
    const auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(bool(projection.projected_html == "<p><a href=\"&amp;\">x</a></p>\n")));
    expect(fatal(bool(projection.limitations.empty())));
};

"unknown_html_tokens_and_incomplete_html_openings_emit_raw_markup"_test = [] {
    const auto unknown = parse_text(1, "<widget>x</widget>\n");
    expect(fatal(bool(unknown.document.root.children.size() == 1u)));
    if (unknown.document.root.children.empty()) return;
    expect(fatal(bool(first_inline(
        unknown.document.root.children.front().inline_content,
        InlineCstKind::Raw) != nullptr)));
    const auto unknown_projection = folia::test::commonmark::project(unknown.document.root);
    expect(fatal(bool(unknown_projection.projected_html == "<p><widget>x</widget></p>\n")));

    const auto escaped = parse_text(1, "&lt;widget&gt;x&lt;/widget&gt;\n");
    const auto escaped_projection = folia::test::commonmark::project(escaped.document.root);
    expect(fatal(bool(escaped_projection.projected_html
        == "<p>&lt;widget&gt;x&lt;/widget&gt;</p>\n")));

    const auto incomplete = parse_text(1, "<span>unfinished\n");
    expect(fatal(bool(incomplete.document.root.children.size() == 1u)));
    if (incomplete.document.root.children.empty()) return;
    const auto* incomplete_node = first_inline(
        incomplete.document.root.children.front().inline_content,
        InlineCstKind::Incomplete);
    expect(fatal(bool(incomplete_node != nullptr)));
    if (incomplete_node) {
        expect(fatal(bool(incomplete_node->status == ParseStatus::MissingCloser)));
    }
    expect(fatal(bool(folia::test::commonmark::inline_lossless(incomplete.document.root))));
    const auto incomplete_projection = folia::test::commonmark::project(incomplete.document.root);
    expect(fatal(bool(incomplete_projection.projected_html == "<p><span>unfinished</p>\n")));
};

};
