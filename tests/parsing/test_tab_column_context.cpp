#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.ids;
import folia.markdown.inline_document;
import folia.markdown.block_source;
import folia.markdown.serializer;
import folia.markdown.utf;
import folia.markdown.text_edit;
import folia.markdown.markdown_html;

using namespace folia;
using namespace boost::ut;

#include "support/parser_test_support.hpp"
import folia.markdown.dialect;
import folia.markdown.inline_cst;
#include "support/commonmark_projection.hpp"

suite tab_column_context_tests = [] {

"partial_tab_columns_are_projected_without_changing_code_source"_test = [] {
    // In #5/#6/#7 a container consumes two columns of a tab that physically
    // ends at column four. The next raw tab advances to column eight; after
    // the code block consumes four columns, two visual columns remain.
    const std::u32string source = U"\tfoo\n";
    const std::vector<BlockLineIndentation> contexts{{0, 4, 2}};
    auto document = make_block_source(
        source, BlockSourceKind::IndentedCode, contexts);

    expect(block_source_content(document) == U"foo");
    expect(block_source_code_projection(document) == U"  foo");
    expect(document.source() == source);
    expect(flatten_block_source_tokens(document) == source);
    expect(block_source_tokens_partition(document));
    expect(document.tree().generated_indentation.size() == 1u);
    if (!document.tree().generated_indentation.empty()) {
        const auto& generated = document.tree().generated_indentation.front();
        expect(generated.content_offset == 0u);
        expect(generated.source_offset == 1u);
        expect(generated.columns == 2u);
    }
    expect(document.tree().content.size() + 1u == document.tree().content_to_source.size());
    expect(std::ranges::is_sorted(document.tree().content_to_source));
};

"tab_context_survives_block_source_copy_and_reparse"_test = [] {
    auto original = make_block_source(
        U"\tfoo\n", BlockSourceKind::IndentedCode,
        {{0, 4, 2}});
    auto copied = original;
    reparse_block_source(copied);
    expect(copied.source() == U"\tfoo\n");
    expect(block_source_content(copied) == U"foo");
    expect(block_source_code_projection(copied) == U"  foo");
};

"tab_context_transform_tracks_replacements_and_physical_line_endings"_test = [] {
    const std::u32string old_source = U"a\r\nb\nc\rd";
    const std::vector<BlockLineIndentation> old_contexts{
        {0, 3, 1}, {3, 3, 1}, {5, 2, 0}, {7, 4, 2}};
    const TextEdit edit{NodeId{1}, {1, 1}, U"x\r\ny"};
    const auto transformed = transform_block_line_indentation(old_contexts, edit, old_source);
    const std::vector<BlockLineIndentation> expected{
        {0, 3, 1}, {4, 3, 1}, {7, 3, 1}, {9, 2, 0}, {11, 4, 2}};
    expect(transformed == expected);
};

"official_tab_examples_5_through_7_keep_two_residual_code_columns"_test = [] {
    struct Case { std::string source; std::u32string raw_code; std::u32string projected; };
    const std::vector<Case> cases{
        {"- foo\n\n\t\tbar\n", U"\tbar", U"  bar"},
        {">\t\tfoo\n", U"\tfoo\n", U"  foo"},
        {"-\t\tfoo\n", U"\tfoo", U"  foo"},
    };
    for (const auto& item : cases) {
        const auto parsed = parse_text(1, item.source, folia::test::commonmark::dialect());
        const auto* code = first_block(parsed.document.root.children, BlockKind::CodeBlock);
        expect(fatal(code != nullptr));
        if (!code) continue;
        expect(code->block_source.source() == item.raw_code);
        expect(block_source_code_projection(code->block_source) == item.projected);
        expect(flatten_block_source_tokens(code->block_source) == item.raw_code);
    }
};

"third_tabbed_list_line_keeps_nested_list_column_context"_test = [] {
    const auto parsed = parse_text(
        1, " - foo\n   - bar\n\t - baz\n", folia::test::commonmark::dialect());
    const auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(projection.strict_tree.find("\"baz\"") != std::string::npos);
};

"partial_tab_columns_apply_before_heading_and_quote_recognition"_test = [] {
    for (const auto& source : {std::string(">\t  # literal\n"), std::string(">\t  > literal\n")}) {
        const auto parsed = parse_text(1, source);
        const auto* code = first_block(parsed.document.root.children, BlockKind::CodeBlock);
        expect(fatal(code != nullptr));
        if (!code) continue;
        expect(serialize_markdown(parsed.document) == source);
        expect(code->block_source.tree().generated_indentation.empty());
        expect(code->block_source.tree().content.find(U"literal") != std::u32string::npos);
    }
};

};
