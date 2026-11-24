#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.core.inline_source_edit;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "parsing/support/inline_cst.hpp"

suite parsing_inline_cst_whitespace_tests = [] {

"block whitespace semantic ranges preserve raw paragraph source"_test = [] {
    for (std::size_t spaces = 1; spaces <= 3; ++spaces) {
        std::u32string source(spaces, U' ');
        source += U"alpha\n             beta\n";
        const auto tree = parse_inline(source, test_context());
        InlineDocument document{source, tree};
        const auto hidden = inline_block_semantic_whitespace_ranges(
            document,
            InlineBlockWhitespacePolicy::Paragraph);
        expect(hidden.size() == 1u);
        if (!hidden.empty()) expect(hidden.front() == SourceRange{0, spaces});
        expect(flatten_tokens(document.tree, document.source) == document.source);
        expect(serialize_lossless(document.tree, document.source) == document.source);
        const auto* soft_break = first_node(document.tree, InlineCstKind::SoftBreak);
        expect(soft_break != nullptr);
        if (soft_break) {
            expect(document.source.substr(soft_break->range.start, soft_break->range.length())
                == U"\n             ");
        }
    }

    const std::u32string four_spaces = U"    alpha";
    InlineDocument code_like{four_spaces, parse_inline(four_spaces, test_context())};
    expect(inline_block_semantic_whitespace_ranges(
        code_like, InlineBlockWhitespacePolicy::Paragraph).empty());

    const std::u32string inside_markup = U"* alpha *";
    InlineDocument emphasis{inside_markup, parse_inline(inside_markup, test_context())};
    expect(inline_block_semantic_whitespace_ranges(
        emphasis, InlineBlockWhitespacePolicy::Paragraph).empty());
};

"heading semantic trim is confined to terminal raw whitespace"_test = [] {
    const std::u32string source = U"alpha \t";
    InlineDocument heading{source, parse_inline(source, test_context())};
    const auto hidden = inline_block_semantic_whitespace_ranges(
        heading,
        InlineBlockWhitespacePolicy::Heading);
    expect(hidden.size() == 1u);
    if (!hidden.empty()) {
        expect(hidden.front() == SourceRange{5, 7});
        expect(inline_source_slice(heading, hidden.front()) == U" \t");
    }
    expect(flatten_tokens(heading.tree, heading.source) == heading.source);

    // The current tokenizer keeps `foo \t` in one Text leaf. Also exercise
    // the semantic helper with adjacent raw leaves so that a future CST split
    // between the space and tab remains fully covered.
    InlineDocument split_heading = heading;
    if (!split_heading.tree.nodes.empty()) {
        auto trailing_leaf = split_heading.tree.nodes.back();
        trailing_leaf.id = NodeId{2001};
        trailing_leaf.range = {6, 7};
        split_heading.tree.nodes.back().range.end = 6;
        split_heading.tree.nodes.push_back(std::move(trailing_leaf));
        expect(roots_partition_source(split_heading.tree, split_heading.source.size()));
        const auto split_hidden = inline_block_semantic_whitespace_ranges(
            split_heading,
            InlineBlockWhitespacePolicy::Heading);
        expect(split_hidden.size() == 2u);
        if (split_hidden.size() == 2u) {
            expect(split_hidden[0] == SourceRange{5, 6});
            expect(split_hidden[1] == SourceRange{6, 7});
        }
    }

    const std::u32string code = U"`alpha `";
    InlineDocument code_heading{code, parse_inline(code, test_context())};
    expect(inline_block_semantic_whitespace_ranges(
        code_heading, InlineBlockWhitespacePolicy::Heading).empty());

    const std::u32string entity = U"&amp;";
    InlineDocument entity_heading{entity, parse_inline(entity, test_context())};
    expect(inline_block_semantic_whitespace_ranges(
        entity_heading, InlineBlockWhitespacePolicy::Heading).empty());

    const std::u32string escaped = U"\\* ";
    InlineDocument escaped_heading{escaped, parse_inline(escaped, test_context())};
    const auto escaped_hidden = inline_block_semantic_whitespace_ranges(
        escaped_heading,
        InlineBlockWhitespacePolicy::Heading);
    expect(escaped_hidden.size() == 1u);
    if (!escaped_hidden.empty()) expect(escaped_hidden.front() == SourceRange{2, 3});

    const std::u32string emphasis_and_space = U"*alpha* ";
    InlineDocument emphasized_heading{
        emphasis_and_space,
        parse_inline(emphasis_and_space, test_context())};
    const auto emphasis_hidden = inline_block_semantic_whitespace_ranges(
        emphasized_heading,
        InlineBlockWhitespacePolicy::Heading);
    expect(emphasis_hidden.size() == 1u);
    if (!emphasis_hidden.empty()) {
        expect(inline_source_slice(emphasized_heading, emphasis_hidden.front()) == U" ");
    }

    const std::u32string html_source = U"alpha \t";
    auto html_context = test_context();
    html_context.syntax_mode = InlineSyntaxMode::HtmlText;
    InlineDocument html_text{html_source, parse_inline(html_source, html_context), InlineSyntaxMode::HtmlText};
    expect(inline_block_semantic_whitespace_ranges(
        html_text, InlineBlockWhitespacePolicy::Heading).empty());
};

"hard_breaks_are_single_visual_delete_units"_test = [] {
    InlineDocument html{U"a<br>b", {}};
    reparse_inline_document(html, test_context());
    expect(inline_backward_delete_range(html, 5) == SourceRange{1, 5});
    expect(inline_backward_delete_range(html, 3) == SourceRange{1, 5});
    expect(inline_forward_delete_range(html, 1) == SourceRange{1, 5});
    expect(inline_forward_delete_range(html, 3) == SourceRange{1, 5});

    InlineDocument markdown{U"a  \nb", {}};
    reparse_inline_document(markdown, test_context());
    expect(inline_backward_delete_range(markdown, 4) == SourceRange{1, 4});
    expect(inline_forward_delete_range(markdown, 1) == SourceRange{1, 4});
};

"physical line endings_are_lossless_single_break_nodes"_test = [] {
    const std::array<std::u32string, 3> sources{U"a\r\nb", U"a\rb", U"a\nb"};
    for (auto const& source : sources) {
        const auto tree = parse_inline(source, test_context());
        expect(fatal(bool(flatten_tokens(tree, source) == source)));
        expect(fatal(bool(serialize_lossless(tree, source) == source)));
        const auto* soft_break = first_node(tree, InlineCstKind::SoftBreak);
        expect(fatal(soft_break != nullptr));
        expect(fatal(bool(soft_break->range.start == 1u)));
        expect(fatal(bool(soft_break->range.end == source.size() - 1u)));
    }

    InlineDocument markdown{U"a  \r\nb", {}};
    reparse_inline_document(markdown, test_context());
    const auto* hard_break = first_node(markdown.tree, InlineCstKind::HardBreak);
    expect(fatal(hard_break != nullptr));
    expect(fatal(bool(hard_break->range == SourceRange{1, 5})));
    expect(fatal(bool(inline_backward_delete_range(markdown, 5) == SourceRange{1, 5})));
    expect(fatal(bool(inline_forward_delete_range(markdown, 1) == SourceRange{1, 5})));
};

};
