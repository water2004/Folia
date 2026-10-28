#pragma once

#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Input.h>

namespace winrt::Folia::detail {
    // The SDK explicitly activates its preparation HWND with SetWindowPos;
    // neither DWM cloak nor WS_EX_NOACTIVATE vetoes that operation. Keep this
    // guard only for the lifetime of the preparation window. The hook is local
    // to this UI thread and only rejects activation/focus for marked HWNDs.
    class PreparedTabActivation {
    public:
        PreparedTabActivation() = default;
        PreparedTabActivation(PreparedTabActivation const&) = delete;
        PreparedTabActivation& operator=(PreparedTabActivation const&) = delete;
        ~PreparedTabActivation() { Reset(); }
        void Attach(HWND window) {
            Reset();
            hook_ = ::SetWindowsHookExW(WH_CBT, Callback, nullptr, ::GetCurrentThreadId());
            winrt::check_bool(hook_ != nullptr);
            window_ = window;
            if (!::SetPropW(window_, Property, this)) {
                auto error = ::GetLastError();
                Reset();
                winrt::throw_hresult(HRESULT_FROM_WIN32(error));
            }
        }
        void Reset() noexcept {
            if (window_) ::RemovePropW(window_, Property);
            if (hook_) ::UnhookWindowsHookEx(hook_);
            window_ = nullptr;
            hook_ = nullptr;
        }
    private:
        static constexpr auto Property = L"Folia.PreparedTabActivation";
        static LRESULT CALLBACK Callback(int code, WPARAM wparam, LPARAM lparam) noexcept {
            if (code == HCBT_ACTIVATE || code == HCBT_SETFOCUS) {
                auto root = ::GetAncestor(reinterpret_cast<HWND>(wparam), GA_ROOT);
                if (::GetPropW(root, Property)) return 1;
            }
            return ::CallNextHookEx(nullptr, code, wparam, lparam);
        }
        HWND window_ = nullptr;
        HHOOK hook_ = nullptr;
    };

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
