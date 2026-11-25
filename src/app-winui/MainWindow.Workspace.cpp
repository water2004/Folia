#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Windowing;
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace Microsoft::UI::Xaml::Media::Animation;
    using namespace std::chrono_literals;

    namespace {
        constexpr double sidebarWidth = 216;
        constexpr UINT sidebarResizeFlags = SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;
    }

    void MainWindow::RegisterWorkspaceLayout() {
        auto configure = [](auto const& control, WorkspaceIcon icon, wchar_t const* label) {
            auto visual = MakeWorkspaceIcon(icon);
            if (control.Width() >= 34.) {
                visual.Width(19.);
                visual.Height(19.);
            }
            control.Content(visual);
            auto name = Localize(label);
            Automation::AutomationProperties::SetName(control, name);
            ToolTipService::SetToolTip(control, box_value(name));
        };
        configure(FilesTab(), WorkspaceIcon::Folder, L"Files");
        configure(OutlineTab(), WorkspaceIcon::Outline, L"Outline");
        configure(SettingsButton(), WorkspaceIcon::Settings, L"Settings");
        configure(SidebarButton(), WorkspaceIcon::PanelLeft, L"ToggleSidebar");
        configure(NewTabButton(), WorkspaceIcon::Add, L"NewDocument");
        configure(DocumentActionsButton(), WorkspaceIcon::More, L"FileMenu");
        configure(SourceModeButton(), WorkspaceIcon::Source, L"SourceMode");
        configure(FolderNewFileButton(), WorkspaceIcon::NewFile, L"NewFile");
        configure(FolderNewFolderButton(), WorkspaceIcon::NewFolder, L"NewFolder");
        configure(FolderRefreshButton(), WorkspaceIcon::Refresh, L"RefreshFiles");
        configure(FolderCollapseButton(), WorkspaceIcon::Collapse, L"CollapseFolders");
        auto watchPane = [this](auto const& button, bool outline) {
            auto select = WindowEvent([this, outline](auto const&, auto const&) {
                if (!updatingRailSelection) SelectWorkspacePane(outline);
            });
            button.Checked(select);
            button.Unchecked(select);
        };
        watchPane(FilesTab(), false);
        watchPane(OutlineTab(), true);
        auto toggleSidebar = [this](bool open) {
            return WindowEvent([this, open](auto const&, auto const&) {
                if (updatingRailSelection) return;
                if (SettingsActive()) ReturnToDocumentTab();
                SetSidebarOpen(open);
            });
        };
        SidebarButton().Checked(toggleSidebar(true));
        SidebarButton().Unchecked(toggleSidebar(false));
        WorkspaceSidebar().Width(sidebarWidth);
        WorkspaceSidebar().HorizontalAlignment(HorizontalAlignment::Left);
        documentTools.Attach(ReadingCard(), DocumentToolbar(), DocumentViewport());
        for (auto const& flyout : {EncodingButton().Flyout(), DocumentActionsButton().Flyout()}) {
            flyout.Opening(WindowEvent([this](auto const&, auto const&) { documentTools.HoldOpen(true); }));
            flyout.Closed(WindowEvent([this](auto const&, auto const&) { documentTools.HoldOpen(false); }));
        }
        ApplyBarLayout();
    }

    void MainWindow::SelectWorkspacePane(bool outline) {
        const bool samePane = sidebarOpen && outlineSelected == outline && !SettingsActive();
        const bool fromSettings = SettingsActive();
        if (fromSettings) ReturnToDocumentTab();
        outlineSelected = outline;
        SetSidebarOpen(fromSettings || !samePane);
    }

    void MainWindow::SetSidebarOpen(bool open) {
        if (open == sidebarOpen) {
            UpdateWorkspaceLayout();
            return;
        }

        const double from = SidebarSlot().ActualWidth();
        RevokeSidebarMotion();
        sidebarOpen = open;
        if (open && !sidebarPresented) {
            sidebarPresented = true;
            ReserveSidebarSpace();
        }
        UpdateWorkspaceLayout();

        auto presenter = AppWindow().Presenter().try_as<OverlappedPresenter>();
        const bool extended = sidebarWindowExtension && presenter && presenter.State() == OverlappedPresenterState::Restored;
        if (extended || !Root().IsLoaded() || !AppWindow().IsVisible()
            || !Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
            FinishSidebarMotion();
            return;
        }
        AnimateSidebarWidth(from);
    }

    void MainWindow::AnimateSidebarWidth(double from) {
        const double to = sidebarOpen ? sidebarWidth : 0.;
        SidebarSlot().Width(to);
        if (std::abs(to - from) < 0.5) { FinishSidebarMotion(); return; }
        DoubleAnimation animation;
        animation.From(from);
        animation.To(to);
        animation.EnableDependentAnimation(true);
        animation.Duration(DurationHelper::FromTimeSpan(sidebarOpen ? 200ms : 170ms));
        CubicEase easing;
        easing.EasingMode(EasingMode::EaseOut);
        animation.EasingFunction(easing);
        Storyboard::SetTarget(animation, SidebarSlot());
        Storyboard::SetTargetProperty(animation, L"Width");
        sidebarMotion = Storyboard();
        sidebarMotion.Children().Append(animation);
        sidebarMotionCompleted = sidebarMotion.Completed(WindowEvent([this](auto const&, auto const&) {
            FinishSidebarMotion();
        }));
        sidebarMotion.Begin();
    }

    void MainWindow::ReserveSidebarSpace() {
        auto presenter = AppWindow().Presenter().try_as<OverlappedPresenter>();
        if (sidebarWindowExtension || !AppWindow().IsVisible() || !presenter
            || presenter.State() != OverlappedPresenterState::Restored || !presenter.IsResizable()) return;

        RECT bounds{};
        auto handle = WindowHandle();
        if (!::GetWindowRect(handle, &bounds)) return;
        const double dpi = static_cast<double>(::GetDpiForWindow(handle)) / 96.;
        const int width = static_cast<int>(std::lround(sidebarWidth * dpi));
        sidebarWindowExtension = width;
        LayoutSidebar();
        SidebarSlot().Width(sidebarWidth);
        // Move only the left edge so the reading card remains where it was on
        // the desktop. The user controls where the window sits on the display.
        if (!::SetWindowPos(handle, nullptr, bounds.left - width, bounds.top,
                bounds.right - bounds.left + width, bounds.bottom - bounds.top,
                sidebarResizeFlags)) {
            sidebarWindowExtension = 0;
            SidebarSlot().Width(0.);
        }
        Root().UpdateLayout();
    }

    void MainWindow::RevokeSidebarMotion() {
        if (!sidebarMotion) return;
        sidebarMotion.Completed(sidebarMotionCompleted);
        sidebarMotion.Stop();
        sidebarMotion = nullptr;
    }

    void MainWindow::FinishSidebarMotion() {
        RevokeSidebarMotion();
        SidebarSlot().Width(sidebarOpen ? sidebarWidth : 0.);
        if (sidebarOpen) return;
        sidebarPresented = false;

        auto presenter = AppWindow().Presenter().try_as<OverlappedPresenter>();
        const bool restored = presenter && presenter.State() == OverlappedPresenterState::Restored;
        int shrink = restored ? std::exchange(sidebarWindowExtension, 0) : 0;
        LayoutSidebar();
        if (!shrink) return;

        RECT bounds{};
        auto handle = WindowHandle();
        if (!::GetWindowRect(handle, &bounds)) return;
        const int width = bounds.right - bounds.left;
        if (width > shrink)
            ::SetWindowPos(handle, nullptr, bounds.left + shrink, bounds.top, width - shrink,
                bounds.bottom - bounds.top, sidebarResizeFlags);
        Root().UpdateLayout();
    }

    void MainWindow::LayoutSidebar() {
        auto sidebar = WorkspaceSidebar();
        sidebar.Visibility(sidebarPresented ? Visibility::Visible : Visibility::Collapsed);
        sidebar.IsHitTestVisible(sidebarOpen);
    }

    void MainWindow::UpdateWorkspaceLayout() {
        if (!sidebarPresented && sidebarWindowExtension) FinishSidebarMotion();
        else LayoutSidebar();
        updatingRailSelection = true;
        SidebarButton().IsChecked(sidebarOpen);
        FilesTab().IsChecked(sidebarOpen && !outlineSelected);
        OutlineTab().IsChecked(sidebarOpen && outlineSelected);
        updatingRailSelection = false;
        ExplorerPane().Visibility(outlineSelected ? Visibility::Collapsed : Visibility::Visible);
        OutlinePaneContent().Visibility(outlineSelected ? Visibility::Visible : Visibility::Collapsed);
        OutlineList().Visibility(HasActiveEditor() ? Visibility::Visible : Visibility::Collapsed);
        OutlineEmptyText().Visibility(HasActiveEditor() && OutlineList().Items().Size() ? Visibility::Collapsed : Visibility::Visible);
        RefreshTabCaptionRegion();
    }

    void MainWindow::ApplyBarLayout() {
        const bool showTools = !SettingsActive();
        const bool overlay = showTools && appSettings.appearance.autoHideTools;
        Grid::SetRow(DocumentViewport(), overlay ? 0 : 1);
        Grid::SetRowSpan(DocumentViewport(), overlay ? 2 : 1);
        documentTools.Configure(overlay, showTools);
    }
}
