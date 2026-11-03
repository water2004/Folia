#include "pch.h"
import std;
#include "App.xaml.h"
#include "MainWindow.xaml.h"

namespace winrt::Folia::implementation {
    App* App::Current() {
        return winrt::get_self<App>(Microsoft::UI::Xaml::Application::Current()
            .as<Microsoft::UI::Xaml::Markup::IXamlMetadataProvider>());
    }
    Microsoft::UI::Xaml::Window App::CreateMainWindow(winrt::hstring const& path) {
        auto window = winrt::make<MainWindow>(path);
        auto id = window.AppWindow().Id();
        windows.push_back({id, window});
        window.Closed([weak = get_weak(), id](auto const&, auto const&) {
            if (auto app = weak.get()) app->OnWindowClosed(id);
        });
        return window;
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
    MainWindow* App::FindDocumentWindow(std::uint64_t id) const {
        for (auto const& item : windows) {
            auto owner = winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
            if (!owner->windowClosed && owner->workspaceDocuments.Find(id)) return owner;
        }
        return nullptr;
    }
}
