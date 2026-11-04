#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <msctf.h>
#pragma comment(lib, "uuid.lib")

namespace winrt::Folia {
    struct EditorTextInputTestAccess {
        static bool Focused(EditorTextInputController const& input) { return input.focused_; }
    };
}
namespace winrt::Folia::implementation {
    struct TextInputWindowTestAccess {
        static auto Impl(Microsoft::UI::Xaml::Window const& window) {
            return get_self<MainWindow>(window.as<winrt::Folia::MainWindow>());
        }
        static auto& Input(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->textInputController; }
        static auto Surface(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->EditorSurface(); }
        static void Close(Microsoft::UI::Xaml::Window const& window) { Impl(window)->closeApproved = true; window.Close(); }
    };
}
namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Microsoft::UI::Xaml;
    using Access = winrt::Folia::implementation::TextInputWindowTestAccess;
    using InputAccess = winrt::Folia::EditorTextInputTestAccess;
    struct TextWindow {
        Window window = winrt::Folia::implementation::App::Current()->CreateMainWindow();
        TextWindow() {
            window.Activate();
            Pump([&] { return window.Content().as<FrameworkElement>().IsLoaded(); });
        }
        ~TextWindow() { Access::Close(window); }
        void Activate() {
            window.Activate();
            auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
            Pump([&] { return std::chrono::steady_clock::now() >= until; });
        }
        void Focus() {
            Activate();
            expect(Access::Surface(window).Focus(FocusState::Programmatic));
            auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
            Pump([&] { return std::chrono::steady_clock::now() >= until; });
        }
        auto& Input() { return Access::Input(window); }
    };
    auto ThreadFocus() {
        winrt::com_ptr<ITfThreadMgr> manager;
        winrt::check_hresult(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER,
            __uuidof(ITfThreadMgr), manager.put_void()));
        winrt::com_ptr<ITfDocumentMgr> document;
        winrt::check_hresult(manager->GetFocus(document.put()));
        return document;
    }
    suite text_input_focus_tests = [] {
        "a background editor focus notification cannot replace the active window text context"_test = [] {
            TextWindow first, second;
            first.Focus(); first.Input().FocusEnter();
            second.Focus(); second.Input().FocusEnter();
            auto expected = ThreadFocus(); expect(fatal(bool(expected)));
            first.Input().FocusEnter();
            auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
            Pump([&] { return std::chrono::steady_clock::now() >= until; });
            expect(!InputAccess::Focused(first.Input()));
            expect(ThreadFocus() == expected);
        };
        "returning between two real windows restores each editor text services focus"_test = [] {
            TextWindow first, second;
            first.Focus(); first.Input().FocusEnter();
            auto firstContext = ThreadFocus(); expect(fatal(bool(firstContext)));
            second.Focus(); second.Input().FocusEnter();
            auto secondContext = ThreadFocus(); expect(fatal(bool(secondContext)));
            expect(firstContext != secondContext);
            for (int i = 0; i < 6; ++i) {
                first.Activate();
                expect(InputAccess::Focused(first.Input()));
                expect(!InputAccess::Focused(second.Input()));
                expect(ThreadFocus() == firstContext);
                second.Activate();
                expect(InputAccess::Focused(second.Input()));
                expect(!InputAccess::Focused(first.Input()));
                expect(ThreadFocus() == secondContext);
            }
        };
        "queued activation cannot restore the old window after a rapid switch"_test = [] {
            TextWindow first, second;
            first.Focus(); first.Input().FocusEnter();
            second.Focus(); second.Input().FocusEnter();
            auto expected = ThreadFocus(); expect(fatal(bool(expected)));
            first.window.Activate(); second.window.Activate();
            first.Input().FocusEnter();
            auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
            Pump([&] { return std::chrono::steady_clock::now() >= until; });
            expect(!InputAccess::Focused(first.Input()));
            expect(InputAccess::Focused(second.Input()));
            expect(ThreadFocus() == expected);
        };
        "queued window activation respects focus moved to a native text control"_test = [] {
            TextWindow window; window.Focus(); window.Input().FocusEnter();
            auto editor = ThreadFocus(); expect(fatal(bool(editor)));
            Controls::TextBox field;
            window.window.Content().as<Controls::Grid>().Children().Append(field);
            window.window.Activate();
            expect(field.Focus(FocusState::Programmatic));
            auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
            Pump([&] { return std::chrono::steady_clock::now() >= until; });
            expect(!InputAccess::Focused(window.Input()));
            expect(ThreadFocus() != editor);
            expect(field.FocusState() != FocusState::Unfocused);
        };
    };
}
