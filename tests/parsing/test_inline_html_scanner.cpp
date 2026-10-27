#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.dialect;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.core.inline_source_edit;
import folia.markdown.markdown_html;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/parser_test_support.hpp"

suite inline_html_scanner_tests = [] {

"markdown_html_scanner_accepts_html_attribute_ranges"_test = [] {
    const auto source = std::u32string(U"<SPAN DATA:mode.value='first\nsecond' disabled/>");
    const auto scanned = scan_markdown_html_tag(source, 0, source.size());
    expect(fatal(bool(scanned.has_value())));
    if (!scanned) return;
    expect(fatal(bool(scanned->range == SourceRange{0, source.size()})));
    expect(fatal(bool(scanned->name == "span")));
    expect(fatal(bool(!scanned->closing && scanned->self_closing)));
    expect(fatal(bool(scanned->attributes.size() == 2u)));
    expect(fatal(bool(scanned->attributes[0].name == SourceRange{6, 21})));
    expect(fatal(bool(scanned->attributes[0].value.has_value())));
    if (scanned->attributes[0].value) {
        expect(fatal(bool(*scanned->attributes[0].value == SourceRange{23, 35})));
    }
    expect(fatal(bool(!scanned->attributes[1].value.has_value())));

    const std::string markup = "<span data:id.value=ok>text</span>";
    const auto parsed = parse_text(1, markup);
    expect(fatal(bool(parsed.document.root.children.size() == 1u)));
    if (parsed.document.root.children.empty()) return;
    const auto& document = parsed.document.root.children.front().inline_content;
    expect_lossless(document);
    const auto element = std::find_if(document.tree.nodes.begin(), document.tree.nodes.end(), [](const auto& node) {
        return node.kind == InlineCstKind::HtmlElement;
    });
    expect(fatal(bool(element != document.tree.nodes.end())));
    if (element != document.tree.nodes.end()) {
        expect(fatal(bool(element->semantics().html_attributes.contains("data:id.value"))));
    }
};

"valid_unknown_html_tag_is_opaque_to_reference_bracket_matching"_test = [] {
    // Official CommonMark example 536: the brackets in a valid quoted tag
    // attribute must not become a reference link.
    const std::string source = "[foo <bar attr=\"][ref]\">\n\n[ref]: /uri\n";
    const auto parsed = parse_text(1, source);
    expect(parsed.document.root.children.size() == 2u)
        << "reference fixture root count=" << parsed.document.root.children.size();
    expect(parsed.document.root.children.back().kind == BlockKind::LinkDefinition)
        << "last block kind=" << static_cast<int>(parsed.document.root.children.back().kind);
    if (parsed.document.root.children.empty()) return;
    const auto& document = parsed.document.root.children.front().inline_content;
    expect_lossless(document);
    expect(fatal(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source))));
    const bool has_link = std::any_of(document.tree.nodes.begin(), document.tree.nodes.end(), [](const auto& node) {
        return node.kind == InlineCstKind::Link;
    });
    expect(fatal(bool(!has_link)));
};

"invalid_attribute_syntax_does_not_hide_link_bracket"_test = [] {
    const std::u32string invalid = U"<span @ ]>";
    expect(fatal(bool(!scan_markdown_html_tag(invalid, 0, invalid.size()).has_value())));

    const std::string source = "[label <span @ ](https://example.test)";
    const auto parsed = parse_text(1, source);
    expect(fatal(bool(parsed.document.root.children.size() == 1u)));
    if (parsed.document.root.children.empty()) return;
    const auto& document = parsed.document.root.children.front().inline_content;
    expect_lossless(document);
    const bool has_link = std::any_of(document.tree.nodes.begin(), document.tree.nodes.end(), [](const auto& node) {
        return node.kind == InlineCstKind::Link;
    });
    expect(fatal(bool(has_link)));
};

"complete_raw_tag_attributes_do_not_take_part_in_markdown_delimiters"_test = [] {
    const auto source = std::u32string{
        U"<widget data='[x](dest) **'> **outside** [ok](dest)"};
    InlineParseContext context;
    context.dialect = default_dialect();
    context.next_id = NodeId{10};
    InlineDocument document{source, parse_inline(source, context)};
    expect_lossless(document);
    expect(fatal(!document.tree.nodes.empty()));
    expect(document.tree.nodes.front().kind == InlineCstKind::Raw);
    expect(inline_source_slice(document, document.tree.nodes.front().range)
        == U"<widget data='[x](dest) **'>");
    expect(std::ranges::count_if(document.tree.nodes, [](const auto& node) {
        return node.kind == InlineCstKind::Link;
    }) == 1);
    expect(std::ranges::count_if(document.tree.nodes, [](const auto& node) {
        return node.kind == InlineCstKind::Strong;
    }) == 1);
};

"incomplete_safe_html_remains_lossless_editable_cst"_test = [] {
    const std::string source = "<span data:id.value=ok>unfinished";
    const auto parsed = parse_text(1, source);
    expect(fatal(bool(parsed.document.root.children.size() == 1u)));
    if (parsed.document.root.children.empty()) return;
    const auto& document = parsed.document.root.children.front().inline_content;
    expect_lossless(document);
    expect(fatal(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source))));
    const auto found = std::find_if(document.tree.nodes.begin(), document.tree.nodes.end(), [](const auto& node) {
        return node.kind == InlineCstKind::Incomplete && node.status == ParseStatus::MissingCloser;
    });
    expect(fatal(bool(found != document.tree.nodes.end())));

    InlineDocument edited{utf8_to_cps(source), {}};
    InlineParseContext context;
    context.dialect = default_dialect();
    context.next_id = NodeId{10};
    reparse_inline_document(edited, context);
    expect(fatal(bool(first_inline(edited, InlineCstKind::Incomplete) != nullptr)));
    const auto close = edited.source.size();
    apply_inline_source_edit(
        NodeId{7}, edited,
        TextEdit{NodeId{7}, {close, close}, U"</span>"},
        context);
    expect(fatal(bool(edited.source == utf8_to_cps(source + "</span>"))));
    expect(fatal(bool(first_inline(edited, InlineCstKind::Incomplete) == nullptr)));
    expect(fatal(bool(first_inline(edited, InlineCstKind::HtmlElement) != nullptr)));
    expect(fatal(bool(flatten_tokens(edited.tree, edited.source) == edited.source)));
};

};
