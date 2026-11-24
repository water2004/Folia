#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.dialect;
import folia.markdown.text_edit;
import folia.markdown.parser;
import folia.core.document_edit;
import folia.markdown.document;
import folia.markdown.document_symbols;
import folia.markdown.inline_parser;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.callout;
import folia.core.text_measurer;
import folia.core.block_layout;
import folia.core.hit_test;
import folia.core.selection_geometry;
import folia.markdown.selection;

using namespace folia;
using namespace boost::ut;

#include "support/render_layout_test_support.hpp"

suite rendering_model_tables_tests = [] {

"builds_table_block"_test = [] {
    auto m = build_model("| A | B |\n|---|---|\n| 1 | 2 |\n");
    expect(fatal(bool(m.blocks[0].kind == RenderBlockKind::Table)));
    expect(fatal(bool(m.blocks[0].special().row_count >= 2)));
    expect(fatal(bool(m.blocks[0].special().column_count >= 2)));
};

"builds_safe_html_table_with_cell_ranges"_test = [] {
    auto m = build_model("<table><tr><th>A</th><th>B</th></tr><tr><td>1</td><td>2</td></tr></table>\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    expect(fatal(bool(m.blocks[0].kind == RenderBlockKind::Table)));
    expect(fatal(bool(m.blocks[0].special().table_header_row)));
    expect(fatal(bool((m.blocks[0].special().row_count) == (2u))));
    expect(fatal(bool((m.blocks[0].special().column_count) == (2u))));
    expect(fatal(bool((m.blocks[0].special().table_cell_spans.size()) == (4u))));
    for (auto const& range : m.blocks[0].special().table_cell_spans) expect(fatal(bool(range.source_range.end >= range.source_range.start)));
};

"register_html_table_reaches_native_table_render_model"_test = [] {
    const std::string source =
        "<table><tr><td>寄存器</td><td>位宽/bit</td><td>助记符</td><td>汇编用法</td><td>类型</td></tr>"
        "<tr><td>0</td><td>32</td><td>%A0</td><td>临时寄存器 0</td><td>通用寄存器</td></tr>"
        "<tr><td>1</td><td>32</td><td>%A1</td><td>临时寄存器 1</td><td>通用寄存器</td></tr>"
        "<tr><td>2</td><td>32</td><td>%A2</td><td>临时寄存器 2</td><td>通用寄存器</td></tr>"
        "<tr><td>3</td><td>32</td><td>%A3</td><td>临时寄存器 3</td><td>通用寄存器</td></tr>"
        "<tr><td>4</td><td>32</td><td>%A4</td><td>临时寄存器 4</td><td>通用寄存器</td></tr>"
        "<tr><td>5</td><td>32</td><td>%A5</td><td>临时寄存器 5</td><td>通用寄存器</td></tr>"
        "<tr><td>6</td><td>32</td><td>%A6</td><td>临时寄存器 6</td><td>通用寄存器</td></tr>"
        "<tr><td>7</td><td>32</td><td>%A7</td><td>临时寄存器 7</td><td>通用寄存器</td></tr></table>";
    auto model = build_model(source);
    expect(fatal(model.blocks.size() == 1u));
    if (model.blocks.empty()) return;
    const auto& table = model.blocks.front();
    expect(fatal(table.kind == RenderBlockKind::Table));
    expect(fatal(table.special().row_count == 9u));
    expect(fatal(table.special().column_count == 5u));
    expect(fatal(table.special().table_cells.size() == 45u));
    expect(fatal(table.special().table_cell_spans.size() == 45u));
    expect(!table.special().table_header_row);

    auto afterParagraph = build_model("preceding paragraph\n" + source);
    expect(fatal(afterParagraph.blocks.size() == 2u));
    if (afterParagraph.blocks.size() == 2u) {
        expect(fatal(afterParagraph.blocks[0].kind == RenderBlockKind::Text));
        expect(fatal(afterParagraph.blocks[1].kind == RenderBlockKind::Table));
        expect(fatal(afterParagraph.blocks[1].special().row_count == 9u));
    }
};

"table_render_cells_keep_their_own_source_ranges"_test = [] {
    auto m = build_model("| A | B |\n| :--- | ---: |\n| 1 | 2 |\n");
    expect(fatal(bool(m.blocks[0].kind == RenderBlockKind::Table)));
    expect(fatal(bool((m.blocks[0].special().table_cells.size()) == (4u))));
    expect(fatal(bool(!m.blocks[0].special().table_cells[0].empty())));
    expect(fatal(bool(!m.blocks[0].special().table_cells[1].empty())));
    if (!m.blocks[0].special().table_cells[0].empty() && !m.blocks[0].special().table_cells[1].empty()) {
        expect(fatal(bool((m.blocks[0].special().table_cells[0][0].source_span.source_range.start) == (0u))));
        expect(fatal(bool((m.blocks[0].special().table_cells[1][0].source_span.source_range.start) == (0u))));
        expect(fatal(bool(m.blocks[0].special().table_cells[0][0].source_span.container_id != m.blocks[0].special().table_cells[1][0].source_span.container_id)));
    }
};

"table_render_cells_preserve_inline_styles"_test = [] {
    auto m = build_model("| **bold** | plain |\n| --- | --- |\n| `code` | ~~gone~~ |\n");
    expect(fatal(bool(m.blocks[0].kind == RenderBlockKind::Table)));
    expect(fatal(bool((m.blocks[0].special().table_cells.size()) == (4u))));
    bool bold = false;
    bool code = false;
    bool strike = false;
    for (auto const& item : m.blocks[0].special().table_cells[0]) bold = bold || item.style.bold;
    for (auto const& item : m.blocks[0].special().table_cells[2]) code = code || item.style.code == CodeStyle::Inline;
    for (auto const& item : m.blocks[0].special().table_cells[3]) strike = strike || item.style.strikethrough;
    expect(fatal(bool(bold)));
    expect(fatal(bool(code)));
    expect(fatal(bool(strike)));
};

};
