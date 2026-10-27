#include "pch.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorPreparedDocument.h"

namespace winrt::Folia
{
    EditorSurfaceRenderer::EditorSurfaceRenderer() = default;

    EditorSurfaceRenderer::~EditorSurfaceRenderer()
    {
        renderCache.Detach();
        mathJax.SetCompletionCallback({});
        svgNormalizer.SetCompletionCallback({});
        mermaid.SetCompletionCallback({});
        std::scoped_lock lock(invalidationState->mutex);
        invalidationState->active = false;
        invalidationState->callback = {};
    }

    void EditorSurfaceRenderer::SetTheme(folia::ThemeProfile const& value)
    {
        themeProfile = value;
        ++themeRevision;
        styleSheet = CreateEditorStyleSheet(themeProfile);
        renderCache.ClearTextLayouts();
        renderCache.ClearSvgDrawings();
        resources.RebuildTextFormats(styleSheet);
        resources.ResetBrushes();
        ClearPreparedDocument();
    }

    folia::ThemeProfile EditorSurfaceRenderer::Theme() const
    {
        return themeProfile;
    }

    void EditorSurfaceRenderer::ResetDocumentCaches()
    {
        caretAnimation.Reset();
        mathJax.Clear();
        AttachMathCompletion();
        mathModelRevision.reset();
        treeSitter.Clear();
        renderCache.ClearTextLayouts();
        renderCache.ClearSvgDrawings();
        ClearPreparedDocument();
    }

    void EditorSurfaceRenderer::ClearPreparedDocument()
    {
        inlineMathPreviewBounds.reset();
        preparedDocument.reset();
    }

    MathJaxRenderer EditorSurfaceRenderer::TakeMathCache()
    {
        mathJax.SetCompletionCallback({});
        mathJax.SetVisibleSvgIds({});
        mathJax.CancelOffscreenRenders({});
        auto cache = std::move(mathJax);
        ResetDocumentCaches();
        interactionMap.Clear();
        nonInteractiveRegions.clear();
        ClearPointer();
        SetTableDrag({}, {});
        return cache;
    }

    void EditorSurfaceRenderer::RestoreMathCache(MathJaxRenderer cache)
    {
        ResetDocumentCaches();
        mathJax = std::move(cache);
        mathJax.SetPreparationStopToken(mathPreparationStop);
        AttachMathCompletion();
    }

    void EditorSurfaceRenderer::SetInvalidateCallback(std::function<void()> callback)
    {
        std::scoped_lock lock(invalidationState->mutex);
        invalidationState->callback = std::move(callback);
    }

    void EditorSurfaceRenderer::AttachMathCompletion()
    {
        mathJax.SetCompletionCallback(EmbeddedCompletionCallback());
    }

    std::function<void()> EditorSurfaceRenderer::EmbeddedCompletionCallback()
    {
        if (!mathDispatcher) return {};
        std::weak_ptr<InvalidationState> weak = invalidationState;
        return [weak, dispatcher = mathDispatcher, this]
        {
            auto state = weak.lock();
            if (!state) return;
            {
                std::scoped_lock lock(state->mutex);
                if (!state->active || state->embeddedQueued) return;
                state->embeddedQueued = true;
            }
            if (!dispatcher.TryEnqueue(
                winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                [weak, this]
            {
                auto current = weak.lock();
                if (!current) return;
                {
                    std::scoped_lock lock(current->mutex);
                    current->embeddedQueued = false;
                    if (!current->active) return;
                }
                ++embeddedGeneration;
                Invalidate();
            }))
            {
                std::scoped_lock lock(state->mutex);
                state->embeddedQueued = false;
            }
        };
    }

    void EditorSurfaceRenderer::Invalidate()
    {
        if (rendering || exporting) { deferredInvalidate = true; return; }
        if (!mathDispatcher) return; // Offscreen exporters have no UI to repaint.
        auto state = invalidationState;
        {
            std::scoped_lock lock(state->mutex);
            if (!state->active || !state->callback || state->repaintQueued) return;
            state->repaintQueued = true;
        }
        // A frame budget must yield to input, including the scrollbar release.
        // Never resume drawing recursively from a Render scope destructor.
        if (!mathDispatcher.TryEnqueue(
            winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
            [weak = std::weak_ptr<InvalidationState>{state}]
        {
            auto current = weak.lock();
            if (!current) return;
            std::function<void()> callback;
            {
                std::scoped_lock lock(current->mutex);
                current->repaintQueued = false;
                if (current->active) callback = current->callback;
            }
            if (callback) callback();
        }))
        {
            std::scoped_lock lock(state->mutex);
            state->repaintQueued = false;
        }
    }

    void EditorSurfaceRenderer::Initialize(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel)
    {
        if (resources.Ready()) return;
        resources.Initialize(panel, styleSheet);
        auto dispatcher = panel.DispatcherQueue();
        mathDispatcher = dispatcher;
        AttachMathCompletion();
        renderCache.Attach(dispatcher, [this] { Invalidate(); });
        auto completion = EmbeddedCompletionCallback();
        svgNormalizer.SetCompletionCallback(completion);
        mermaid.SetCompletionCallback(std::move(completion));
    }

    void EditorSurfaceRenderer::InitializeForPdf()
    {
        if (resources.Ready()) return;
        constexpr float pageWidth = 210.0f / 25.4f * 96.0f;
        constexpr float pageHeight = 297.0f / 25.4f * 96.0f;
        constexpr float pageMargin = 48.0f;
        resources.InitializeOffscreen(
            styleSheet,
            pageWidth - pageMargin * 2.0f,
            pageHeight - pageMargin * 2.0f);
        winrt::Microsoft::UI::Dispatching::DispatcherQueue noDispatcher{nullptr};
        renderCache.Attach(noDispatcher, {});
    }

    void EditorSurfaceRenderer::Resize(
        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel,
        double width,
        double height)
    {
        if (resizing) return;
        resizing = true;
        struct ResetFlag { bool& value; ~ResetFlag() { value = false; } } reset{resizing};
        auto result = resources.Resize(panel, width, height);
        if (!result.resized) return;
        if (result.widthChanged)
        {
            renderCache.ClearTextLayouts();
            ClearPreparedDocument();
        }
        scrollState.Clamp(MaximumScrollOffset());
    }

    void EditorSurfaceRenderer::Render(detail::EditorRenderFrame const& frame)
    {
        if (!resources.Ready()) { caretAnimation.Reset(); return; }
        if (resizing || rendering)
        {
            // A source edit may request a frame while an embedded-content
            // invalidation is painting. Never drop that newer frame: the
            // deferred callback obtains the latest session model after this
            // render completes.
            deferredInvalidate = true;
            return;
        }
        rendering = true;
        struct Reset { EditorSurfaceRenderer& owner; ~Reset() { owner.rendering = false; if (owner.deferredInvalidate.exchange(false)) owner.Invalidate(); } } reset{*this};
        resources.EnsureFrameResources(styleSheet);
        resources.d2dContext->BeginDraw();
        resources.d2dContext->Clear(styleSheet.canvasColor);
        try
        {
            DrawDocument(frame);
        }
        catch (...)
        {
            // Synchronous native/file preparation can report an error. Close
            // the drawing transaction so the next frame can still render.
            resources.d2dContext->EndDraw();
            caretAnimation.Reset();
            throw;
        }
        auto ended = resources.d2dContext->EndDraw();
        if (ended == D2DERR_RECREATE_TARGET) { caretAnimation.Reset(); resources.ResetTargets(); return; }
        if (FAILED(ended)) { caretAnimation.Reset(); return; }
        auto presented = resources.swapChain->Present(0, 0);
        if (presented == DXGI_ERROR_DEVICE_REMOVED || presented == DXGI_ERROR_DEVICE_RESET) {
            caretAnimation.Reset();
            resources.ResetTargets();
        }
    }
}
