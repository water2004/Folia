#include "pch.h"
import std;
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <winrt/Microsoft.UI.Interop.h>

namespace winrt::Folia::implementation {
    App* App::Current() {
        return winrt::get_self<App>(Microsoft::UI::Xaml::Application::Current()
            .as<Microsoft::UI::Xaml::Markup::IXamlMetadataProvider>());
    }
    Microsoft::UI::Xaml::Window App::CreateMainWindow(winrt::hstring const& path) {
        auto window = winrt::make<MainWindow>(path);
        if (!tabMouseHook) {
            tabMouseHook.reset(::SetWindowsHookExW(WH_MOUSE, TabMouseHook, nullptr, ::GetCurrentThreadId()));
            winrt::check_bool(static_cast<bool>(tabMouseHook));
        }
        auto id = window.AppWindow().Id();
        windows.push_back({id, window});
        window.Closed([weak = get_weak(), id](auto const&, auto const&) {
            if (auto app = weak.get()) app->OnWindowClosed(id);
        });
        return window;
    }

    LRESULT CALLBACK App::TabMouseHook(int code, WPARAM message, LPARAM data) noexcept {
        if (code == HC_ACTION) {
            try {
                if (RouteTabMiddleButton(message, *reinterpret_cast<MOUSEHOOKSTRUCT const*>(data))) return 1;
            } catch (...) {
                // Preserve normal message delivery if dispatch fails.
            }
        }
        return ::CallNextHookEx(nullptr, code, message, data);
    }

    bool App::RouteTabMiddleButton(WPARAM message, MOUSEHOOKSTRUCT const& mouse) {
        const bool release = message == WM_MBUTTONUP || message == WM_NCMBUTTONUP;
        const bool captionPress = mouse.wHitTestCode == HTCAPTION
            && (message == WM_NCMBUTTONDOWN || message == WM_NCMBUTTONDBLCLK);
        if (!release && !captionPress) return false;

        // WinUI dispatches caption input to a child HWND of the main window.
        auto window = ::GetAncestor(mouse.hwnd, GA_ROOT);
        if (!window) return false;
        auto owner = Current()->FindMainWindow(Microsoft::UI::GetWindowIdFromWindow(window));
        if (!owner) return false;
        auto target = owner->get_strong();
        const Windows::Graphics::PointInt32 point{mouse.pt.x, mouse.pt.y};
        return release ? target->ReleaseTabMiddlePress(point) : target->BeginTabMiddlePress(point);
    }

    void App::OnWindowClosed(Microsoft::UI::WindowId id) {
        std::erase_if(windows, [id](auto const& item) { return item.id == id; });
        if (closingPreparedWindows) return;
        // WinUI may prepare a cloaked tear-out host without completing a drag.
        // It belongs to its source until it receives a document and is revealed.
        std::vector<Microsoft::UI::Xaml::Window> unused;
        for (auto const& item : windows) {
            auto target = winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
            if (!target->preparedTabWindow) continue;
            auto owner = target->preparedTabOwner.get();
            if (!owner || owner->windowClosed) unused.push_back(item.window);
        }
        closingPreparedWindows = true;
        for (auto const& window : unused) {
            auto target = winrt::get_self<MainWindow>(window.as<winrt::Folia::MainWindow>());
            target->closeApproved = true;
            window.Close();
        }
        closingPreparedWindows = false;
        if (windows.empty()) {
            tabMouseHook.reset();
            activationSubscription.revoke();
            if (activationInstance && activationInstance.IsCurrent()) activationInstance.UnregisterKey();
            Exit();
        }
    }
    void App::OpenWorkspaceWindows(winrt::Folia::WorkspaceLaunchRequest const& request) {
        if (request.documents.empty()) CreateMainWindow().Activate();
        else for (auto const& path : request.documents) CreateMainWindow(winrt::hstring(path.native())).Activate();
        ::OutputDebugStringW(L"Folia.StartupComplete\n");
    }
    MainWindow* App::FindMainWindow(Microsoft::UI::WindowId id) const {
        for (auto const& item : windows)
            if (item.id == id)
                return winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
        return nullptr;
    }
    MainWindow* App::FindTabWindow(std::uint64_t view) const {
        for (auto const& item : windows) {
            auto owner = winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
            if (!owner->windowClosed && owner->editorGroups.Find(view)) return owner;
        }
        return nullptr;
    }
    MainWindow* App::FindChatWindow(std::string_view id) const {
        for (auto const& item : windows) {
            auto owner = winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
            if (!owner->windowClosed && owner->workspaceDocuments.FindChat(id)) return owner;
        }
        return nullptr;
    }
    void App::UpdateWindowViewSettings(MainWindow const* source, MediaDisplaySettings const& media, WorkspaceAppearance const& appearance) {
        for (auto const& item : windows) {
            auto window = winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
            if (window != source && !window->windowClosed) {
                window->UpdateMediaSettings(media);
                window->UpdateWorkspaceAppearance(appearance);
            }
        }
    }
}
