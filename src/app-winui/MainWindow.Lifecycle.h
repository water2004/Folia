#pragma once

#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Input.h>

namespace winrt::Folia::detail {
    // This native source also requires a strong source/token subscription;
    // C++/WinRT auto_revoker attempts an unsupported weak-reference query.
    class TabDragCompletionSubscription {
    public:
        using Source = winrt::Microsoft::UI::Input::InputNonClientPointerSource;
        using Handler = winrt::Windows::Foundation::TypedEventHandler<Source,
            winrt::Microsoft::UI::Input::ExitedMoveSizeEventArgs>;
        TabDragCompletionSubscription() = default;
        TabDragCompletionSubscription(TabDragCompletionSubscription const&) = delete;
        TabDragCompletionSubscription& operator=(TabDragCompletionSubscription const&) = delete;
        ~TabDragCompletionSubscription() { Reset(); }
        void Attach(Source source, Handler const& handler) {
            Reset();
            token_ = source.ExitedMoveSize(handler);
            source_ = std::move(source);
        }
        void Reset() noexcept {
            if (source_) source_.ExitedMoveSize(token_);
            source_ = nullptr;
            token_ = {};
        }
    private:
        Source source_{nullptr};
        winrt::event_token token_{};
    };

    // AppWindow does not expose IWeakReferenceSource. Own its event subscription
    // explicitly instead of constructing C++/WinRT's weak auto_revoker.
    class WindowClosingSubscription {
    public:
        using Handler = winrt::Windows::Foundation::TypedEventHandler<
            winrt::Microsoft::UI::Windowing::AppWindow,
            winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs>;

        WindowClosingSubscription() = default;
        WindowClosingSubscription(WindowClosingSubscription const&) = delete;
        WindowClosingSubscription& operator=(WindowClosingSubscription const&) = delete;
        ~WindowClosingSubscription() { Reset(); }

        void Attach(winrt::Microsoft::UI::Windowing::AppWindow source, Handler const& handler) {
            Reset();
            auto token = source.Closing(handler);
            source_ = std::move(source);
            token_ = token;
        }
        void Reset() noexcept {
            if (!source_) return;
            source_.Closing(token_);
            source_ = nullptr;
            token_ = {};
        }

    private:
        winrt::Microsoft::UI::Windowing::AppWindow source_{nullptr};
        winrt::event_token token_{};
    };
}
