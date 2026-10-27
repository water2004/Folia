#pragma once

#include "editor/interaction/EditorFrameScheduler.h"
#include "editor/interaction/EditorScrollController.h"

namespace winrt::Folia {
    // One display-paced clock for editor motion; a one-shot wake for idle blink.
    class EditorAnimationController {
    public:
        ~EditorAnimationController();
        void Attach(EditorSurfaceRenderer& renderer, EditorScrollController& scroll,
            Microsoft::UI::Dispatching::DispatcherQueue const& queue, HWND hwnd, std::function<void()> render);
        void Detach();
        void RequestFrame();
        void PrepareRender();
        void Rendered();
        void Focus(bool focused);
    private:
        struct Lifetime { EditorAnimationController* owner = nullptr; };
        std::shared_ptr<Lifetime> lifetime;
        EditorFrameScheduler scheduler;
        Microsoft::UI::Dispatching::DispatcherQueueTimer wake{nullptr};
        Windows::UI::ViewManagement::UISettings uiSettings{nullptr};
        EditorSurfaceRenderer* renderer = nullptr;
        EditorScrollController* scroll = nullptr;
    };
}
