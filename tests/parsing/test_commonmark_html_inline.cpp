#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.markdown_html;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {
bool contains(const InlineCstNodes& nodes, InlineCstKind kind) {
    return std::ranges::any_of(nodes, [kind](const auto& node) {
        return node.kind == kind || contains(node.children, kind);
    });
}
}

suite commonmark_html_inline_tests = [] {
    "complete_html_special_constructs_have_exact_bounded_ranges"_test = [] {
        for (const std::u32string source : {
                U"<!-->", U"<!--->", U"<!-- body -->", U"<?php echo $a; ?>",
                U"<!ELEMENT br EMPTY>", U"<![CDATA[>&<]]>"}) {
            const auto scanned = scan_markdown_html_special(source, 0, source.size());
            expect(fatal(scanned.has_value()));
            if (!scanned) continue;
            expect(scanned->range == SourceRange{0, source.size()});
            expect(scanned->opening.end == scanned->content.start);
            expect(scanned->content.end == scanned->closing.start);
            expect(scanned->closing.end == source.size());
            expect(!scan_markdown_html_special(source, 0, source.size() - 1));
        }
        for (const std::u32string source : {U"<!-- unfinished", U"<?unfinished>",
                U"<!1 invalid>", U"<![cdata[data]]>"}) {
            expect(!scan_markdown_html_special(source, 0, source.size()));
        }
    };

    "markdown_inside_inline_html_is_parsed_without_changing_raw_html_block_mode"_test = [] {
        const std::u32string source = U"<span>*word*<!-- hidden --></span>";
        InlineParseContext context;
        const auto markdown = parse_inline(source, context);
        expect(contains(markdown.nodes, InlineCstKind::Emphasis));
        expect(contains(markdown.nodes, InlineCstKind::HtmlComment));
        expect(flatten_tokens(markdown, source) == source);
        context.syntax_mode = InlineSyntaxMode::HtmlText;
        const auto html = parse_inline(source, context);
        expect(!contains(html.nodes, InlineCstKind::Emphasis));
        expect(contains(html.nodes, InlineCstKind::HtmlComment));
        expect(flatten_tokens(html, source) == source);
    };

    "complete_unclosed_html_tag_retains_nested_markdown_and_opaque_attributes"_test = [] {
        const std::u32string source = U"<a title='**[label](url)'>*word*";
        const auto tree = parse_inline(source, InlineParseContext{});
        expect(fatal(!tree.nodes.empty()));
        const auto& node = tree.nodes.front();
        expect(node.kind == InlineCstKind::Incomplete);
        expect(node.status == ParseStatus::MissingCloser);
        expect(node.semantics().html_tag == "a");
        expect(contains(node.children, InlineCstKind::Emphasis));
        expect(!contains(tree.nodes, InlineCstKind::Link));
        expect(!contains(tree.nodes, InlineCstKind::Strong));
        expect(flatten_tokens(tree, source) == source);
    };
};
