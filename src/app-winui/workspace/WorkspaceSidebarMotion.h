#pragma once

#include <functional>
#include <initializer_list>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

namespace winrt::Folia {
    class WorkspaceSidebarMotion {
    public:
        void Attach(Microsoft::UI::Xaml::UIElement const& workspace,
            Microsoft::UI::Xaml::Controls::ColumnDefinition const& column,
            Microsoft::UI::Xaml::FrameworkElement const& pane,
            Microsoft::UI::Xaml::UIElement const& layout, Microsoft::UI::Xaml::UIElement const& document,
            std::function<void()> settled);
        void ClipTitle(Microsoft::UI::Xaml::UIElement const& frame, Microsoft::UI::Xaml::UIElement const& caption);
        void ClipReading(Microsoft::UI::Xaml::UIElement const& frame);
        void PinTrailing(std::initializer_list<Microsoft::UI::Xaml::UIElement> elements,
            Microsoft::UI::Xaml::UIElement const& leadingLabel);
        void SetOpen(bool open, bool animate);
        bool IsOpen() const noexcept { return open_; }
        void Stop();

    private:
        Microsoft::UI::Composition::ExpressionAnimation Expression(winrt::hstring const& code) const;
        void Translate(Microsoft::UI::Xaml::UIElement const& element, winrt::hstring const& code) const;
        void Animate();
        void Settle();
        void ReserveWidth(double width);
        void CancelCallbacks();

        Microsoft::UI::Xaml::Controls::ColumnDefinition column_{nullptr};
        Microsoft::UI::Xaml::FrameworkElement pane_{nullptr};
        Microsoft::UI::Composition::Compositor compositor_{nullptr};
        Microsoft::UI::Composition::CompositionPropertySet reveal_{nullptr};
        Microsoft::UI::Composition::CompositionPropertySet layout_{nullptr};
        Microsoft::UI::Composition::CompositionScopedBatch batch_{nullptr};
        winrt::event_token completed_{};
        winrt::event_token frame_{};
        std::function<void()> settled_;
        float width_ = 0;
        bool open_ = false;
    };
}
