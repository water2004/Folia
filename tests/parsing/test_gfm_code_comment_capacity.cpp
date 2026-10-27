#include <string>
#include "support/folia_test.hpp"

import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.markdown_html;

using namespace boost::ut;
using namespace folia;

suite gfm_code_comment_capacity = [] {
    "increasing unmatched code runs keep separate source spans"_test = [] {
        InlineParseContext context;
        std::u32string source;
        constexpr std::size_t run_count = 256;
        for (std::size_t length = 1; length <= run_count; ++length) {
            source.push_back(U'e');
            source.append(length, U'`');
        }
        const auto tree = parse_inline(source, context);
        expect(flatten_tokens(tree, source) == source);
        expect(serialize_lossless(tree, source) == source);
        std::size_t incomplete = 0;
        for (const auto& node : tree.nodes) {
            if (node.kind == InlineCstKind::Incomplete) {
                ++incomplete;
                expect(node.status == ParseStatus::MissingCloser);
            }
        }
        expect(incomplete == run_count);
    };

    "unclosed comments remain visible and complete comments keep ranges"_test = [] {
        InlineParseContext context;
        std::u32string unclosed = U"</";
        for (std::size_t count = 0; count < 2048; ++count) unclosed += U"<!--";
        const auto plain = parse_inline(unclosed, context);
        expect(flatten_tokens(plain, unclosed) == unclosed);
        expect(serialize_lossless(plain, unclosed) == unclosed);
        expect(plain.nodes.size() == 1_u);
        expect(plain.nodes.front().kind == InlineCstKind::Text);

        const std::u32string complete = U"before <!--body--> after";
        const auto parsed = parse_inline(complete, context);
        expect(flatten_tokens(parsed, complete) == complete);
        expect(serialize_lossless(parsed, complete) == complete);
        expect(parsed.nodes.size() == 3_u);
        if (parsed.nodes.size() == 3) {
            expect(parsed.nodes[1].kind == InlineCstKind::HtmlComment);
            expect(parsed.nodes[1].range.start == 7_u);
            expect(parsed.nodes[1].range.end == 18_u);
        }
    };

    "special terminator lookup stays inside the active parse limit"_test = [] {
        const std::u32string source = U"<!--x-->";
        const MarkdownHtmlSpecialIndex index{source};
        expect(!scan_markdown_html_special(source, 0, source.size() - 1, &index));
        const auto complete = scan_markdown_html_special(source, 0, source.size(), &index);
        expect(complete.has_value());
        if (complete) expect(complete->range.end == source.size());
    };

    "escaped first tick stays opaque for link and emphasis scanning"_test = [] {
        InlineParseContext context;
        const std::u32string source = U"[a\\``x]y`](dest) *ok*";
        const auto tree = parse_inline(source, context);
        expect(flatten_tokens(tree, source) == source);
        expect(serialize_lossless(tree, source) == source);
        expect(std::ranges::any_of(tree.nodes, [](const InlineCstNode& node) {
            return node.kind == InlineCstKind::Link;
        }));
        expect(std::ranges::any_of(tree.nodes, [](const InlineCstNode& node) {
            return node.kind == InlineCstKind::Emphasis;
        }));
    };

    "code run matching clips both opener and final candidate to scope"_test = [] {
        InlineParseContext context;
        const std::u32string source = U"\\``x`";
        inline_parser_detail::Parser escaped{source, context};
        expect(escaped.matching_code_span_end(2, source.size()) == source.size());

        const std::u32string clipped = U"a`x``";
        inline_parser_detail::Parser bounded{clipped, context};
        const auto limit = clipped.size() - 1;
        bounded.limit = limit;
        expect(bounded.matching_code_span_end(1, limit) == limit);
    };
};
