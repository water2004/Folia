#pragma once

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>

namespace winrt::Folia {
    // A pinned bar participates in Grid layout. An auto-hidden bar occupies
    // the same edge as an overlay, so hover never resizes the editor surface.
    class WorkspaceEdgeBar {
    public:
        enum class Edge { Top, Bottom };
        ~WorkspaceEdgeBar() { Stop(); }
        void Attach(Microsoft::UI::Xaml::Controls::Grid const& root, Microsoft::UI::Xaml::Controls::Grid const& bar,
                    Microsoft::UI::Xaml::Controls::Grid const& viewport, Edge edge, Microsoft::UI::WindowId window,
                    std::function<void(bool)> visibilityChanged = {});
        void Configure(bool autoHide, bool available = true);
        void OnPointer(Windows::Foundation::Point point);
        void HoldOpen(bool hold);
        void SetTabDragActive(bool active);
        void SetCoveredWidth(double width) { coveredWidth_ = width; }
        void Stop();

    private:
        void Reveal(bool show, bool animate);
        void ScheduleHide();
        void UpdateViewportClip();
        bool HasKeyboardFocus() const;
        Microsoft::UI::Xaml::Controls::Grid bar_{nullptr};
        Microsoft::UI::Xaml::Controls::Grid root_{nullptr};
        Microsoft::UI::Xaml::Controls::Grid viewport_{nullptr};
        Microsoft::UI::Xaml::Media::RectangleGeometry clip_{nullptr};
        Windows::Foundation::IInspectable pointerMoved_{nullptr};
        Microsoft::UI::Xaml::Media::TranslateTransform slide_{nullptr};
        Microsoft::UI::Xaml::Media::Animation::Storyboard animation_{nullptr};
        Microsoft::UI::Dispatching::DispatcherQueueTimer hideTimer_{nullptr};
        Microsoft::UI::Input::InputNonClientPointerSource nativePointer_{nullptr};
        winrt::event_token nativeEnter_{}, nativeMove_{}, nativeExit_{};
        winrt::event_token hideTick_{};
        Microsoft::UI::Xaml::UIElement::PointerExited_revoker rootExit_;
        Microsoft::UI::Xaml::UIElement::GotFocus_revoker focus_;
        Microsoft::UI::Xaml::UIElement::LostFocus_revoker blur_;
        Microsoft::UI::Xaml::FrameworkElement::SizeChanged_revoker resized_;
        Microsoft::UI::Xaml::FrameworkElement::SizeChanged_revoker viewportResized_;
        std::function<void(bool)> visibilityChanged_;
        Edge edge_ = Edge::Top;
        double coveredWidth_ = 0;
        bool tabDragActive_ = false;
        bool autoHide_ = false, available_ = true, revealed_ = true, held_ = false;
        bool pointerOver_ = false;
    };
}
