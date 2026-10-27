#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"

import folia.markdown.ast;
import folia.markdown.dialect;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.markdown.block_source;
import folia.markdown.utf;
import folia.markdown.html_entities;
import folia.markdown.serializer;

#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

suite gfm_table_capacity_tests = [] {
    "gfm_pipe_less_table_recognition_preserves_setext_precedence"_test = [] {
        auto dialect = folia::test::commonmark::gfm_dialect();

        auto heading = parse_text(1, "abc\n---", dialect);
        expect(heading.document.root.children.size() == 1_u);
        if (heading.document.root.children.size() == 1)
            expect(heading.document.root.children.front().kind == BlockKind::Heading);
        expect(serialize_markdown(heading.document) == "abc\n---");

        auto aligned_table = parse_text(2, "abc\n:---", dialect);
        expect(aligned_table.document.root.children.size() == 1_u);
        if (aligned_table.document.root.children.size() == 1)
            expect(aligned_table.document.root.children.front().kind == BlockKind::Table);
        expect(serialize_markdown(aligned_table.document) == "abc\n:---");

        auto vt_table = parse_text(3, "abc\n-\v", dialect);
        expect(vt_table.document.root.children.size() == 1_u);
        if (vt_table.document.root.children.size() == 1)
            expect(vt_table.document.root.children.front().kind == BlockKind::Table);
        expect(serialize_markdown(vt_table.document) == "abc\n-\v");
    };

    "gfm_single_column_table_accepts_pipe_less_header_and_vt_padded_separator"_test = [] {
        auto dialect = folia::test::commonmark::gfm_dialect();
        std::string source;
        constexpr std::size_t repetitions = 4;
        for (std::size_t index = 0; index < repetitions; ++index) {
            source += "aaa\rbbb\n-\v\n";
        }

        auto parsed = parse_text(1, source, dialect);
        expect(parsed.document.root.children.size() == 2_u);
        if (parsed.document.root.children.size() != 2) return;
        const auto& paragraph = parsed.document.root.children[0];
        const auto& table = parsed.document.root.children[1];
        expect(paragraph.kind == BlockKind::Paragraph);
        expect(paragraph.inline_content.source == U"aaa");
        expect(table.kind == BlockKind::Table);
        expect(table.children.size() == 1 + 3 * (repetitions - 1));
        if (table.kind != BlockKind::Table || table.children.size() != 1 + 3 * (repetitions - 1)) return;
        expect(table.children.front().table_special().table_header_row);
        expect(table.children.front().children.size() == 1_u);
        expect(table.children.front().children.front().inline_content.source == U"bbb");

        for (std::size_t index = 1; index < table.children.size(); ++index) {
            const auto row_index = (index - 1) % 3;
            const auto expected = row_index == 0 ? U"aaa" : row_index == 1 ? U"bbb" : U"-\v";
            const auto& row = table.children[index];
            expect(row.children.size() == 1_u);
            if (row.children.size() != 1) continue;
            expect(row.children.front().inline_content.source == expected);
            expect(flatten_tokens(row.children.front().inline_content.tree,
                row.children.front().inline_content.source)
                == row.children.front().inline_content.source);
            expect(serialize_lossless(row.children.front().inline_content.tree,
                row.children.front().inline_content.source)
                == row.children.front().inline_content.source);
            expect(row.table_special().row_syntax.has_value());
            if (row.table_special().row_syntax) {
                expect(row.table_special().row_syntax->cell_owners.size() == 1_u);
                expect(row.table_special().row_syntax->gaps.size() == 2_u);
            }
        }

        expect(serialize_markdown(parsed.document) == source);
        const auto html = folia::test::commonmark::project(parsed.document.root).projected_html;
        expect(html.starts_with("<p>aaa</p>\n<table>\n<thead>\n<tr>\n<th>bbb</th>\n</tr>\n</thead>\n<tbody>\n"));
        expect(html.ends_with("</tbody>\n</table>\n"));
        expect(html.find("<td>-\v</td>") != std::string::npos);
    };
};
