#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.inline_cst;
import folia.markdown.inline_parser;
import folia.markdown.inline_document;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

namespace {
std::size_t count_kind(const InlineCstNodes& nodes, InlineCstKind kind) {
    std::size_t count = 0;
    for (const auto& node : nodes) {
        if (node.kind == kind) ++count;
        count += count_kind(node.children, kind);
    }
    return count;
}

void append_visible(const InlineCstNodes& nodes, std::u32string_view source, std::u32string& out) {
    for (const auto& node : nodes) {
        if (node.kind == InlineCstKind::Strikethrough || node.kind == InlineCstKind::Incomplete) {
            append_visible(node.children, source, out);
        } else if (node.range.valid_for(source.size())) {
            out.append(source.substr(node.range.start, node.range.length()));
        }
    }
}
}

suite gfm_strikethrough_tests = [] {
    "published_491_parses_both_single_and_double_runs_with_visible_text"_test = [] {
        InlineParseContext ctx;
        ctx.dialect.gfm.strikethrough = true;
        const std::u32string source = U"~~Hi~~ Hello, ~there~ world!\n";
        const auto tree = parse_inline(source, ctx);
        expect(count_kind(tree.nodes, InlineCstKind::Strikethrough) == 2_u);
        expect(tree.nodes.size() == 5_u);
        expect(tree.nodes[0].kind == InlineCstKind::Strikethrough);
        expect(tree.nodes[0].range == SourceRange{0, 6});
        expect(tree.nodes[2].kind == InlineCstKind::Strikethrough);
        expect(tree.nodes[2].range == SourceRange{14, 21});
        expect(tree.nodes[3].kind == InlineCstKind::Text);
        expect(tree.nodes[3].range == SourceRange{21, 28});
        expect(tree.nodes[4].kind == InlineCstKind::SoftBreak);
        expect(tree.nodes[4].range == SourceRange{28, 29});
        std::u32string visible;
        append_visible(tree.nodes, source, visible);
        expect(visible == U"Hi Hello, there world!\n");
        expect(flatten_tokens(tree, source) == source);
    };

    "published_493_keeps_triple_runs_literal_and_exact_single_double_runs_work"_test = [] {
        InlineParseContext ctx;
        ctx.dialect.gfm.strikethrough = true;
        const std::u32string triple = U"This will ~~~not~~~ strike.\n";
        const auto triple_tree = parse_inline(triple, ctx);
        expect(count_kind(triple_tree.nodes, InlineCstKind::Strikethrough) == 0_u);
        expect(triple_tree.nodes.size() == 2_u);
        expect(triple_tree.nodes.back().kind == InlineCstKind::SoftBreak);
        expect(triple_tree.nodes[0].kind == InlineCstKind::Text);
        expect(flatten_tokens(triple_tree, triple) == triple);
        for (const auto& source : {U"~one~", U"~~two~~"}) {
            const auto tree = parse_inline(source, ctx);
            expect(count_kind(tree.nodes, InlineCstKind::Strikethrough) == 1_u);
            expect(flatten_tokens(tree, source) == source);
        }
    };

    "unclosed_single_and_double_openers_remain_incomplete_and_lossless"_test = [] {
        InlineParseContext ctx;
        ctx.dialect.gfm.strikethrough = true;
        for (const auto& source : {U"~unfinished", U"~~unfinished"}) {
            const auto tree = parse_inline(source, ctx);
            expect(tree.nodes.size() == 1_u);
            expect(tree.nodes.front().kind == InlineCstKind::Incomplete);
            expect(tree.nodes.front().status == ParseStatus::MissingCloser);
            expect(flatten_tokens(tree, source) == source);
        }
    };

    "strike_delimiter_search_shields_code_html_and_link_destinations"_test = [] {
        InlineParseContext ctx;
        ctx.dialect.gfm.strikethrough = true;
        for (const auto& source : {U"~~`code ~~ still code` tail~~",
                U"~~<span title='~~'>body</span>~~", U"~~[label](url/~~) tail~~"}) {
            const auto tree = parse_inline(source, ctx);
            expect(count_kind(tree.nodes, InlineCstKind::Strikethrough) == 1_u);
            expect(flatten_tokens(tree, source) == source);
        }
    };

    "nested_inline_delimiters_and_escaped_tildes_keep_their_owners"_test = [] {
        InlineParseContext ctx;
        ctx.dialect.gfm.strikethrough = true;
        const std::u32string nested = U"~~[~label~](url)~~";
        const auto nested_tree = parse_inline(nested, ctx);
        expect(count_kind(nested_tree.nodes, InlineCstKind::Strikethrough) == 2_u);
        expect(count_kind(nested_tree.nodes, InlineCstKind::Link) == 1_u);
        expect(flatten_tokens(nested_tree, nested) == nested);
        const std::u32string escaped = U"~~literal \\~ inside~~";
        const auto escaped_tree = parse_inline(escaped, ctx);
        expect(count_kind(escaped_tree.nodes, InlineCstKind::Strikethrough) == 1_u);
        expect(flatten_tokens(escaped_tree, escaped) == escaped);
    };

    "per_character_source_edits_and_inverse_restore_losslessly"_test = [] {
        InlineParseContext ctx;
        ctx.dialect.gfm.strikethrough = true;
        std::u32string source;
        for (const auto ch : std::u32string_view{U"~word~"}) {
            TextEdit insert{NodeId{7}, SourceRange{source.size(), source.size()}, std::u32string(1, ch)};
            apply_text_edit(source, insert);
            const auto parsed = parse_inline(source, ctx);
            expect(flatten_tokens(parsed, source) == source);
            TextEdit inverse{NodeId{7}, SourceRange{source.size() - 1, source.size()}, {}};
            apply_text_edit(source, inverse);
            apply_text_edit(source, insert);
            expect(source.back() == ch);
            expect(flatten_tokens(parse_inline(source, ctx), source) == source);
        }
        expect(count_kind(parse_inline(source, ctx).nodes, InlineCstKind::Strikethrough) == 1_u);
    };
};

