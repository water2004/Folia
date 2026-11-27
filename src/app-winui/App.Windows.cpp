#include "pch.h"
import std;
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <windowsx.h>
#include <winrt/Microsoft.UI.Content.h>
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
                if (RouteTabMouse(message, data)) return 1;
            } catch (...) {
                // Preserve normal message delivery if dispatch fails.
            }
        }
        return ::CallNextHookEx(nullptr, code, message, data);
    }

    bool App::RouteTabMouse(WPARAM message, LPARAM data) {
        auto const& mouse = *reinterpret_cast<MOUSEHOOKSTRUCT const*>(data);
        const bool wheel = message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL;
        const bool release = message == WM_MBUTTONUP || message == WM_NCMBUTTONUP;
        const bool captionPress = mouse.wHitTestCode == HTCAPTION
            && (message == WM_NCMBUTTONDOWN || message == WM_NCMBUTTONDBLCLK);
        if (!wheel && !release && !captionPress) return false;

        // WinUI dispatches caption input to a child HWND of the main window.
        auto window = ::GetAncestor(mouse.hwnd, GA_ROOT);
        if (!window) return false;
        auto owner = Current()->FindMainWindow(Microsoft::UI::GetWindowIdFromWindow(window));
        if (!owner) return false;
        auto target = owner->get_strong();
        const Windows::Graphics::PointInt32 point{mouse.pt.x, mouse.pt.y};
        if (wheel) {
            // Tab faces occupy non-client drag regions. Wheel input there never reaches XAML.
            auto root = target->Root().XamlRoot();
            if (!root) return false;
            auto group = target->TabStripAt(root.CoordinateConverter().ConvertScreenToLocal(point));
            auto delta = GET_WHEEL_DELTA_WPARAM(reinterpret_cast<MOUSEHOOKSTRUCTEX const*>(data)->mouseData);
            return group && target->ScrollTabsWithWheel(message == WM_MOUSEHWHEEL ? -delta : delta, *group);
        }
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
        auto foreground = [](MainWindow* target) {
            auto handle = target->WindowHandle();
            if (::IsIconic(handle)) ::ShowWindow(handle, SW_RESTORE);
            target->Activate();
            ::SetForegroundWindow(handle);
        };
        auto recent = [&](std::filesystem::path const& path) {
            MainWindow* best = nullptr;
            for (auto const& item : windows) {
                auto w = winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
                if (w->preparedTabWindow || w->closePending || w->closingDocument || w->windowClosed || w->relocatingFiles) continue;
                if (!path.empty() && !w->workspaceDocuments.Find(path)) continue;
                if (!best || w->lastActivated >= best->lastActivated) best = w;
            }
            return best;
        };
        auto create = [&] {
            return winrt::get_self<MainWindow>(CreateMainWindow().as<winrt::Folia::MainWindow>());
        };
        if (request.documents.empty()) foreground(create());
        else {
            auto target = recent({});
            bool emptyWindow = !target;
            if (emptyWindow) target = create();
            const bool newWindows = target->appSettings.openFilesInNewWindow;
            for (auto const& path : request.documents) {
                if (auto owner = recent(path)) {
                    auto doc = owner->workspaceDocuments.Find(path);
                    foreground(owner);
                    if (owner->workspaceDocuments.Keep(doc->id)) owner->UpdateDocumentTab(doc->id);
                    owner->ActivateView(owner->FindDocumentView(doc->id));
                } else {
                    if (newWindows && !emptyWindow) target = create();
                    foreground(target);
                    target->OpenWorkspaceFile(path, WorkspaceOpenMode::NewTab);
                    emptyWindow = false;
                }
            }
        }
        ::OutputDebugStringW(L"Folia.StartupComplete\n");
    }
    MainWindow* App::FindMainWindow(Microsoft::UI::WindowId id) const {
        for (auto const& item : windows)
            if (item.id == id)
                return winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
        return nullptr;
    }
    MainWindow* App::OtherDocumentWindow(MainWindow const* source, std::uint64_t id) const {
        for (auto const& item : windows) {
            auto window = get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
            if (window != source && !window->windowClosed && window->workspaceDocuments.Find(id)) return window;
        }
        return nullptr;
    }

    void App::RefreshSharedDocument(MainWindow const* source, std::uint64_t id) {
        for (auto const& item : windows) {
            auto window = get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
            if (window == source || window->windowClosed || !window->workspaceDocuments.Find(id)) continue;
            if (window->IsActiveDocument(id)) {
                window->UpdateDocumentInfo(false);
                window->UpdateSourceModeUi();
                window->RenderEditorSurface();
            } else {
                window->UpdateDocumentTab(id);
                window->QueueOtherGroupRender();
            }
        }
    }

    MainWindow* App::FindTabWindow(std::uint64_t view) const {
        for (auto const& item : windows) {
            auto owner = winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
            if (!owner->windowClosed && owner->editorGroups.Find(view)) return owner;
        }
        return nullptr;
    }
    MainWindow* App::FindReferencedDocument(std::string_view key, std::filesystem::path const& path, std::uint64_t& id) const {
        auto locate = [&](bool byKey) -> MainWindow* {
            for (auto const& item : windows) {
                auto owner = winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
                if (owner->windowClosed) continue;
                auto tab = byKey ? nullptr : owner->workspaceDocuments.Find(path);
                if (byKey) for (auto const& doc : owner->workspaceDocuments.Items())
                    if (doc->referenceKey == key) { tab = doc; break; }
                if (tab) { id = tab->id; return owner; }
            }
            return nullptr;
        };
        if (!key.empty()) if (auto owner = locate(true)) return owner;
        return path.empty() ? nullptr : locate(false);
    }

    MainWindow* App::FindChatWindow(std::string_view id) const {
        for (auto const& item : windows) {
            auto owner = winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
            if (!owner->windowClosed && owner->workspaceDocuments.FindChat(id)) return owner;
        }
        return nullptr;
    }
    void App::UpdateWindowViewSettings(MainWindow const* source, AppSettings const& settings) {
        for (auto const& item : windows) {
            auto window = winrt::get_self<MainWindow>(item.window.as<winrt::Folia::MainWindow>());
            if (window != source && !window->windowClosed) {
                window->UpdateMediaSettings(settings.media);
                window->UpdateWorkspaceAppearance(settings.appearance);
                if (window->appSettings.openFilesInNewWindow != settings.openFilesInNewWindow) {
                    window->appSettings.openFilesInNewWindow = settings.openFilesInNewWindow;
                    if (window->settingsView) window->settingsView->UpdateFileOpening(settings.openFilesInNewWindow);
                }
                if (window->appSettings.themeId != settings.themeId) {
                    window->appSettings.themeId = settings.themeId;
                    window->UpdateTheme();
                    if (window->settingsView) window->settingsView->UpdateAppliedTheme(settings.themeId);
                }
            }
        }
    }
}
