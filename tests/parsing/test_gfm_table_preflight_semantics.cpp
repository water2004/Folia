#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <string_view>
#include <span>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"

import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.serializer;
import folia.markdown.utf;

import folia.markdown.markdown_html;
import folia.markdown.dialect;
#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

namespace {

void expect_one_paragraph(std::string const& source, std::string const& expected_html) {
    const auto parsed = parse_text(1, source, folia::test::commonmark::gfm_dialect());
    expect(parsed.document.root.children.size() == 1_u);
    if (parsed.document.root.children.size() != 1) return;
    const auto& block = parsed.document.root.children.front();
    expect(block.kind == BlockKind::Paragraph);
    std::u32string expected_source;
    const auto source_without_terminal_newline = source.ends_with('\n')
        ? std::string_view(source).substr(0, source.size() - 1) : std::string_view(source);
    for (const auto ch : source_without_terminal_newline)
        expected_source.push_back(static_cast<char32_t>(static_cast<unsigned char>(ch)));
    expect(bool(block.inline_content.source == expected_source));
    expect(serialize_markdown(parsed.document) == source);
    expect(folia::test::commonmark::project(parsed.document.root).projected_html == expected_html);
}

} // namespace

suite gfm_table_preflight_semantics_tests = [] {
    "published_gfm_203_mismatched_table_column_counts_stay_paragraph"_test = [] {
        const std::string source = "| abc | def |\n| --- |\n| bar |\n";
        expect_one_paragraph(source, "<p>| abc | def |\n| --- |\n| bar |</p>\n");
    };

    "cmark_spec_203_mismatched_table_column_counts_stay_paragraph"_test = [] {
        const std::string source = "| abc | def |\n| --- |\n| bar |\n";
        expect_one_paragraph(source, "<p>| abc | def |\n| --- |\n| bar |</p>\n");
    };

    "extensions_7_mismatched_header_and_separator_stay_paragraph"_test = [] {
        const std::string source = "| a | b | c |\n| --- | --- |\n| this | isn't | okay |\n";
        expect_one_paragraph(source, "<p>| a | b | c |\n| --- | --- |\n| this | isn't | okay |</p>\n");
    };

    "extensions_16_paragraph_then_table_without_blank_line"_test = [] {
        const std::string source = "123\n456\n| a | b |\n| ---| --- |\nd | e\n";
        const auto parsed = parse_text(2, source, folia::test::commonmark::gfm_dialect());
        expect(parsed.document.root.children.size() == 2_u);
        if (parsed.document.root.children.size() != 2) return;
        expect(parsed.document.root.children[0].kind == BlockKind::Paragraph);
        expect(parsed.document.root.children[0].inline_content.source == U"123\n456");
        expect(parsed.document.root.children[1].kind == BlockKind::Table);
        expect(serialize_markdown(parsed.document) == source);
        expect(folia::test::commonmark::project(parsed.document.root).projected_html
            == "<p>123\n456</p>\n<table>\n<thead>\n<tr>\n<th>a</th>\n<th>b</th>\n</tr>\n</thead>\n<tbody>\n<tr>\n<td>d</td>\n<td>e</td>\n</tr>\n</tbody>\n</table>\n");
    };

    "regressions_19_pipe_and_hyphen_pipe_are_one_paragraph"_test = [] {
        const std::string source = "|\n-|\n";
        expect_one_paragraph(source, "<p>|\n-|</p>\n");
    };
};
