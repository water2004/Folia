#pragma once

#include "App.xaml.g.h"
#include "media/MediaDisplaySettings.h"
#include "workspace/WorkspaceLaunch.h"
#include "workspace/WorkspaceAppearance.h"

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
        void SetTabDragActive(Microsoft::UI::WindowId source, bool active);
        void UpdateWindowViewSettings(MainWindow const* source, MediaDisplaySettings const& media, WorkspaceAppearance const& appearance);

    private:
        winrt::fire_and_forget LaunchAsync();
        void OpenWorkspaceWindows(winrt::Folia::WorkspaceLaunchRequest const& request);
        void OnWindowClosed(Microsoft::UI::WindowId id);
        Microsoft::Windows::AppLifecycle::AppInstance activationInstance{nullptr};
        Microsoft::Windows::AppLifecycle::AppInstance::Activated_revoker activationSubscription;
        struct WindowEntry { Microsoft::UI::WindowId id; Microsoft::UI::Xaml::Window window; };
        std::vector<WindowEntry> windows;
        Microsoft::UI::WindowId tabDragSource{};
        bool closingPreparedWindows = false;
    };
}
