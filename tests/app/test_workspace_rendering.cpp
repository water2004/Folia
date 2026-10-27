#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorContentGeometry.h"
#include "editor/rendering/EditorContentPreparation.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

import folia.markdown.instrumentation;

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;

    suite workspace_rendering_tests = [] {
        "workspace rendering transfers formula geometry and releases inactive SVG demand"_test = [] {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession first, second;
            first.SetText(L"First $x+1$ and $x+2$.");
            second.SetText(L"Second $y+3$.");
            const auto source = first.TextUtf8();
            const auto selection = first.Selection();
            EditorSurfaceRenderer renderer;
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            const auto context = renderer.CaptureMathPreparationContext();
            renderer.AdoptMathPreparation(EditorSurfaceRenderer::PrepareMath(first.RenderFrame(), context, {}, {}));
            renderer.Render(first.RenderFrame());
            auto firstCache = renderer.TakeMathCache();
            firstCache.WaitForIdle();
            auto original = firstCache.Snapshot();
            expect(original.entries == 2_u);
            expect(original.residentSvgBytes == 0_u);
            expect(original.queuedReads == 0_u);
            expect(original.previewSvgBytes == 0_u);
            renderer.AdoptMathPreparation(EditorSurfaceRenderer::PrepareMath(second.RenderFrame(), context, {}, {}));
            renderer.Render(second.RenderFrame());
            auto secondCache = renderer.TakeMathCache();
            secondCache.WaitForIdle();
            expect(secondCache.Snapshot().backingPath != original.backingPath);
            folia::reset_core_operation_counters();
            renderer.RestoreMathCache(std::move(firstCache));
            renderer.Render(first.RenderFrame());
            firstCache = renderer.TakeMathCache();
            firstCache.WaitForIdle();
            auto restored = firstCache.Snapshot();
            expect(restored.backingPath == original.backingPath);
            expect(restored.nativeRenders == original.nativeRenders);
            expect(restored.fileWrites == original.fileWrites);
            expect(restored.residentSvgBytes == 0_u);
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(first.Selection() == selection);
            expect(first.TextUtf8() == source);
        };
    };
}
