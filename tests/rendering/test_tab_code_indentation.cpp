#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.parser;
import folia.markdown.document;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.block_source;
import folia.markdown.ast;
import folia.markdown.inline_parser;
import folia.core.layout_tree;
import folia.core.types;
import folia.core.theme;
import folia.markdown.ids;
import folia.markdown.inline_document;
import folia.markdown.block_tree;
import folia.core.text_measurer;
import folia.core.block_layout;
import folia.core.hit_test;
import folia.markdown.selection;
import folia.markdown.outline;

using namespace folia;
using namespace boost::ut;

#include "support/render_layout_test_support.hpp"

namespace {

const BlockNode* first_code_block(const std::vector<BlockNode>& blocks) {
    for (const auto& block : blocks) {
        if (block.kind == BlockKind::CodeBlock) return &block;
        if (const auto* nested = first_code_block(block.children)) return nested;
    }
    return nullptr;
}

const InlineRenderItem* generated_indent_item(const RenderBlock& block, NodeId owner, std::size_t offset) {
    for (const auto& item : block.inline_items) {
        if (item.kind == InlineRenderItem::Kind::Marker
            && item.source_span.container_id == owner
            && item.source_span.source_range == SourceRange{offset, offset}
            && item.special().generated_boundary_affinity == TextAffinity::Downstream
            && item.special().display_text == U"  ") return &item;
    }
    for (const auto& child : block.child_blocks) {
        if (const auto* found = generated_indent_item(child, owner, offset)) return found;
    }
    return nullptr;
}

const InlineRenderItem* source_text_item(const RenderBlock& block, NodeId owner, std::u32string_view text) {
    for (const auto& item : block.inline_items) {
        if (item.kind == InlineRenderItem::Kind::Text
            && item.source_span.container_id == owner && item.text == text) return &item;
    }
    for (const auto& child : block.child_blocks) {
        if (const auto* found = source_text_item(child, owner, text)) return found;
    }
    return nullptr;
}

const GlyphRunLayout* generated_indent_run(const LayoutBlock& block, NodeId owner, std::size_t offset) {
    for (const auto& item : block.children) {
        if (item.kind == LayoutItem::Kind::Line) {
            for (const auto& run : item.line.runs) {
                if (run.source_span.container_id == owner
                    && run.source_span.source_range == SourceRange{offset, offset}
                    && run.generated_boundary_affinity == TextAffinity::Downstream
                    && run.text == U"  ") return &run;
            }
        }
    }
    return nullptr;
}

const TextLineLayout* generated_indent_line(const LayoutBlock& block, NodeId owner, std::size_t offset) {
    for (const auto& item : block.children) {
        if (item.kind != LayoutItem::Kind::Line) continue;
        for (const auto& run : item.line.runs) {
            if (run.source_span.container_id == owner
                && run.source_span.source_range == SourceRange{offset, offset}
                && run.generated_boundary_affinity == TextAffinity::Downstream
                && run.text == U"  ") return &item.line;
        }
    }
    return nullptr;
}

BlockNode make_standalone_tab_code() {
    BlockNode code;
    code.id = NodeId{2};
    code.kind = BlockKind::CodeBlock;
    code.block_source = make_block_source(
        U"\tfoo\n", BlockSourceKind::IndentedCode, {{0, 4, 2}});
    code.ensure_atomic_special().code_indented = true;
    return code;
}

BlockNode make_context_code(std::size_t residual) {
    BlockNode code;
    code.id = NodeId{2};
    code.kind = BlockKind::CodeBlock;
    code.block_source = make_block_source(
        U"\tfoo\n", BlockSourceKind::IndentedCode, {{0, 4, residual}});
    code.ensure_atomic_special().code_indented = true;
    return code;
}

} // namespace

suite tab_code_indentation_render_tests = [] {

"standalone_indented_code_lays_out_generated_partial_tab_columns"_test = [] {
    auto model = build_model(make_standalone_tab_code(), 3);
    const auto* block = model.blocks.empty() ? nullptr : &model.blocks.front();
    expect(fatal(block != nullptr));
    if (!block) return;
    expect(block->special().code_text == U"foo");
    expect(block->special().generated_code_indentation.size() == 1u);

    StubMeasurer measurer(6.0f);
    const auto tree = layout_blocks(
        model.blocks, 800.0f, 1.0f, measurer, std::nullopt,
        LogicalPoint(0, 0), Outline::empty(1));
    expect(fatal(tree.blocks.size() == 1u));
    if (tree.blocks.empty()) return;
    const auto& line = tree.blocks.front().children.front().line;
    expect(line.runs.size() == 2u);
    const auto& generated = line.runs.front();
    const auto& actual = line.runs.back();
    expect(generated.text == U"  ");
    expect(generated.source_span.source_range.empty());
    expect(generated.generated_boundary_affinity == TextAffinity::Downstream);
    expect(actual.text == U"foo");
    expect(actual.source_span.source_range == SourceRange{1, 4});
};

"render_source_key_includes_generated_columns_for_identical_raw_code"_test = [] {
    const auto two_columns = make_context_code(2);
    const auto one_column = make_context_code(1);
    expect(two_columns.block_source.source() == one_column.block_source.source());
    expect(block_source_code_projection(two_columns.block_source) == U"  foo");
    expect(block_source_code_projection(one_column.block_source) == U" foo");
    const auto model_two = build_model(two_columns, 3);
    const auto model_one = build_model(one_column, 3);
    expect(fatal(model_two.blocks.size() == 1u && model_one.blocks.size() == 1u));
    if (model_two.blocks.size() == 1u && model_one.blocks.size() == 1u) {
        expect(model_two.blocks.front().source_key != model_one.blocks.front().source_key);
    }
};

"quote_flow_keeps_partial_tab_generated_run_and_hits_its_source_boundary"_test = [] {
    auto parsed = parse_text(1, ">\t\tfoo\n");
    const auto* code = first_code_block(parsed.document.root.children);
    expect(fatal(code != nullptr));
    if (!code || code->block_source.tree().generated_indentation.empty()) return;
    const auto generated = code->block_source.tree().generated_indentation.front();
    const auto owner = code->id;
    const auto raw = code->block_source.source();
    const auto expected_start = code->block_source.tree().content_to_source.front();
    const auto expected_end = code->block_source.tree().content_to_source[3];

    auto model = build_render_model(
        parsed.document,
        parsed.outline,
        parsed.symbols,
        default_theme_profile());
    const InlineRenderItem* marker = nullptr;
    const InlineRenderItem* text = nullptr;
    for (const auto& block : model.blocks) {
        if (!marker) marker = generated_indent_item(block, owner, generated.source_offset);
        if (!text) text = source_text_item(block, owner, U"foo");
    }
    expect(fatal(marker != nullptr));
    expect(fatal(text != nullptr));
    if (!marker || !text) return;
    expect(marker->source_span.source_range.empty());
    expect(marker->special().generated_boundary_affinity == TextAffinity::Downstream);
    expect(text->source_span.source_range == SourceRange{expected_start, expected_end});
    expect(text->text == U"foo");
    expect(raw.size() >= expected_end);

    StubMeasurer measurer(6.0f);
    const auto tree = layout_blocks(
        model.blocks, 800.0f, 1.0f, measurer, std::nullopt,
        LogicalPoint(0, 0), Outline::empty(1));
    const LayoutBlock* containing = nullptr;
    for (const auto& block : tree.blocks) {
        if (const auto* found = generated_indent_run(block, owner, generated.source_offset)) {
            (void)found;
            containing = &block;
            break;
        }
    }
    expect(fatal(containing != nullptr));
    if (!containing) return;
    const auto* run = generated_indent_run(*containing, owner, generated.source_offset);
    expect(fatal(run != nullptr));
    if (!run) return;
    const auto* line = generated_indent_line(*containing, owner, generated.source_offset);
    expect(fatal(line != nullptr));
    if (!line) return;
    const auto hit = hit_test_layout_tree(
        tree,
        LogicalPoint(run->origin.x + run->width * 0.5f, line->rect.y + line->rect.height * 0.5f));
    expect(fatal(hit.has_value()));
    if (hit) {
        expect(hit->position.container_id == owner);
        expect(hit->position.source_offset == generated.source_offset);
        expect(hit->position.affinity == TextAffinity::Downstream);
    }
};

};
