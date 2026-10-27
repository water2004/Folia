#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.dialect;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

InlineParseContext context() {
    InlineParseContext value;
    value.dialect = default_dialect();
    value.next_id = NodeId{1000};
    return value;
}

bool lossless(const InlineCstTree& tree, std::u32string_view source) {
    return tokens_partition_source(tree, source.size())
        && roots_partition_source(tree, source.size())
        && flatten_tokens(tree, source) == source
        && serialize_lossless(tree, source) == source;
}

std::size_t count_nodes(const InlineCstNodes& nodes, InlineCstKind kind) {
    std::size_t count = 0;
    for (const auto& node : nodes) {
        count += node.kind == kind ? 1u : 0u;
        count += count_nodes(node.children, kind);
    }
    return count;
}

std::size_t count_tokens(const InlineCstTree& tree, TokenKind kind) {
    std::size_t count = 0;
    for (const auto& token : tree.tokens) count += token.kind() == kind ? 1u : 0u;
    return count;
}

} // namespace

suite inline_scalar_properties = [] {

"literal CR LF and CRLF form one lossless break each"_test = [] {
    const std::vector<std::pair<std::u32string, std::size_t>> cases{
        {U"left\rright", 1},
        {U"left\nright", 1},
        {U"left\r\nright", 2},
    };
    for (const auto& [source, ending_length] : cases) {
        const auto tree = parse_inline(source, context());
        expect(lossless(tree, source)) << "source=" << cps_to_utf8(source);
        expect(count_nodes(tree.nodes, InlineCstKind::SoftBreak) == 1u);
        expect(count_nodes(tree.nodes, InlineCstKind::HardBreak) == 0u);
        expect(count_tokens(tree, TokenKind::SoftBreak) == 1u);
        expect(count_tokens(tree, TokenKind::HardBreak) == 0u);
        const auto found = std::find_if(tree.nodes.begin(), tree.nodes.end(), [](const auto& node) {
            return node.kind == InlineCstKind::SoftBreak;
        });
        expect(found != tree.nodes.end());
        if (found != tree.nodes.end()) {
            expect(found->range.length() == ending_length);
            if (ending_length == 2) {
                expect(source.substr(found->range.start, found->range.length()) == U"\r\n");
            } else {
                expect(source[found->range.start] == U'\r' || source[found->range.start] == U'\n');
            }
        }
    }
};

"backslash parity before each literal line ending"_test = [] {
    const std::vector<std::u32string> endings{U"\n", U"\r", U"\r\n"};
    for (const auto& ending : endings) {
        for (std::size_t slash_count = 0; slash_count <= 8; ++slash_count) {
            std::u32string source = U"left";
            source.append(slash_count, U'\\');
            source.append(ending);
            source += U"right";

            const auto tree = parse_inline(source, context());
            expect(lossless(tree, source)) << "source=" << cps_to_utf8(source);
            const bool expected_hard = slash_count % 2 == 1;
            expect(count_nodes(tree.nodes, InlineCstKind::HardBreak) == (expected_hard ? 1u : 0u));
            expect(count_nodes(tree.nodes, InlineCstKind::SoftBreak) == (expected_hard ? 0u : 1u));
            expect(count_tokens(tree, TokenKind::HardBreak) == (expected_hard ? 1u : 0u));
            expect(count_tokens(tree, TokenKind::SoftBreak) == (expected_hard ? 0u : 1u));
            expect(count_tokens(tree, TokenKind::Escape) == slash_count / 2);
        }
    }
};

"HTML text mode never treats a slash as a Markdown hard break"_test = [] {
    InlineParseContext html_context = context();
    html_context.syntax_mode = InlineSyntaxMode::HtmlText;
    for (const auto& ending : std::vector<std::u32string>{U"\n", U"\r", U"\r\n"}) {
        const auto source = std::u32string{U"left\\"} + ending + U"right";
        const auto tree = parse_inline(source, html_context);
        expect(lossless(tree, source)) << "source=" << cps_to_utf8(source);
        expect(count_nodes(tree.nodes, InlineCstKind::HardBreak) == 0u);
        expect(count_tokens(tree, TokenKind::HardBreak) == 0u);
        expect(count_tokens(tree, TokenKind::SoftBreak) == 1u);
    }
};

"terminal backslashes stay visible before a soft break"_test = [] {
    for (const auto& ending : std::vector<std::u32string>{U"\n", U"\r", U"\r\n"}) {
        const std::vector<std::u32string> sources{
            std::u32string{U"foo\\"} + ending,
            std::u32string{U"foo\\"} + ending + U"   ",
        };
        for (const auto& source : sources) {
            const auto tree = parse_inline(source, context());
            expect(lossless(tree, source)) << "source=" << cps_to_utf8(source);
            expect(count_nodes(tree.nodes, InlineCstKind::HardBreak) == 0u);
            expect(count_nodes(tree.nodes, InlineCstKind::SoftBreak) == 1u);
            expect(count_tokens(tree, TokenKind::HardBreak) == 0u);
            expect(count_tokens(tree, TokenKind::SoftBreak) == 1u);
            const auto text = std::find_if(tree.nodes.begin(), tree.nodes.end(), [](const auto& node) {
                return node.kind == InlineCstKind::Text;
            });
            expect(text != tree.nodes.end());
            if (text != tree.nodes.end()) {
                expect(source.substr(text->range.start, text->range.length()) == U"foo\\");
            }
        }
    }
};

"backslash breaks require content after continuation indentation"_test = [] {
    for (const auto& ending : std::vector<std::u32string>{U"\n", U"\r", U"\r\n"}) {
        const auto source = std::u32string{U"foo\\"} + ending + U"  bar";
        const auto tree = parse_inline(source, context());
        expect(lossless(tree, source)) << "source=" << cps_to_utf8(source);
        expect(count_nodes(tree.nodes, InlineCstKind::HardBreak) == 1u);
        expect(count_nodes(tree.nodes, InlineCstKind::SoftBreak) == 0u);
        expect(count_tokens(tree, TokenKind::HardBreak) == 1u);
        expect(count_tokens(tree, TokenKind::SoftBreak) == 0u);
    }
};

"terminal two-space hard break policy is retained"_test = [] {
    const auto source = std::u32string{U"foo  \n"};
    const auto tree = parse_inline(source, context());
    expect(lossless(tree, source));
    expect(count_nodes(tree.nodes, InlineCstKind::HardBreak) == 1u);
    expect(count_tokens(tree, TokenKind::HardBreak) == 1u);
};

};
