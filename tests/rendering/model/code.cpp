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
import folia.core.layout_tree;
import folia.core.types;
import folia.markdown.symbols;
import folia.markdown.text_edit;
import folia.markdown.parser;
import folia.markdown.dialect;
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

suite rendering_model_code_tests = [] {

"code_presentation_role_comes_from_semantic_projection_in_every_container"_test = [] {
    struct Case { std::string source; CodeStyle role; };
    for (auto const& example : {
        Case{"`alpha`\n", CodeStyle::Inline},
        Case{"# `alpha`\n", CodeStyle::Inline},
        Case{"- `alpha`\n", CodeStyle::Inline},
        Case{"- [ ] `alpha`\n", CodeStyle::Inline},
        Case{"> `alpha`\n", CodeStyle::Inline},
        Case{"> [!NOTE]\n> `alpha`\n", CodeStyle::Inline},
        Case{"[^n]: `alpha`\n\nref[^n]\n", CodeStyle::Inline},
        Case{"| head |\n| --- |\n| `alpha` |\n", CodeStyle::Inline},
        Case{"<code>alpha</code>\n", CodeStyle::Inline},
        Case{"`alpha\n", CodeStyle::None},
        Case{"> ```txt\n> alpha\n> ```\n", CodeStyle::Block}}) {
        auto model = build_model(example.source);
        std::size_t matches = 0;
        auto inspect = [&](auto const& items) {
            for (auto const& item : items) {
                if (item.kind != InlineRenderItem::Kind::Text || item.text.find(U"alpha") == std::u32string::npos) continue;
                ++matches;
                expect(item.style.code == example.role) << example.source;
            }
        };
        auto walk = [&](auto&& self, RenderBlock const& block) -> void {
            inspect(block.inline_items);
            for (auto const& cell : block.special().table_cells) inspect(cell);
            for (auto const& child : block.child_blocks) self(self, child);
        };
        for (auto const& block : model.blocks) walk(walk, block);
        expect(matches > 0_u) << example.source;
    }
};

"builds_code_block"_test = [] {
    auto m = build_model("```rust\nfn main() {}\n```");
    expect(fatal(bool(!m.blocks.empty())));
    expect(fatal(bool(m.blocks[0].kind == RenderBlockKind::Code)));
    expect(fatal(bool(m.blocks[0].special().language && *m.blocks[0].special().language == "rust")));
    expect(fatal(bool(m.blocks[0].special().line_count == 1u)));
};

"code_block_terminal_endings_do_not_create_phantom_visual_lines"_test = [] {
    struct Case {
        std::string source;
        std::size_t expected_lines;
        std::u32string expected_last_line;
    };
    const std::vector<Case> cases{
        {"```cpp\nvalue\n```", 1u, U"value"},
        {"```cpp\nvalue\n\n```", 2u, U""},
        {"```cpp\r\nvalue\r\n```", 1u, U"value"},
        {"```cpp\r\nvalue\r\n\r\n```", 2u, U""},
        {"```cpp\rvalue\r```", 1u, U"value"},
        {"```cpp\n```", 1u, U""},
        {"```cpp\nvalue", 1u, U"value"},
    };
    StubMeasurer measurer(6.0f);
    for (const auto& item : cases) {
        const auto model = build_model(item.source);
        expect(fatal(bool(model.blocks.size() == 1u)));
        if (model.blocks.empty()) continue;
        const auto& code = model.blocks.front();
        expect(fatal(bool(code.kind == RenderBlockKind::Code)));
        expect(fatal(bool(code.special().language == std::optional<std::string>{"cpp"})));
        expect(fatal(bool(code.special().line_count == item.expected_lines)));
        const auto layout = layout_blocks(
            model.blocks,
            800.0f,
            1.0f,
            measurer,
            std::nullopt,
            LogicalPoint(0, 0),
            Outline::empty(1));
        expect(fatal(bool(layout.blocks.size() == 1u)));
        if (layout.blocks.empty()) continue;
        expect(fatal(bool(layout.blocks.front().children.size() == item.expected_lines)));
        if (!layout.blocks.front().children.empty()) {
            const auto& line = layout.blocks.front().children.back().line;
            expect(fatal(bool(line.runs.size() == 1u)));
            if (!line.runs.empty()) {
                expect(fatal(bool(line.runs.front().text == item.expected_last_line)));
            }
        }
    }
};

"builds_indented_code_block_with_local_code_source"_test = [] {
    auto m = build_model("    **alpha**\n    _beta_ $gamma$\n");
    expect(fatal(bool((m.blocks.size()) == (1u))));
    expect(fatal(bool(m.blocks[0].kind == RenderBlockKind::Code)));
    expect(fatal(bool(m.blocks[0].special().code_indented)));
    expect(fatal(bool((m.blocks[0].special().code_text) == (std::u32string(U"**alpha**\n_beta_ $gamma$")))));
    expect(fatal(bool(m.blocks[0].inline_items.empty())));
    expect(fatal(bool((m.blocks[0].source_span.source_range.start) == (0u))));
    expect(fatal(bool((m.blocks[0].source_span.source_range.end) == (m.blocks[0].special().raw_source.size()))));
};

"nested_indented_code_keeps_markdown_markers_literal"_test = [] {
    auto model = build_model(
        "1. item\n"
        "\n"
        "       **bold** _italic_ $math$\n");
    const auto* code = find_render_block(model.blocks.front(), RenderBlockKind::Code);
    expect(fatal(bool(code != nullptr))) << "nested code node";
    if (!code) return;
    expect(fatal(bool(code->special().code_indented))) << "indented code kind";
    auto literal = code->special().code_text;
    if (!literal.empty() && literal.back() == U'\n') literal.pop_back();
    expect(fatal(bool(literal == U"**bold** _italic_ $math$"))) << "literal code source";

    std::u32string displayed;
    for (const auto& item : model.blocks.front().inline_items) {
        if (item.source_span.container_id != code->id
            || item.kind != InlineRenderItem::Kind::Text) continue;
        displayed += item.text;
        expect(fatal(item.style.code == CodeStyle::Block)) << "block code style only";
        expect(fatal(bool(!item.style.bold && !item.style.italic
            && !item.style.link && !item.style.strikethrough))) << "no markdown inline style";
    }
    const auto presentation_lines = code_presentation_lines(code->special().code_text);
    const auto presentation_end = presentation_lines.empty()
        ? std::size_t{0}
        : presentation_lines.back().content_range.end;
    expect(fatal(bool(displayed == code->special().code_text.substr(0, presentation_end))))
        << "displayed literal source without a phantom terminal line";
};

};
