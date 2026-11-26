#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

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
        sidebarMotion.Attach(WorkspaceLayout(), SidebarColumn(), WorkspaceSidebar(), DocumentLayoutSlot(), DocumentColumn(),
            [weak = get_weak()] {
                if (auto window = weak.get(); window && !window->windowClosed) {
                    window->RefreshTabCaptionRegion();
                    window->UpdateSelectedTabBackdrop();
                }
            });
        sidebarMotion.ClipTitle(TabMotionFrame(), CaptionInset());
        sidebarMotion.ClipReading(ReadingMotionFrame());
        sidebarMotion.PinTrailing({ToolbarStatus(), EditorScrollBar(), WorkspaceNotifications()}, ToolbarFilename());
        documentTools.Attach(ReadingCard(), DocumentToolbar(), DocumentViewport());
        for (auto const& flyout : {EncodingButton().Flyout(), DocumentActionsButton().Flyout()}) {
            flyout.Opening(WindowEvent([this](auto const&, auto const&) { documentTools.HoldOpen(true); }));
            flyout.Closed(WindowEvent([this](auto const&, auto const&) { documentTools.HoldOpen(false); }));
        }
        ApplyBarLayout();
    }

    void MainWindow::SelectWorkspacePane(bool outline) {
        const bool fromSettings = SettingsActive();
        const bool samePane = sidebarMotion.IsOpen() && outlineSelected == outline && !fromSettings;
        if (fromSettings) ReturnToDocumentTab();
        outlineSelected = outline;
        SetSidebarOpen(fromSettings || !samePane);
    }

    void MainWindow::SetSidebarOpen(bool open) {
        const bool animate = Root().IsLoaded() && AppWindow().IsVisible()
            && Windows::UI::ViewManagement::UISettings().AnimationsEnabled();
        sidebarMotion.SetOpen(open, animate);
        UpdateWorkspaceLayout();
    }

    void MainWindow::UpdateWorkspaceLayout() {
        const bool sidebarOpen = sidebarMotion.IsOpen();
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
        if (editorGroups.IsSplit()) {
            auto view = editorGroups.Find(editorGroups.Selected(OtherEditorGroup(editorGroups.ActiveGroup())));
            auto document = view ? workspaceDocuments.Find(view->document) : nullptr;
            const bool available = document && document->kind != WorkspaceTabKind::Settings;
            const bool otherOverlay = available && appSettings.appearance.autoHideTools;
            Grid::SetRow(OtherDocumentViewport(), otherOverlay ? 0 : 1);
            Grid::SetRowSpan(OtherDocumentViewport(), otherOverlay ? 2 : 1);
            otherDocumentTools.Configure(otherOverlay, available);
        }
    }
}
