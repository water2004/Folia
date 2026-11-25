#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Windowing;
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace std::chrono_literals;

    namespace {
        constexpr double sidebarWidth = 216;
        constexpr int minimumWorkspaceWidth = 480;
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
        auto watchPane = [this](auto const& button, bool outline) {
            auto select = WindowEvent([this, outline](auto const&, auto const&) {
                if (!updatingRailSelection) SelectWorkspacePane(outline);
            });
            button.Checked(select);
            button.Unchecked(select);
        };
        watchPane(FilesTab(), false);
        watchPane(OutlineTab(), true);
        SidebarButton().Checked(WindowEvent([this](auto const&, auto const&) { SetSidebarOpen(true); }));
        SidebarButton().Unchecked(WindowEvent([this](auto const&, auto const&) { SetSidebarOpen(false); }));
        WorkspaceSidebar().Width(sidebarWidth);
        WorkspaceSidebar().HorizontalAlignment(HorizontalAlignment::Left);
        WorkspaceSidebar().SizeChanged(WindowEvent([this](auto const&, auto const&) { LayoutSidebar(); }));
        AppWindow().Changed(WindowEvent([this](auto const&, AppWindowChangedEventArgs const& args) {
            if (args.DidPresenterChange()) LayoutSidebar();
        }));
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
        sidebarOpen = open;
        if (open && sidebarMotion.fraction == 0. && !sidebarMotion.appliedWidth) PlanSidebarExpansion();
        sidebarMotion.rendering.revoke();
        if (Root().IsLoaded() && AppWindow().IsVisible()
            && Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
            sidebarMotion.from = sidebarMotion.fraction;
            sidebarMotion.started = std::chrono::steady_clock::now();
            sidebarMotion.rendering = Media::CompositionTarget::Rendering(auto_revoke, WindowEvent(
                [this](auto const&, auto const&) { AdvanceSidebar(); }));
        } else {
            ApplySidebarFrame(open ? 1. : 0.);
        }
        UpdateWorkspaceLayout();
    }

    void MainWindow::PlanSidebarExpansion() {
        auto window = AppWindow();
        auto presenter = window.Presenter().try_as<OverlappedPresenter>();
        auto& motion = sidebarMotion;
        motion.targetWidth = motion.targetLeft = 0;
        if (!window.IsVisible() || !presenter || presenter.State() != OverlappedPresenterState::Restored
            || Root().ActualWidth() < minimumWorkspaceWidth) return;
        auto display = DisplayArea::GetFromWindowId(window.Id(), DisplayAreaFallback::Nearest);
        if (!display) return;
        auto work = display.WorkArea();
        auto position = window.Position();
        auto size = window.Size();
        const double dpi = static_cast<double>(::GetDpiForWindow(WindowHandle())) / 96.;
        const int width = static_cast<int>(std::lround(sidebarWidth * dpi));
        const int left = (std::max)(0, position.X - work.X);
        const int right = (std::max)(0, work.X + work.Width - position.X - size.Width);
        if (left + right < width) return;
        motion.targetWidth = width;
        motion.targetLeft = (std::min)(left, width);
    }

    void MainWindow::AdvanceSidebar() {
        auto& motion = sidebarMotion;
        auto elapsed = std::chrono::steady_clock::now() - motion.started;
        const auto duration = sidebarOpen ? 240ms : 190ms;
        const double progress = std::clamp(std::chrono::duration<double>(elapsed) / duration, 0., 1.);
        const double remaining = 1. - progress;
        const double eased = 1. - remaining * remaining * remaining;
        ApplySidebarFrame(std::lerp(motion.from, sidebarOpen ? 1. : 0., eased));
        if (progress == 1.) motion.rendering.revoke();
    }

    void MainWindow::ApplySidebarFrame(double fraction) {
        auto& motion = sidebarMotion;
        motion.fraction = fraction;
        auto window = AppWindow();
        auto presenter = window.Presenter().try_as<OverlappedPresenter>();
        if (presenter && presenter.State() == OverlappedPresenterState::Restored && window.IsVisible()) {
            const int width = static_cast<int>(std::lround(motion.targetWidth * fraction));
            const int left = static_cast<int>(std::lround(motion.targetLeft * fraction));
            const int grow = width - motion.appliedWidth;
            const int move = motion.appliedLeft - left;
            if (grow || move) {
                RECT bounds{};
                if (!::GetWindowRect(WindowHandle(), &bounds)) return;
                // Update the pane offset in the same frame as the HWND bounds,
                // so the window can grow without reflowing the paper.
                motion.appliedWidth = width;
                motion.appliedLeft = left;
                LayoutSidebar();
                if (!::SetWindowPos(WindowHandle(), nullptr, bounds.left + move, bounds.top,
                        bounds.right - bounds.left + grow, bounds.bottom - bounds.top,
                        SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE)) {
                    motion.appliedWidth -= grow;
                    motion.appliedLeft += move;
                    motion.targetWidth = motion.appliedWidth;
                    motion.targetLeft = motion.appliedLeft;
                }
            }
        }
        LayoutSidebar();
    }

    void MainWindow::LayoutSidebar() {
        auto& motion = sidebarMotion;
        const bool showing = sidebarOpen || motion.fraction > 0.;
        auto presenter = AppWindow().Presenter().try_as<OverlappedPresenter>();
        const bool restored = presenter && presenter.State() == OverlappedPresenterState::Restored;
        const bool overlay = showing && (!restored || !motion.targetWidth);
        if (sidebarOverlay != overlay) {
            sidebarOverlay = overlay;
            ApplySidebarSurface();
        }
        const double dpi = (std::max)(1., static_cast<double>(::GetDpiForWindow(WindowHandle())) / 96.);
        const double docked = restored && !overlay ? (std::min)(sidebarWidth, motion.appliedWidth / dpi) : 0.;
        const double exposed = overlay ? sidebarWidth * motion.fraction : docked;
        if (SidebarColumn().Width().Value != docked) SidebarColumn().Width(GridLengthHelper::FromPixels(docked));

        auto sidebar = WorkspaceSidebar();
        const double inset = sidebarWidth - exposed;
        auto margin = sidebar.Margin();
        margin.Left = -inset;
        if (sidebar.Margin() != margin) sidebar.Margin(margin);
        const int span = overlay ? 2 : 1;
        if (Grid::GetColumnSpan(sidebar) != span) Grid::SetColumnSpan(sidebar, span);
        SidebarRevealClip().Rect({static_cast<float>(inset), 0, static_cast<float>(exposed), static_cast<float>(sidebar.ActualHeight())});
        sidebar.Visibility(showing ? Visibility::Visible : Visibility::Collapsed);
        sidebar.IsHitTestVisible(sidebarOpen);
    }

    void MainWindow::UpdateWorkspaceLayout() {
        // If the pane was closed while maximized, remove the extension from
        // the restored window as soon as it returns.
        if (!sidebarOpen && sidebarMotion.fraction == 0. && sidebarMotion.appliedWidth)
            ApplySidebarFrame(0.);
        else
            LayoutSidebar();
        const bool settings = SettingsActive();
        SidebarButton().IsChecked(sidebarOpen);
        updatingRailSelection = true;
        FilesTab().IsChecked(sidebarOpen && !settings && !outlineSelected);
        OutlineTab().IsChecked(sidebarOpen && !settings && outlineSelected);
        updatingRailSelection = false;
        ExplorerPane().Visibility(outlineSelected ? Visibility::Collapsed : Visibility::Visible);
        OutlinePaneContent().Visibility(outlineSelected ? Visibility::Visible : Visibility::Collapsed);
        OutlineList().Visibility(HasActiveEditor() ? Visibility::Visible : Visibility::Collapsed);
        OutlineEmptyText().Visibility(HasActiveEditor() && OutlineList().Items().Size() ? Visibility::Collapsed : Visibility::Visible);
        RefreshTabCaptionRegion();
    }

    void MainWindow::ApplySidebarSurface() {
        auto base = sidebarOverlay ? SystemBackdrop() : nullptr;
        FloatingSidebarBackdrop().SystemBackdrop(base);
        auto layer = Root().Resources().Lookup(box_value(L"FoliaShellBackgroundBrush")).as<Media::Brush>();
        FloatingSidebarTint().Background(sidebarOverlay ? layer : nullptr);
        FloatingSidebarTint().Opacity(base ? 0.68 : 1.);
    }

    void MainWindow::ApplyBarLayout() {
        const bool showTools = !SettingsActive();
        const bool overlay = showTools && appSettings.appearance.autoHideTools;
        Grid::SetRow(DocumentViewport(), overlay ? 0 : 1);
        Grid::SetRowSpan(DocumentViewport(), overlay ? 2 : 1);
        documentTools.Configure(overlay, showTools);
    }
}
