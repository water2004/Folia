#pragma once

#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace winrt::Folia {
    class DocumentTools {
    public:
        ~DocumentTools() { Stop(); }
        void Attach(Microsoft::UI::Xaml::Controls::Grid const& card,
                    Microsoft::UI::Xaml::Controls::Grid const& toolbar,
                    Microsoft::UI::Xaml::Controls::Grid const& viewport);
        void Configure(bool autoHide, bool available);
        void OnPointer(Windows::Foundation::Point point);
        void HoldOpen(bool hold);
        void Stop();

    private:
        void Reveal(bool show, bool animate);
        void UpdateClip();
        void ScheduleHide();
        bool HasKeyboardFocus() const;
        Microsoft::UI::Xaml::Controls::Grid card_{nullptr}, toolbar_{nullptr}, viewport_{nullptr};
        Microsoft::UI::Xaml::Media::TranslateTransform slide_{nullptr}, clipOffset_{nullptr};
        Microsoft::UI::Xaml::Media::RectangleGeometry clip_{nullptr};
        Microsoft::UI::Xaml::Media::Animation::Storyboard motion_{nullptr};
        Microsoft::UI::Dispatching::DispatcherQueueTimer hideTimer_{nullptr};
        Windows::Foundation::IInspectable pointerMoved_{nullptr};
        Microsoft::UI::Xaml::UIElement::PointerExited_revoker exit_;
        Microsoft::UI::Xaml::UIElement::GotFocus_revoker focus_;
        Microsoft::UI::Xaml::UIElement::LostFocus_revoker blur_;
        Microsoft::UI::Xaml::FrameworkElement::SizeChanged_revoker toolbarResized_, viewportResized_;
        winrt::event_token tick_{};
        bool autoHide_ = false, available_ = true, revealed_ = true;
        bool hovered_ = false, held_ = false;
    };
}
