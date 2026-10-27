#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string>

#include "support/folia_test.hpp"

import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;

using namespace boost::ut;
using namespace folia;

suite pathological_bracket_tests = [] {
    "65000 unmatched link openers remain bounded and lossless"_test = [] {
        InlineParseContext context;
        std::u32string source(65000, U'[');
        const auto tree = parse_inline(source, context);
        expect(tree.nodes.size() == 1_u);
        expect(tree.nodes.front().kind == InlineCstKind::Incomplete);
        expect(flatten_tokens(tree, source) == source);
        expect(serialize_lossless(tree, source) == source);
    };

    "large nested link labels bound reference lookup and stay lossless"_test = [] {
        InlineParseContext context;
        std::size_t maximum_lookup_size = 0;
        context.resolve_link_label = [&](const std::string& label) -> std::optional<InlineLinkDef> {
            maximum_lookup_size = (std::max)(maximum_lookup_size, label.size());
            return std::nullopt;
        };
        std::u32string source;
        source.reserve(50001);
        source.append(25000, U'[');
        source.push_back(U'x');
        source.append(25000, U']');
        const auto tree = parse_inline(source, context);
        expect(maximum_lookup_size <= 999_u);
        expect(flatten_tokens(tree, source) == source);
        expect(serialize_lossless(tree, source) == source);
    };
    "nested link lookahead preserves link priority and skips opaque brackets"_test = [] {
        InlineParseContext context;
        const std::u32string source = U"[outer [inner](dest)](";
        const auto tree = parse_inline(source, context);
        expect(flatten_tokens(tree, source) == source);
        expect(serialize_lossless(tree, source) == source);
        expect(std::ranges::any_of(tree.nodes, [](const InlineCstNode& node) {
            return node.kind == InlineCstKind::Link;
        }));

        const std::u32string opaque = U"[x `]` y](dest)";
        const auto opaque_tree = parse_inline(opaque, context);
        expect(flatten_tokens(opaque_tree, opaque) == opaque);
        expect(serialize_lossless(opaque_tree, opaque) == opaque);
        expect(opaque_tree.nodes.size() == 1_u);
        expect(opaque_tree.nodes.front().kind == InlineCstKind::Link);
    };

    "reference link lookahead remains read-only and scope bounded"_test = [] {
        InlineParseContext context;
        context.resolve_link_label = [](const std::string& label) -> std::optional<InlineLinkDef> {
            if (label == "ref") return InlineLinkDef{"https://example.test/", std::nullopt};
            return std::nullopt;
        };
        const std::u32string source = U"[outer [inner][ref]](";
        const auto tree = parse_inline(source, context);
        expect(flatten_tokens(tree, source) == source);
        expect(serialize_lossless(tree, source) == source);
        expect(std::ranges::any_of(tree.nodes, [](const InlineCstNode& node) {
            return node.kind == InlineCstKind::Link;
        }));
    };

};
