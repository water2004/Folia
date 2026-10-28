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
            if (auto app = weak.get()) {
                std::erase_if(app->windows, [id](auto const& item) { return item.id == id; });
                if (app->windows.empty() && app->activationInstance && app->activationInstance.IsCurrent()) {
                    app->activationSubscription.revoke();
                    app->activationInstance.UnregisterKey();
                }
            }
        });
        return window;
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
