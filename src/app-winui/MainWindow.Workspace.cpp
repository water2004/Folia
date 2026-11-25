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
    using namespace Microsoft::UI::Xaml::Media::Animation;
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
        SidebarButton().Checked(WindowEvent([this](auto const&, auto const&) {
            if (!sidebarOpen || sidebarClosing) SetSidebarOpen(true);
        }));
        SidebarButton().Unchecked(WindowEvent([this](auto const&, auto const&) {
            if (sidebarOpen && !sidebarClosing) SetSidebarOpen(false);
        }));
        AppWindow().Changed(WindowEvent([this](auto const&, AppWindowChangedEventArgs const& args) {
            if (args.DidPresenterChange()) UpdateWorkspaceLayout();
        }));
        documentTools.Attach(ReadingCard(), DocumentToolbar(), DocumentViewport());
        for (auto const& flyout : {EncodingButton().Flyout(), DocumentActionsButton().Flyout()}) {
            flyout.Opening(WindowEvent([this](auto const&, auto const&) { documentTools.HoldOpen(true); }));
            flyout.Closed(WindowEvent([this](auto const&, auto const&) { documentTools.HoldOpen(false); }));
        }
        ApplyBarLayout();
    }

    void MainWindow::SelectWorkspacePane(bool outline) {
        const bool samePane = sidebarOpen && !sidebarClosing && outlineSelected == outline && !SettingsActive();
        const bool fromSettings = SettingsActive();
        if (fromSettings) ReturnToDocumentTab();
        outlineSelected = outline;
        SetSidebarOpen(fromSettings || !samePane);
    }

    void MainWindow::SetSidebarOpen(bool open) {
        if (open == (sidebarOpen && !sidebarClosing)) {
            UpdateWorkspaceLayout();
            return;
        }
        const bool animated = WorkspaceSidebar().IsLoaded() && AppWindow().IsVisible()
            && Windows::UI::ViewManagement::UISettings().AnimationsEnabled();
        if (!animated && sidebarMotion) {
            sidebarMotionCompleted.revoke();
            sidebarMotion.Stop();
            WorkspaceSidebar().Opacity(1.);
            WorkspaceSidebar().IsHitTestVisible(true);
            WorkspaceSidebar().RenderTransform().as<Media::TranslateTransform>().X(0.);
        }
        sidebarClosing = !open && animated;
        if (open || !animated) ApplySidebarOpen(open);
        if (animated) AnimateSidebar(open);
    }

    void MainWindow::AnimateSidebar(bool open) {
        auto sidebar = WorkspaceSidebar();
        auto slide = sidebar.RenderTransform().try_as<Media::TranslateTransform>();
        if (!slide) { slide = Media::TranslateTransform(); sidebar.RenderTransform(slide); }
        const bool moving = sidebarMotion && sidebarMotion.GetCurrentState() == ClockState::Active;
        const double opacity = moving ? sidebar.Opacity() : (open ? 0. : 1.);
        const double offset = moving ? slide.X() : (open ? -10. : 0.);
        sidebarMotionCompleted.revoke();
        if (sidebarMotion) sidebarMotion.Stop();
        sidebar.Opacity(open ? 1. : 0.);
        slide.X(open ? 0. : -10.);
        sidebar.IsHitTestVisible(open);
        sidebarMotion = Storyboard();
        auto add = [&](DependencyObject const& target, hstring const& property, double from, double to) {
            DoubleAnimation animation;
            animation.From(from);
            animation.To(to);
            animation.Duration(DurationHelper::FromTimeSpan(open ? 180ms : 135ms));
            CubicEase easing;
            easing.EasingMode(EasingMode::EaseOut);
            animation.EasingFunction(easing);
            Storyboard::SetTarget(animation, target);
            Storyboard::SetTargetProperty(animation, property);
            sidebarMotion.Children().Append(animation);
        };
        add(sidebar, L"Opacity", opacity, open ? 1. : 0.);
        add(slide, L"X", offset, open ? 0. : -10.);
        if (!open) {
            UpdateWorkspaceLayout();
            sidebarMotionCompleted = sidebarMotion.Completed(auto_revoke, [weak = get_weak()](auto const&, auto const&) {
                if (auto window = weak.get(); window && !window->windowClosed) {
                    window->sidebarClosing = false;
                    window->ApplySidebarOpen(false);
                }
            });
        }
        sidebarMotion.Begin();
    }

    void MainWindow::ApplySidebarOpen(bool open) {
        if (open == sidebarOpen) {
            UpdateWorkspaceLayout();
            return;
        }
        auto window = AppWindow();
        auto presenter = window.Presenter().try_as<OverlappedPresenter>();
        const bool restored = presenter && presenter.State() == OverlappedPresenterState::Restored;
        if (window.IsVisible() && restored) {
            auto work = DisplayArea::GetFromWindowId(window.Id(), DisplayAreaFallback::Nearest).WorkArea();
            const auto size = window.Size();
            const auto pos = window.Position();
            const auto dpi = static_cast<double>(::GetDpiForWindow(WindowHandle())) / 96.;
            const auto width = static_cast<int>(std::lround(sidebarWidth * dpi));
            const int freeLeft = (std::max)(0, pos.X - work.X);
            const int freeRight = (std::max)(0, work.X + work.Width - pos.X - size.Width);
            if (open && Root().ActualWidth() >= minimumWorkspaceWidth && freeLeft + freeRight >= width) {
                sidebarAddedWidth = width;
                sidebarMovedLeft = (std::min)(freeLeft, width);
                window.MoveAndResize({pos.X - sidebarMovedLeft, pos.Y, size.Width + width, size.Height});
            } else if (!open && sidebarAddedWidth) {
                const int shrink = (std::min)(sidebarAddedWidth, (std::max)(0, size.Width - static_cast<int>(minimumWorkspaceWidth * dpi)));
                const int move = (std::min)(sidebarMovedLeft, shrink);
                window.MoveAndResize({pos.X + move, pos.Y, size.Width - shrink, size.Height});
            }
        }
        if (!open) sidebarMovedLeft = sidebarAddedWidth = 0;
        sidebarOpen = open;
        // Keep the base values usable when the system skips motion.
        WorkspaceSidebar().Opacity(1.);
        WorkspaceSidebar().IsHitTestVisible(true);
        if (auto slide = WorkspaceSidebar().RenderTransform().try_as<Media::TranslateTransform>()) slide.X(0.);
        UpdateWorkspaceLayout();
    }

    void MainWindow::UpdateWorkspaceLayout() {
        const bool settings = SettingsActive();
        const auto presenter = AppWindow().Presenter().try_as<OverlappedPresenter>();
        const bool restored = presenter && presenter.State() == OverlappedPresenterState::Restored;
        // When the desktop has no room to extend the window, keep the paper
        // at its current width and show the pane over its left edge.
        const bool overlay = sidebarOpen && (!restored || !sidebarAddedWidth
            || Root().ActualWidth() < minimumWorkspaceWidth + sidebarWidth);
        if (sidebarOverlay != overlay) {
            sidebarOverlay = overlay;
            ApplySidebarSurface();
        }
        const auto width = sidebarOpen && !sidebarOverlay ? sidebarWidth : 0.;
        if (SidebarColumn().Width().Value != width) SidebarColumn().Width(GridLengthHelper::FromPixels(width));
        auto sidebar = WorkspaceSidebar();
        sidebar.Width(sidebarWidth);
        sidebar.HorizontalAlignment(HorizontalAlignment::Left);
        Grid::SetColumnSpan(sidebar, sidebarOverlay ? 2 : 1);
        sidebar.Visibility(sidebarOpen ? Visibility::Visible : Visibility::Collapsed);
        const bool selected = sidebarOpen && !sidebarClosing;
        SidebarButton().IsChecked(selected);
        updatingRailSelection = true;
        FilesTab().IsChecked(selected && !settings && !outlineSelected);
        OutlineTab().IsChecked(selected && !settings && outlineSelected);
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
