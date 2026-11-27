#pragma once

#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Windowing.h>

namespace app_tests {
    class MouseInput {
    public:
        explicit MouseInput(winrt::Microsoft::UI::Xaml::Window const& window)
            : presenter_(window.AppWindow().Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>()),
              wasTopmost_(presenter_.IsAlwaysOnTop()) {
            winrt::check_hresult(window.as<::IWindowNative>()->get_WindowHandle(&hwnd_));
            winrt::check_bool(::GetCursorPos(&previous_));
            presenter_.IsAlwaysOnTop(true);
            window.Activate();
        }
        ~MouseInput() {
            if (leftDown_) {
                INPUT up{};
                up.type = INPUT_MOUSE;
                up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                ::SendInput(1, &up, sizeof(INPUT));
            }
            ::SetCursorPos(previous_.x, previous_.y);
            presenter_.IsAlwaysOnTop(wasTopmost_);
        }
        MouseInput(MouseInput const&) = delete;
        MouseInput& operator=(MouseInput const&) = delete;

        static auto ScreenPoint(winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
                                winrt::Windows::Foundation::Point position) {
            auto root = element.XamlRoot();
            return root.CoordinateConverter().ConvertLocalToScreen(
                element.TransformToVisual(root.Content()).TransformPoint(position));
        }

        void Move(winrt::Microsoft::UI::Xaml::FrameworkElement const& element, winrt::Windows::Foundation::Point position) const {
            auto p = ScreenPoint(element, position);
            if (::GetAncestor(::WindowFromPoint({p.X, p.Y}), GA_ROOT) != hwnd_)
                throw std::runtime_error("mouse test window is obscured");
            // A cursor warp alone does not always deliver PointerMoved to a WinUI island.
            INPUT event{};
            event.type = INPUT_MOUSE;
            event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
            event.mi.dx = ::MulDiv(p.X - ::GetSystemMetrics(SM_XVIRTUALSCREEN), 65535, ::GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1);
            event.mi.dy = ::MulDiv(p.Y - ::GetSystemMetrics(SM_YVIRTUALSCREEN), 65535, ::GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1);
            winrt::check_bool(::SendInput(1, &event, sizeof(INPUT)) == 1);
        }

        void Wheel(int delta, unsigned count = 1, bool horizontal = false) const {
            std::vector<INPUT> input(count);
            for (auto& event : input) {
                event.type = INPUT_MOUSE;
                event.mi.dwFlags = horizontal ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
                event.mi.mouseData = static_cast<DWORD>(delta);
            }
            winrt::check_bool(::SendInput(count, input.data(), sizeof(INPUT)) == count);
        }

        void Left(bool down) {
            INPUT event{};
            event.type = INPUT_MOUSE;
            event.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            winrt::check_bool(::SendInput(1, &event, sizeof(INPUT)) == 1);
            leftDown_ = down;
        }
        void Click() { Left(true); Left(false); }

    private:
        winrt::Microsoft::UI::Windowing::OverlappedPresenter presenter_;
        bool wasTopmost_;
        bool leftDown_ = false;
        HWND hwnd_{};
        POINT previous_{};
    };
}
