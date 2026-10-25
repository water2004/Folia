#pragma once

#include <winrt/Microsoft.UI.Windowing.h>

namespace winrt::Folia::detail {
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
