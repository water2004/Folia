#pragma once

#include "editor/rendering/EditorSurfaceRenderer.h"

namespace winrt::Folia
{
    struct EditorScrollController
    {
        ~EditorScrollController();
        void Attach(
            EditorSurfaceRenderer& renderer,
            winrt::Microsoft::UI::Xaml::Controls::Primitives::ScrollBar const& scrollBar,
            winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition const& column,
            std::function<void()> requestFrame);
        void Detach();
        void Sync();
        void SetWidth(float value);
        void QueueScrollBy(float delta);
        void ScrollPreciselyBy(float delta);
        void BeginSelectionAutoScroll(float velocityPixelsPerSecond, std::function<void()> updateSelection);
        void EndSelectionAutoScroll();
        void Stop();
        bool AdvanceFrame(float elapsedSeconds);

    private:
        void Start();

        EditorSurfaceRenderer* renderer = nullptr;
        winrt::Microsoft::UI::Xaml::Controls::Primitives::ScrollBar scrollBar{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition column{ nullptr };
        std::function<void()> requestFrame;
        winrt::event_token valueChangedToken{};
        winrt::event_token scrollToken{};
        bool attached = false;
        bool synchronizing = false;
        bool scrollBarDragging = false;
        float width = 16.0f;
        float selectionAutoScrollVelocity = 0.0f;
        std::function<void()> updateSelectionDuringAutoScroll;
    };
}
