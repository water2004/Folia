#include <string>
#include <optional>
#include "support/folia_test.hpp"

import folia.markdown.block_source;
import folia.markdown.parser_source_session;
import folia.markdown.parser_leaf_line_view;
import folia.markdown.parser_code_math_leaf_source;

using namespace folia::parser_source_detail;
using folia::BlockLineIndentation;
using folia::BlockSourceKind;
using namespace boost::ut;

namespace {
LeafLine logical_line(RootPhysicalSource const& root, std::size_t line, std::size_t content_start) {
    const auto& physical = root.line(line);
    const auto physical_content_start = physical.begin + content_start;
    LeafLine result;
    result.content_cursor = root.begin_line(line);
    while (result.content_cursor.offset < physical_content_start)
        root.advance(result.content_cursor);
    result.ending = {physical.content_end, physical.ending_end};
    if (physical_content_start < physical.content_end)
        result.content_spans.push_back({physical_content_start, physical.content_end});
    return result;
}
}

suite code_math_leaf_source = [] {
    "fenced_code_keeps_root_content_and_exact_crlf_but_omits_close_eol"_test = [] {
        const std::u32string input = U"> ```cpp\r\n>   x\r\n> ``` \t\r\n";
        RootPhysicalSource root(input);
        CodeMathLeafSource leaf(root);
        const auto opening = logical_line(root, 0, 2);
        const auto body = logical_line(root, 1, 2);
        const auto closing = logical_line(root, 2, 2);
        append_opening_and_body_line(leaf, opening);
        append_fence_body_line(leaf, body);
        const auto closing_part = leaf.append(closing, false);
        leaf.append_ending(closing);
        auto block_source = finish_code_math_source(
            leaf, BlockSourceKind::FencedCode, closing_part.source_end);
        expect(bool(block_source.source() == U"```cpp\r\n  x\r\n``` \t"));
        expect(bool(leaf.materialize() == U"```cpp\r\n  x\r\n``` \t\r\n"));
        auto owner_prefix = leaf.materialize();
        owner_prefix.resize(closing_part.source_end);
        expect(bool(owner_prefix == block_source.source()));
        const auto mapped = leaf.physical_range(0, leaf.size());
        expect(bool(mapped && mapped->begin == 2 && mapped->end == input.size()));
        expect(bool(leaf.physical_offset(8) == std::optional<std::size_t>{12}));
    };

    "indented_code_records_physical_tab_context_and_source_offsets"_test = [] {
        const std::u32string input = U"\tcode\r\n  \tmore\n";
        RootPhysicalSource root(input);
        CodeMathLeafSource leaf(root);
        append_indented_code_line(leaf, logical_line(root, 0, 0));
        append_indented_code_line(leaf, logical_line(root, 1, 0));
        expect(bool(leaf.materialize() == input));
        const auto contexts = leaf.take_indentation_contexts();
        expect(bool(contexts.size() == 2u));
        expect(bool(bool(contexts[0] == BlockLineIndentation{0, 0, 0})));
        expect(bool(bool(contexts[1] == BlockLineIndentation{7, 0, 0})));
    };

    "math_source_preserves_unclosed_body_and_cr_eol"_test = [] {
        const std::u32string input = U"$$\rbody\r";
        RootPhysicalSource root(input);
        CodeMathLeafSource leaf(root);
        append_opening_and_body_line(leaf, logical_line(root, 0, 0));
        append_fence_body_line(leaf, logical_line(root, 1, 0));
        expect(bool(leaf.materialize() == input));
        const auto mapped = leaf.physical_range(2, leaf.size());
        expect(bool(mapped && mapped->begin == 2 && mapped->end == input.size()));
    };

    "math_closer_can_end_inside_line_and_preserve_suffix_boundary"_test = [] {
        const std::u32string input = U"$$abc$$tail\n";
        RootPhysicalSource root(input);
        CodeMathLeafSource leaf(root);
        const auto line = logical_line(root, 0, 0);
        const auto before_close = leaf.append_content_range(line, 0, 5);
        const auto closer = leaf.append_content_range(line, 5, 7);
        expect(bool(leaf.materialize() == U"$$abc$$"));
        expect(bool(before_close.source_begin == 0u && before_close.source_end == 5u));
        expect(bool(closer.source_begin == 5u && closer.source_end == 7u));
        const auto mapped = leaf.physical_range(0, leaf.size());
        expect(bool(mapped && mapped->begin == 0 && mapped->end == 7));
    };
};
