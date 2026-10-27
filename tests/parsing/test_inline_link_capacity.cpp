#include <cstddef>
#include <string>
#include <ranges>
#include <optional>

#include "support/folia_test.hpp"

import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;

using namespace boost::ut;
using namespace folia;

suite inline_link_capacity_tests = [] {
    "unclosed link suffix lookahead preserves lossless text"_test = [] {
        InlineParseContext context;
        std::u32string parens;
        parens.reserve(400000);
        for (std::size_t i = 0; i < 80000; ++i) parens += U"[ (](";
        auto tree = parse_inline(parens, context);
        expect(flatten_tokens(tree, parens) == parens);
        expect(serialize_lossless(tree, parens) == parens);

        std::u32string angle;
        angle.reserve(180000);
        for (std::size_t i = 0; i < 30000; ++i) angle += U"[a](<b";
        tree = parse_inline(angle, context);
        expect(flatten_tokens(tree, angle) == angle);
        expect(serialize_lossless(tree, angle) == angle);
        expect(fatal(tree.nodes.size() == 1_u));
        expect(tree.nodes.front().kind == InlineCstKind::Incomplete);
    };

    "angle candidates stop at the first invalid character or physical line"_test = [] {
        InlineParseContext context;
        // Isolate CommonMark angle links; GFM can separately recognize the
        // bare URLs inside these unfinished angle constructs.
        context.dialect.gfm.autolinks = false;
        const std::u32string source = U"<http://a.example <https://b.example\n>";
        const auto tree = parse_inline(source, context);
        expect(flatten_tokens(tree, source) == source);
        expect(serialize_lossless(tree, source) == source);
        expect(!std::ranges::any_of(tree.nodes, [](const InlineCstNode& node) {
            return node.kind == InlineCstKind::Autolink;
        }));
    };

    "complete parenthesized titles keep nesting and escape semantics"_test = [] {
        InlineParseContext context;
        const std::u32string source = U"[x](u (a\\)b (nested)))";
        const auto tree = parse_inline(source, context);
        expect(flatten_tokens(tree, source) == source);
        expect(serialize_lossless(tree, source) == source);
        expect(fatal(tree.nodes.size() == 1_u));
        expect(tree.nodes.front().kind == InlineCstKind::Link);
        expect(tree.nodes.front().semantics().title
            == std::optional<std::string>{"a)b (nested)"});
    };

    "an incomplete image opener leaves its complete inner link available"_test = [] {
        InlineParseContext context;
        context.dialect.wiki_links = false;
        const std::u32string source = U"![[]()";
        const auto tree = parse_inline(source, context);
        expect(flatten_tokens(tree, source) == source);
        expect(serialize_lossless(tree, source) == source);
        expect(fatal(tree.nodes.size() == 2_u));
        expect(tree.nodes.front().kind == InlineCstKind::Text);
        expect(tree.nodes.front().range.start == 0_u);
        expect(tree.nodes.front().range.end == 2_u);
        const auto link = std::ranges::find_if(tree.nodes, [](const InlineCstNode& node) {
            return node.kind == InlineCstKind::Link && node.range.start == 2;
        });
        expect(fatal(link != tree.nodes.end()));
        expect(link->semantics().href.empty());
    };
};
