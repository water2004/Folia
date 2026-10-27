#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>
#include "support/folia_test.hpp"
import folia.markdown.dialect;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
using namespace folia;
using namespace boost::ut;

namespace {
InlineParseContext emphasis_scaling_context() {
    InlineParseContext context;
    context.dialect = default_dialect();
    context.next_id = NodeId{1000};
    return context;
}
void check_emphasis_source(std::u32string const& source, InlineCstTree const& tree) {
    expect(tokens_partition_source(tree, source.size()));
    expect(roots_partition_source(tree, source.size()));
    expect(flatten_tokens(tree, source) == source);
    expect(serialize_lossless(tree, source) == source);
}
}

suite emphasis_scaling = [] {
    "unmatched_emphasis_closers_keep_every_marker_source"_test = [] {
        std::u32string source;
        for (int i=0; i<65000; ++i) source += U"a_ ";
        const auto tree = parse_inline(source, emphasis_scaling_context());
        check_emphasis_source(source, tree);
        std::size_t incomplete = 0;
        for (auto const& node : tree.nodes) {
            if (node.kind != InlineCstKind::Incomplete) continue;
            ++incomplete;
            expect(node.status == ParseStatus::MissingCloser);
            expect(source.substr(node.range.start, node.range.end-node.range.start) == U"_");
        }
        expect(incomplete == 65000u);
    };
    "adjacent_emphasis_preserves_pairs_and_distinct_node_ids"_test = [] {
        std::u32string source;
        for (int i=0; i<8192; ++i) source += U"*a* ";
        const auto tree = parse_inline(source, emphasis_scaling_context());
        check_emphasis_source(source, tree);
        std::unordered_set<std::uint64_t> ids;
        std::size_t paired = 0;
        std::vector<InlineCstNode const*> pending;
        for (auto const& node : tree.nodes) pending.push_back(&node);
        while (!pending.empty()) {
            const auto* node = pending.back(); pending.pop_back();
            expect(ids.insert(node->id.v).second);
            if (node->kind == InlineCstKind::Emphasis) {
                ++paired;
                expect(source.substr(node->range.start, node->range.end-node->range.start) == U"*a*");
            }
            for (auto const& child : node->children) pending.push_back(&child);
        }
        expect(paired == 8192u);
    };
    "rule_of_three_rejections_preserve_unclosed_editing_source"_test = [] {
        std::u32string source = U"a**b";
        for (int i=0; i<4096; ++i) source += U"c* ";
        const auto tree = parse_inline(source, emphasis_scaling_context());
        check_emphasis_source(source, tree);
        std::vector<InlineCstNode const*> pending;
        for (auto const& node : tree.nodes) pending.push_back(&node);
        while (!pending.empty()) {
            const auto* node = pending.back(); pending.pop_back();
            expect(node->kind != InlineCstKind::Strong && node->kind != InlineCstKind::Emphasis);
            for (auto const& child : node->children) pending.push_back(&child);
        }
    };
    "unclosed_opening_runs_are_independent_editable_constructs"_test = [] {
        const std::u32string source = U"*abc _def";
        for (std::size_t end=0; end<=source.size(); ++end) {
            const auto prefix = source.substr(0, end);
            const auto tree = parse_inline(prefix, emphasis_scaling_context());
            check_emphasis_source(prefix, tree);
        }
        const auto tree = parse_inline(source, emphasis_scaling_context());
        expect(tree.nodes.size() == 2u);
        if (tree.nodes.size() != 2) return;
        expect(tree.nodes[0].kind == InlineCstKind::Incomplete);
        expect(tree.nodes[1].kind == InlineCstKind::Incomplete);
        expect(tree.nodes[0].status == ParseStatus::MissingCloser);
        expect(tree.nodes[1].status == ParseStatus::MissingCloser);
        expect(tree.nodes[0].range.start == 0u && tree.nodes[0].range.end == 5u);
        expect(tree.nodes[1].range.start == 5u && tree.nodes[1].range.end == 9u);
        expect(tree.nodes[0].delimiter_ranges().content.start == 1u);
        expect(tree.nodes[0].delimiter_ranges().content.end == 5u);
        const std::u32string ordinary = U"*abc";
        const auto single = parse_inline(ordinary, emphasis_scaling_context());
        check_emphasis_source(ordinary, single);
        expect(single.nodes.size() == 1u);
        if (!single.nodes.empty()) {
            expect(single.nodes.front().kind == InlineCstKind::Incomplete);
            expect(single.nodes.front().range.end == ordinary.size());
            expect(single.nodes.front().delimiter_ranges().content.start == 1u);
        }
    };
    "unmatched_emphasis_openers_keep_sibling_error_nodes"_test = [] {
        std::u32string source;
        for (int i=0; i<65000; ++i) source += U"_a ";
        const auto tree = parse_inline(source, emphasis_scaling_context());
        check_emphasis_source(source, tree);
        expect(tree.nodes.size() == 65000u);
        for (auto const& node : tree.nodes) {
            expect(node.kind == InlineCstKind::Incomplete);
            expect(node.status == ParseStatus::MissingCloser);
            expect(node.range.end - node.range.start == 3u);
            expect(node.delimiter_ranges().content.start == node.range.start + 1u);
        }
    };

};
