#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.dialect;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.utf;

import folia.markdown.markdown_html;
#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

namespace {

BlockNode const* find_code_block(BlockNode const& block) {
    if (block.kind == BlockKind::CodeBlock) return &block;
    for (auto const& child : block.children) {
        if (auto const* found = find_code_block(child)) return found;
    }
    return nullptr;
}

} // namespace

suite code_block_projection_tests = [] {

"code_block_projection_keeps_fenced_blank_lines_and_trims_indented_blank_edges"_test = [] {
    struct Case {
        std::string markdown;
        std::string expected_code_text;
        BlockSourceKind kind;
    };
    const std::vector<Case> cases{
        {"```\nfoo\n```\n", "foo\n", BlockSourceKind::FencedCode},
        {"```\nfoo\n\n```\n", "foo\n\n", BlockSourceKind::FencedCode},
        {"```\nfoo\n\n\n```\n", "foo\n\n\n", BlockSourceKind::FencedCode},
        {"```\n```\n", "", BlockSourceKind::FencedCode},
        {"```\n\n```\n", "\n", BlockSourceKind::FencedCode},
        {"```\n\n\n```\n", "\n\n", BlockSourceKind::FencedCode},
        {"\n    \n    foo\n    \n\n", "foo\n", BlockSourceKind::IndentedCode},
    };

    for (auto const& test_case : cases) {
        auto parsed = parse_text(1, test_case.markdown, folia::test::commonmark::dialect());
        expect(fatal(serialize_markdown(parsed.document) == test_case.markdown));

        auto const* code = find_code_block(parsed.document.root);
        expect(fatal(code != nullptr));
        if (!code) continue;
        expect(fatal(code->block_source.tree().kind == test_case.kind));
        expect(fatal(block_source_tokens_partition(code->block_source)));
        expect(fatal(flatten_block_source_tokens(code->block_source) == code->block_source.source()));
        expect(fatal(folia::test::commonmark::projection_detail::code_block_text(code->block_source)
            == test_case.expected_code_text));
    }
};

};
