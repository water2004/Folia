#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorContentPreparation.h"

import folia.markdown.instrumentation;

namespace {
using namespace boost::ut;
using namespace winrt::Folia;
suite nested_math_blocks = [] {
    "nested math retains block source markers highlighting and every caret boundary"_test = [] {
        for (auto input : {
            L"> before\n>\n> $$\n> \\frac{x}{2}+\U0001f600\n> $$\n>\n> after\n",
            L"> > $$\n> > \\frac{x}{2}\n> > $$\n",
            L"- before\n\n  $$\n  \\frac{x}{2}\n  $$\n\n  after\n",
            L"- [ ] before\n\n  $$\n  \\frac{x}{2}\n  $$\n",
            L"> [!NOTE]\n> $$\n> \\frac{x}{2}\n> $$\n",
            L"[^a]: before\n\n    $$\n    \\frac{x}{2}\n    $$\n\nuse [^a]\n",
            L"> $$\n> \\frac{x}{2}\n",
            L"> $$\n> \n> $$\n",
            L"> \\[\n> \\frac{x}{2}\n> \\]\n"}) {
            EditorSession session;
            session.SetText(input);
            auto before = session.TextUtf8();
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
            auto find = [&](auto& self, folia::RenderBlock const& block) -> folia::RenderBlock const* {
                if (block.kind == folia::RenderBlockKind::Math) return &block;
                for (auto const& child : block.child_blocks) if (auto result = self(self, child)) return result;
                return nullptr;
            };
            folia::RenderBlock const* mathBlock = nullptr;
            folia::RenderBlock const* root = nullptr;
            for (auto const& block : frame.renderModel.blocks) if (auto result = find(find, block)) {
                root = &block; mathBlock = result; break;
            }
            expect(fatal(mathBlock != nullptr));
            auto const& raw = mathBlock->special().raw_source;
            auto command = raw.find(U"\\frac");
            auto contentOffset = command == std::u32string::npos
                ? mathBlock->content_span.source_range.start : command + 1;
            MathJaxRenderer math;
            SvgNormalizer normalizer;
            TreeSitterHighlighter highlighter;
            for (auto offset : {std::size_t{0}, contentOffset, raw.size()}) {
                auto caret = folia::TextPosition{mathBlock->id, offset, folia::TextAffinity::Downstream};
                auto display = BuildDisplayInlineText(root->inline_items, caret,
                    InlineItemsEndPosition(root->inline_items, caret), math, normalizer,
                    {}, 18, 500, false, false, false);
                HighlightMathSources(display, caret, highlighter);
                expect(display.mathSources.size() == 1_u);
                expect(display.mathSources.front().toSource == mathBlock->special().content_to_source);
                std::size_t icons = 0;
                for (auto const& range : display.ranges) if (range.icon != SyntaxIcon::None) {
                    ++icons;
                    expect(range.iconPlacement == SyntaxIconPlacement::Block);
                }
                auto expectedIcons = std::ranges::count_if(mathBlock->special().source_markers,
                    [](auto const& marker) { return marker.icon != folia::SyntaxMarker::None; });
                expect(icons == static_cast<std::size_t>(expectedIcons));
                if (command != std::u32string::npos) expect(std::ranges::any_of(display.ranges, [](auto const& range) {
                    return range.syntax == SyntaxHighlightKind::Function;
                }));
                for (std::size_t i = 0; i < raw.size(); ++i) {
                    auto it = std::ranges::find_if(display.displayToSource, [&](auto const& mapped) {
                        return mapped.container_id == mathBlock->id && mapped.source_offset == i
                            && mapped.kind == EditorDisplayPositionKind::Source;
                    });
                    expect(it != display.displayToSource.end());
                }
            }
            auto caret = folia::TextPosition{mathBlock->id, contentOffset, folia::TextAffinity::Downstream};
            session.SetSelection(caret, caret);
            folia::reset_core_operation_counters();
            expect(session.ExecuteCommand(folia::Command::InsertText(U"z")));
            folia::Command undo; undo.kind = folia::CommandKind::Undo;
            expect(session.ExecuteCommand(undo));
            auto counts = folia::read_core_operation_counters();
            expect(session.TextUtf8() == before);
            expect(session.Selection().active == caret);
            expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
        }
    };
};
}
