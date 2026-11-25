#pragma once

#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

namespace winrt::Folia {
    // WinUI owns the pointer, reorder thresholds and window transfer. Move
    // only the tab's artwork so its hit rectangle remains in the native list.
    class TabDragPresentation {
    public:
        void Start(Microsoft::UI::Xaml::Controls::TabView const& tabs,
                   Windows::Foundation::IInspectable const& item, float grabOffset,
                   Microsoft::UI::Input::InputNonClientPointerSource const& pointer);
        void MoveTo(Windows::Graphics::PointInt32 point);
        void Stop(bool settle = false);
        ~TabDragPresentation() { Stop(); }
    private:
        void ReleaseContainer(bool settle);
        Microsoft::UI::Xaml::Controls::TabView tabs_{nullptr};
        Windows::Foundation::IInspectable item_{nullptr};
        Microsoft::UI::Xaml::Controls::TabViewItem container_{nullptr};
        Microsoft::UI::Xaml::Media::TranslateTransform slide_{nullptr};
        Microsoft::UI::Input::InputNonClientPointerSource pointer_{nullptr};
        winrt::event_token move_{};
        float grabOffset_ = 0;
    };
}
