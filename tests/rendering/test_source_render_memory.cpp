#include "support/folia_test.hpp"

import folia.core.source_editor;
import folia.core.source_render;
import folia.core.source_style;
import folia.core.render_model;
import folia.core.block_layout;
import folia.core.text_measurer;
import folia.core.layout_tree;
import folia.core.hit_test;
import folia.core.types;
import folia.markdown.selection;

using namespace folia;
using namespace boost::ut;

suite source_render_memory_tests = [] {
    "source render memory does not allocate unused payloads per ordinary line"_test = [] {
        std::u32string source;
        for (std::size_t index = 0; index < 2048; ++index) source += U"ordinary source line\n";
        SourceEditor editor(std::move(source));
        auto model = build_source_render_model(editor);
        auto verify = [&] {
            expect(fatal(model.blocks.size() == editor.lines().size()));
            for (std::size_t index = 0; index < model.blocks.size(); ++index) {
                auto const& block = model.blocks[index];
                expect(!block.payload);
                expect(block.source_mode);
                std::u32string rendered;
                for (auto const& item : block.inline_items) {
                    expect(!item.payload);
                    rendered += item.text;
                }
                expect(rendered == editor.lines()[index].text);
            }
        };
        verify();
        editor.set_selection(SourceSelection::caret(editor.lines()[1024].source_start + 4));
        expect(fatal(editor.insert_text(U"X")));
        model = build_source_render_model_incremental(editor, std::move(model));
        expect(model.rebuilt_block_count == 1_u);
        verify();
    };

    "source render memory preserves styled text and source hit positions"_test = [] {
        SourceEditor editor(U"# Heading\r\n**bold** _italic_ [link](target) $x$ 中文 😀\r\n"
            U"```cpp\r\nreturn 1;\r\n```\r\nplain\r\n");
        auto model = build_source_render_model(editor);
        expect(fatal(model.blocks.size() == editor.lines().size()));
        bool bold = false, italic = false, link = false, math = false;
        // This deterministic measurer checks the portable source-coordinate
        // contract; native shaping remains covered by the DirectWrite tests.
        StubMeasurer measurer(8.0f);
        for (std::size_t index = 0; index < model.blocks.size(); ++index) {
            auto const& block = model.blocks[index];
            auto const& line = editor.lines()[index];
            expect(static_cast<bool>(block.payload) == line.code_language.has_value());
            expect(block.special().language == line.code_language);
            std::u32string rendered;
            for (auto const& item : block.inline_items) {
                expect(!item.payload);
                expect(item.kind == InlineRenderItem::Kind::Text);
                expect(item.source_span.container_id == line.id);
                expect(item.source_span.source_range.start == rendered.size());
                rendered += item.text;
                expect(item.source_span.source_range.end == rendered.size());
                bold = bold || item.style.bold;
                italic = italic || item.style.italic;
                link = link || item.style.link;
                math = math || item.source_syntax == SourceSyntaxKind::Math;
            }
            expect(rendered == line.text);
            auto [layout, height] = layout_text_block(block, 0, 2000, 1, measurer,
                std::nullopt, LogicalPoint{0, 0});
            expect(height > 0);
            for (auto const& child : layout.children) {
                expect(child.kind == LayoutItem::Kind::Line);
                for (auto const& run : child.line.runs) {
                    expect(run.marker_visibility == MarkerVisibility::Always);
                    for (std::size_t character = 0; character < run.text.size(); ++character) {
                        auto hit = hit_test_line(child.line,
                            LogicalPoint{run.origin.x + 8.0f * static_cast<float>(character) + 2.0f, 0}, line.id);
                        expect(fatal(hit.has_value()));
                        expect(hit->position.container_id == line.id);
                        expect(hit->position.source_offset == run.source_span.source_range.start + character);
                        expect(editor.source_offset_from_position(hit->position)
                            == line.source_start + run.source_span.source_range.start + character);
                    }
                }
            }
        }
        expect(bold);
        expect(italic);
        expect(link);
        expect(math);
    };
};
