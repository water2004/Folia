#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.document_text;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/parser_test_support.hpp"

namespace {

std::vector<const BlockNode*> paragraphs(const BlockVec& blocks) {
    std::vector<const BlockNode*> result;
    auto visit = [&](auto&& self, const BlockNode& block) -> void {
        if (block.kind == BlockKind::Paragraph) result.push_back(&block);
        for (const auto& child : block.children) self(self, child);
    };
    for (const auto& block : blocks) visit(visit, block);
    return result;
}

void expect_exact_roundtrip(std::string_view label, const std::string& source) {
    const auto parsed = parse_text(1, source);
    const auto saved = serialize_markdown(parsed.document);
    expect(saved == source) << label << " first save";
    const auto reloaded = parse_text(2, saved);
    expect(serialize_markdown(reloaded.document) == source) << label << " reload save";
}

} // namespace

suite parser_list_separator_tests = [] {

"commonmark_306_blank_runs_between_list_items"_test = [] {
    expect_exact_roundtrip("CommonMark #306", "- foo\n\n- bar\n\n\n- baz\n");
};

"commonmark_311_blank_runs_and_ordered_marker_indentation"_test = [] {
    expect_exact_roundtrip("CommonMark #311", "1. a\n\n  2. b\n\n   3. c\n");
};

"commonmark_314_blank_run_before_final_list_item"_test = [] {
    expect_exact_roundtrip("CommonMark #314", "- a\n- b\n\n- c\n");
};

"commonmark_315_empty_item_separator"_test = [] {
    expect_exact_roundtrip("CommonMark #315", "* a\n*\n\n* c\n");
};

"commonmark_326_nested_list_item_separators"_test = [] {
    expect_exact_roundtrip("CommonMark #326", "- a\n  - b\n  - c\n\n- d\n  - e\n  - f\n");
};

"commonmark_254_separators_between_mixed_item_blocks"_test = [] {
    expect_exact_roundtrip(
        "CommonMark #254",
        "1.  A paragraph\n    with two lines.\n\n        indented code\n\n    > A block quote.\n");
};

"commonmark_256_blank_separator_between_item_paragraphs"_test = [] {
    expect_exact_roundtrip("CommonMark #256", "- one\n\n  two\n");
};

"commonmark_262_multiple_blank_lines_between_item_paragraphs"_test = [] {
    expect_exact_roundtrip("CommonMark #262", "- foo\n\n\n  bar\n");
};

"commonmark_270_blank_separator_does_not_gain_list_indent"_test = [] {
    const auto parsed = parse_text(1, "- foo\n\n      bar\n");
    const auto saved = serialize_markdown(parsed.document);
    expect(saved.find("\n  \n") == std::string::npos)
        << "CommonMark #270 blank separator padding";
    expect(saved.find("\n\n") != std::string::npos)
        << "CommonMark #270 blank separator line";
};

"nested_list_separator_metadata_roundtrips"_test = [] {
    expect_exact_roundtrip(
        "nested list fixture",
        "- parent\n\n  - child\n\n  - sibling\n\n- next\n");
};

"task_list_separator_metadata_roundtrips"_test = [] {
    expect_exact_roundtrip("task list fixture", "- [ ] first\n\n- [x] second\n");
};

"list_item_projection_maps_text_around_preserved_separators"_test = [] {
    const auto parsed = parse_text(1, "- first\n\n- second\n");
    const auto texts = paragraphs(parsed.document.root.children);
    expect(texts.size() == 2u) << "CommonMark #306 paragraph structure";
    if (texts.size() != 2) return;
    const auto projection = serialize_markdown_projection(parsed.document);
    expect(projection.text == U"- first\n\n- second\n") << "CommonMark #306 projection";
    for (const auto* block : texts) {
        const auto expected = block->inline_content.source == U"first" ? U"first" : U"second";
        const auto offset = projection.text.find(expected);
        expect(offset != std::u32string::npos) << "CommonMark #306 projected text";
        if (offset == std::u32string::npos) continue;
        const auto mapped = source_position_for_serialized_offset(
            projection, offset, TextAffinity::Downstream);
        expect(mapped.has_value()) << "CommonMark #306 source mapping";
        if (mapped) {
            expect(mapped->container_id == block->id) << "CommonMark #306 mapped owner";
            expect(mapped->source_offset == 0u) << "CommonMark #306 mapped offset";
        }
    }
};

}; // suite parser_list_separator_tests
