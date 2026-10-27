#include <string>

#include "support/folia_test.hpp"
import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/parser_test_support.hpp"

suite parser_inline_escape_tests = [] {

"escaped reference-label punctuation stays semantic and source-lossless"_test = [] {
    const std::string source =
        R"md([Foo*bar\]]:my_(url) 'title (with parens)'

[Foo*bar\]])md";
    const auto parsed = parse_text(1, source);
    const auto* paragraph = first_block(parsed.document.root.children, BlockKind::Paragraph);
    expect(fatal(bool(paragraph != nullptr)));
    if (!paragraph) return;

    const auto& document = paragraph->inline_content;
    const auto label_source = std::u32string{U"[Foo*bar\\]]"};
    expect(fatal(bool(document.source == label_source)));
    expect_lossless(document);
    expect(fatal(bool(serialize_markdown_cps(parsed.document) == utf8_to_cps(source))));

    const auto* link = first_inline(document, InlineCstKind::Link);
    expect(fatal(bool(link != nullptr)));
    if (!link) return;

    expect(fatal(bool(inline_source_slice(document, link->range) == label_source)));
    expect(fatal(bool(inline_visible_text(document) == U"Foo*bar]")));
    const auto* escaped_close = first_inline(link->children, InlineCstKind::Escape);
    expect(fatal(bool(escaped_close != nullptr)));
    if (escaped_close) {
        expect(fatal(bool(inline_source_slice(document, escaped_close->range) == U"\\]")));
    }
};

};
