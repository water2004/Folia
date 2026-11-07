#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorPreparedDocument.h"

import folia.markdown.instrumentation;

namespace winrt::Folia {
struct EditorRendererTestAccess {
    static auto Layout(EditorSurfaceRenderer& renderer) {
        return renderer.preparedDocument->blocks.front().layout;
    }
    static auto PreparedIdentity(EditorSurfaceRenderer& renderer) {
        auto const& block = renderer.preparedDocument->blocks.front();
        return std::tuple{block.layout.Get(), block.layoutLines.get(), block.paintPlan.get()};
    }
    static void CheckVisibleGlyphs(EditorSurfaceRenderer& renderer) {
        auto const& block = renderer.preparedDocument->blocks.front();
        folia::platform::editor::EditorTextPaintStatistics stats;
        auto& resources = renderer.resources;
        resources.d2dContext->BeginDraw();
        winrt::check_hresult(block.paintPlan->Draw(resources.d2dContext.Get(), {0, 0},
            resources.textBrush.Get(), {0, 0, 1280, 800}, &stats));
        winrt::check_hresult(resources.d2dContext->EndDraw());
        boost::ut::expect(stats.paintedGlyphs < std::size_t{10000});
        boost::ut::expect(stats.paintedGlyphs > std::size_t{0});
        std::fprintf(stderr, "long paragraph visible glyphs: %zu of %zu runs\n",
            stats.paintedGlyphs, stats.visitedRuns);
    }
};
}

namespace {
using namespace boost::ut;
using namespace winrt::Folia;
void CheckLongParagraph(std::size_t bytes) {
        std::fprintf(stderr, "long paragraph fixture bytes: %zu\n", bytes);
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
        EditorSession session;
        std::string source;
        while (source.size() < bytes)
            source += "Sample markdown list item for parser and upload testing.\n";
        session.SetText(winrt::to_hstring(source));
        EditorSurfaceRenderer renderer;
        renderer.Initialize(panel);
        renderer.Resize(panel, 1280, 800);
        auto context = renderer.CaptureMathPreparationContext();
        renderer.AdoptMathPreparation(EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {}));
        auto measure = [&](char const* name, auto action) {
            auto start = std::chrono::steady_clock::now();
            action();
            std::fprintf(stderr, "long paragraph %s: %.3f ms\n", name,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        };
        measure("first frame", [&] { renderer.Render(session.RenderFrame()); });
        auto identity = EditorRendererTestAccess::PreparedIdentity(renderer);
        EditorRendererTestAccess::CheckVisibleGlyphs(renderer);
        folia::reset_core_operation_counters();
        for (int i = 0; i < 3; ++i)
            measure("warm frame", [&] { renderer.Render(session.RenderFrame()); });
        for (int i = 0; i < 3; ++i) {
            renderer.SetScrollOffset(100.f * i);
            measure("scroll frame", [&] { renderer.Render(session.RenderFrame()); });
        }
        auto caret = session.Selection().active;
        for (int i = 0; i < 3; ++i) {
            ++caret.source_offset;
            session.SetSelection(folia::TextSelection::caret(caret));
            measure("caret frame", [&] { renderer.Render(session.RenderFrame()); });
        }
        auto anchor = caret;
        caret.source_offset += 10;
        session.SetSelection(anchor, caret);
        measure("selection frame", [&] { renderer.Render(session.RenderFrame()); });
        expect(EditorRendererTestAccess::PreparedIdentity(renderer) == identity);
        auto counters = folia::read_core_operation_counters();
        expect(counters.inline_scanned_characters == 0_u);
        expect(counters.full_document_parses == 0_u);
        expect(counters.full_document_serializations == 0_u);
        expect(counters.inline_reparses == 0_u);
        expect(session.TextUtf8() == source);
        session.SetSelection(folia::TextSelection::caret(caret));
        folia::reset_core_operation_counters();
        measure("type command", [&] { expect(session.ExecuteCommand(folia::Command::InsertText(U"x"))); });
        measure("edited frame", [&] { renderer.Render(session.RenderFrame()); });
        measure("undo command", [&] { expect(session.ExecuteCommand(folia::Command{folia::CommandKind::Undo})); });
        measure("undo frame", [&] { renderer.Render(session.RenderFrame()); });
        counters = folia::read_core_operation_counters();
        expect(counters.inline_scanned_characters < 256_u);
        expect(counters.inline_reparses == 2_u);
        expect(counters.full_document_parses == 0_u);
        expect(counters.full_document_serializations == 0_u);
        expect(session.TextUtf8() == source);
        expect(session.Selection() == folia::TextSelection::caret(caret));
}
suite large_document_interaction_tests = [] {
    "caret presentation invalidates layout only when CST presentation changes"_test = [] {
        for (auto text : {L"left **bold** right", L"> left **bold** right", L"- left **bold** right",
            L"left [title](url) right", L"left ![alt](absent.png) right"}) {
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession session; session.SetText(text);
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
            std::optional<folia::TextSpan> span;
            auto find = [&](auto const& self, folia::RenderBlock const& block) -> void {
                for (auto const& item : block.inline_items) {
                    if (item.kind == folia::InlineRenderItem::Kind::Marker && item.special().marker_construct)
                        span = folia::TextSpan{item.source_span.container_id, *item.special().marker_construct};
                    if (item.kind == folia::InlineRenderItem::Kind::Link || item.kind == folia::InlineRenderItem::Kind::Image)
                        span = item.source_span;
                }
                for (auto const& child : block.child_blocks) self(self, child);
            };
            find(find, frame.renderModel.blocks.front());
            expect(fatal(span.has_value()));
            auto select = [&](std::size_t offset) {
                session.SetSelection(folia::TextSelection::caret({span->container_id, offset, folia::TextAffinity::Downstream}));
            };
            select(0);
            EditorSurfaceRenderer renderer; renderer.Initialize(panel); renderer.Resize(panel, 1280, 800);
            renderer.Render(session.RenderFrame());
            auto initial = EditorRendererTestAccess::Layout(renderer);
            expect(fatal(static_cast<bool>(initial)));
            select(1); renderer.Render(session.RenderFrame());
            expect(EditorRendererTestAccess::Layout(renderer).Get() == initial.Get());
            select(span->source_range.start + 1); renderer.Render(session.RenderFrame());
            auto expanded = EditorRendererTestAccess::Layout(renderer);
            expect(expanded.Get() != initial.Get());
            select(span->source_range.start + 2); renderer.Render(session.RenderFrame());
            expect(EditorRendererTestAccess::Layout(renderer).Get() == expanded.Get());
            select(0); renderer.Render(session.RenderFrame());
            expect(EditorRendererTestAccess::Layout(renderer).Get() != expanded.Get());
        }
    };

    "one megabyte paragraph reuses layout through scrolling caret and selection"_test = [] { CheckLongParagraph(1024 * 1024); };
    "ten megabyte paragraph reuses layout through scrolling caret and selection"_test = [] { CheckLongParagraph(10 * 1024 * 1024); };
};
}
