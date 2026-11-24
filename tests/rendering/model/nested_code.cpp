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

suite rendering_model_nested_code_tests = [] {

"list_render_model_retains_nested_code_block_identity"_test = [] {
    auto model = build_model(
        "1.  Open the file.\n"
        "\n"
        "2.  Find the following code block on line 21:\n"
        "\n"
        "        <html>\n"
        "          <head>\n"
        "            <title>Test</title>\n"
        "          </head>\n"
        "\n"
        "3.  Update the title to match the name of your website.\n");
    expect(fatal(bool((model.blocks.size()) == (1u))));
    if (model.blocks.empty()) return;
    auto const& list = model.blocks.front();
    expect(fatal(bool(list.kind == RenderBlockKind::Text)));
    auto code = find_render_block(list, RenderBlockKind::Code);
    expect(fatal(bool(code != nullptr)));
    if (code) {
        const auto code_indent = flow_indent_for(list, code->id);
        expect(fatal(bool(code_indent.has_value())));
        expect(fatal(bool(code->special().code_indented)));
        expect(fatal(bool((code->special().code_text) == (std::u32string(U"<html>\n  <head>\n    <title>Test</title>\n  </head>")))));
        expect(fatal(bool((code->source_span.source_range.end) == (code->special().raw_source.size()))));
        std::size_t line_indents = 0;
        std::size_t generated_headers = 0;
        std::u32string flattened_code;
        for (auto const& item : list.inline_items) {
            if (item.source_span.container_id != code->id) continue;
            if (item.kind == InlineRenderItem::Kind::Marker
                && item.special().marker_role == MarkerRole::Structural
                && item.special().display_text == U"\u200B\n") {
                ++generated_headers;
                expect(fatal(bool(item.source_span.source_range.empty())));
                expect(fatal(bool(item.source_span.source_range.start
                    == code->content_span.source_range.start)));
                continue;
            }
            if (item.special().marker_role == MarkerRole::Structural && !item.special().display_text.empty()
                && std::all_of(item.special().display_text.begin(), item.special().display_text.end(), [](char32_t value) { return value == U' '; })) {
                ++line_indents;
            } else if (item.kind == InlineRenderItem::Kind::Text && item.style.code == CodeStyle::Block) {
                flattened_code += item.text;
            }
        }
        // One indentation belongs to the presentation-only language header;
        // every other indentation belongs to a visible code row.
        expect(fatal(bool(generated_headers == 1u)));
        expect(fatal(bool(line_indents == code->special().line_count + 1u)));
        const auto presentation_lines = code_presentation_lines(code->special().code_text);
        const auto presentation_end = presentation_lines.empty()
            ? std::size_t{0}
            : presentation_lines.back().content_range.end;
        expect(fatal(bool(flattened_code == code->special().code_text.substr(0, presentation_end))));
        auto trailingBreak = std::find_if(list.inline_items.begin(), list.inline_items.end(), [&](auto const& item) {
            return item.special().marker_role == MarkerRole::Structural && item.text == U"\n"
                && item.source_span.container_id == code->id
                && item.source_span.source_range.start == code->content_span.source_range.end;
        });
        // The separator between sibling items is a generated boundary after
        // the final code row, not an editable blank paragraph inside the code.
        expect(fatal(bool(trailingBreak != list.inline_items.end())));
        if (trailingBreak != list.inline_items.end()) {
            expect(trailingBreak->source_span.source_range.empty());
        }
    }
    bool styled_code = false;
    for (auto const& item : list.inline_items) {
        if (item.kind == InlineRenderItem::Kind::Text && item.style.code == CodeStyle::Block) styled_code = true;
    }
    expect(fatal(bool(styled_code)));
};

"enter_at_nested_code_end_keeps_the_caret_on_list_content_indent"_test = [] {
    auto parsed = parse_text(1,
        "1. item\n"
        "\n"
        "        code\n"
        "\n"
        "2. next\n");
    BlockNode const* code = nullptr;
    walk_blocks(parsed.document.root, [&](BlockNode const& block) {
        if (!code && block.kind == BlockKind::CodeBlock) code = &block;
    });
    expect(fatal(bool(code != nullptr)));
    auto const content = code ? block_source_content(code->block_source) : std::u32string{};
    if (!code || content.empty() || content.back() != U'\n') return;
    auto const code_id = code->id;

    auto inserted = document_enter(parsed.document, TextSelection::caret({
        code_id,
        block_source_offset_for_content(code->block_source, content.size() - 1),
        TextAffinity::Downstream}));
    expect(fatal(bool(inserted.has_value())));
    if (!inserted) return;
    rebuild_document_block_index(parsed.document);
    expect(fatal(bool(inserted->selection_after.active.container_id == code_id)));
    auto model = build_render_model(
        parsed.document,
        Outline::empty(parsed.document.revision),
        build_document_symbol_index(parsed.document),
        default_theme_profile());
    expect(fatal(bool(model.blocks.size() == 1u)));
    if (model.blocks.empty()) return;
    auto caretIndent = std::find_if(model.blocks.front().inline_items.begin(), model.blocks.front().inline_items.end(), [&](auto const& item) {
        return item.special().marker_role == MarkerRole::Structural
            && item.source_span.container_id == inserted->selection_after.active.container_id
            && item.source_span.source_range.start == inserted->selection_after.active.source_offset
            && !item.special().display_text.empty()
            && std::all_of(item.special().display_text.begin(), item.special().display_text.end(), [](char32_t value) { return value == U' '; });
    });
    expect(fatal(bool(caretIndent != model.blocks.front().inline_items.end())));
};

"exiting_nested_code_maps_the_empty_paragraph_to_list_content_indent"_test = [] {
    auto parsed = parse_text(1,
        "1. item\n"
        "\n"
        "        code\n"
        "\n"
        "2. next\n");
    BlockNode const* code = nullptr;
    walk_blocks(parsed.document.root, [&](BlockNode const& block) {
        if (!code && block.kind == BlockKind::CodeBlock) code = &block;
    });
    expect(fatal(bool(code != nullptr)));
    if (!code) return;
    auto const code_id = code->id;

    auto exited = document_enter(parsed.document, TextSelection::caret({
        code_id,
        code->block_source.source().size(),
        TextAffinity::Downstream}));
    expect(fatal(bool(exited.has_value())));
    if (!exited) return;
    rebuild_document_block_index(parsed.document);
    expect(fatal(bool(exited->selection_after.active.container_id != code_id)));
    auto model = build_render_model(
        parsed.document,
        Outline::empty(parsed.document.revision),
        build_document_symbol_index(parsed.document),
        default_theme_profile());
    expect(fatal(bool(model.blocks.size() == 1u)));
    if (model.blocks.empty()) return;
    auto caretIndent = std::find_if(model.blocks.front().inline_items.begin(), model.blocks.front().inline_items.end(), [&](auto const& item) {
        return item.special().marker_role == MarkerRole::Structural
            && item.source_span.container_id == exited->selection_after.active.container_id
            && item.source_span.source_range.start == 0u
            && !item.special().display_text.empty()
            && std::all_of(item.special().display_text.begin(), item.special().display_text.end(), [](char32_t value) { return value == U' '; });
    });
    expect(fatal(bool(caretIndent != model.blocks.front().inline_items.end())));
};

};
