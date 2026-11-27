#pragma once

#include "App.xaml.g.h"
#include "workspace/WorkspaceLaunch.h"
#include <wil/resource.h>

import folia.app.text_layout_options;

namespace winrt::Folia { struct AppSettings; }

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
        MainWindow* OtherDocumentWindow(MainWindow const* source, std::uint64_t id) const;
        void RefreshSharedDocument(MainWindow const* source, std::uint64_t id);
        MainWindow* FindTabWindow(std::uint64_t view) const;
        MainWindow* FindReferencedDocument(std::string_view tabKey, std::filesystem::path const&, std::uint64_t& documentId) const;
        MainWindow* FindChatWindow(std::string_view id) const;
        void UpdateWindowViewSettings(MainWindow const* source, AppSettings const& settings);
        void ChangeTextLayouts(MainWindow& source, TextLayoutProfiles const&);

    private:
        friend struct WorkspaceWindowTestAccess;

        winrt::fire_and_forget LaunchAsync();
        void OpenWorkspaceWindows(winrt::Folia::WorkspaceLaunchRequest const& request);
        void OnWindowClosed(Microsoft::UI::WindowId id);
        void FlushTextLayouts();
        static LRESULT CALLBACK TabMouseHook(int code, WPARAM message, LPARAM data) noexcept;
        static bool RouteTabMouse(WPARAM message, LPARAM data);
        Microsoft::Windows::AppLifecycle::AppInstance activationInstance{nullptr};
        Microsoft::Windows::AppLifecycle::AppInstance::Activated_revoker activationSubscription;
        wil::unique_hhook tabMouseHook;
        struct WindowEntry { Microsoft::UI::WindowId id; Microsoft::UI::Xaml::Window window; };
        std::vector<WindowEntry> windows;
        Microsoft::UI::Dispatching::DispatcherQueueTimer textLayoutSaveTimer{nullptr};
        std::optional<TextLayoutProfiles> pendingTextLayouts;
        winrt::weak_ref<MainWindow> textLayoutSaveSource;
        bool closingPreparedWindows = false;
    };
}
