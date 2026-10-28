#pragma once

#include "App.xaml.g.h"
#include "workspace/WorkspaceLaunch.h"

namespace winrt::Folia::implementation
{
    struct MainWindow;
    struct App : AppT<App>
    {
        App();
        static App* Current();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
        Microsoft::UI::Xaml::Window CreateMainWindow(winrt::hstring const& path = {});
        MainWindow* FindMainWindow(Microsoft::UI::WindowId id) const;
        MainWindow* FindDocumentWindow(std::uint64_t id) const;

    private:
        winrt::fire_and_forget LaunchAsync();
        void OpenWorkspaceWindows(winrt::Folia::WorkspaceLaunchRequest const& request);
        Microsoft::Windows::AppLifecycle::AppInstance activationInstance{nullptr};
        Microsoft::Windows::AppLifecycle::AppInstance::Activated_revoker activationSubscription;
        struct WindowEntry { Microsoft::UI::WindowId id; Microsoft::UI::Xaml::Window window; };
        std::vector<WindowEntry> windows;
    };
}
