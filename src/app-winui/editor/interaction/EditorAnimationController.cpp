#include "pch.h"
import std;
#include "editor/interaction/EditorAnimationController.h"

namespace winrt::Folia {
    EditorAnimationController::~EditorAnimationController() { Detach(); }
    void EditorAnimationController::Attach(EditorSurfaceRenderer& value, EditorScrollController& scrollValue,
        Microsoft::UI::Dispatching::DispatcherQueue const& queue, HWND hwnd, std::function<void()> render) {
        Detach(); renderer = &value; scroll = &scrollValue;
        renderer->FocusCaret(false);
        uiSettings = Windows::UI::ViewManagement::UISettings();
        lifetime = std::make_shared<Lifetime>(); lifetime->owner = this;
        auto weak = std::weak_ptr{lifetime};
        wake = queue.CreateTimer(); wake.IsRepeating(false);
        wake.Tick([weak](auto const&, auto const&) {
            if (auto current = weak.lock(); current && current->owner) current->owner->RequestFrame();
        });
        scheduler.Attach(queue, value.FrameLatencyWaitableObject(), hwnd,
            [weak, render = std::move(render)](float elapsed) {
                auto current = weak.lock();
                if (!current || !current->owner) return false;
                auto& owner = *current->owner;
                auto scrolling = owner.scroll->AdvanceFrame(elapsed);
                render();
                return scrolling || owner.renderer->CaretAnimationFrame().animating;
            });
    }
    void EditorAnimationController::Detach() {
        if (lifetime) lifetime->owner = nullptr;
        if (wake) wake.Stop();
        scheduler.Detach(); wake = nullptr; uiSettings = nullptr; lifetime.reset(); renderer = nullptr; scroll = nullptr;
    }
    void EditorAnimationController::RequestFrame() { scheduler.Start(); }
    void EditorAnimationController::PrepareRender() {
        if (!renderer || !uiSettings) return;
        auto blink = ::GetCaretBlinkTime();
        renderer->ConfigureCaret(uiSettings.AnimationsEnabled(), blink == INFINITE || blink == 0 ? 0 : blink / 1000.0);
    }
    void EditorAnimationController::Rendered() {
        if (!renderer || !wake) return;
        wake.Stop();
        auto const& frame = renderer->CaretAnimationFrame();
        if (frame.animating) RequestFrame();
        else if (std::isfinite(frame.wakeAfter)) {
            wake.Interval(std::chrono::duration_cast<Windows::Foundation::TimeSpan>(
                std::chrono::duration<double>((std::max)(.001, frame.wakeAfter))));
            wake.Start();
        }
    }
    void EditorAnimationController::Focus(bool focused) {
        if (!renderer) return;
        renderer->FocusCaret(focused);
        RequestFrame();
    }
}
