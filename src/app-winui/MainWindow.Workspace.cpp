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
        configure(ChatsTab(), WorkspaceIcon::Chat, L"ChatSessions");
        configure(NewChatButton(), WorkspaceIcon::Add, L"ChatNew");
        configure(SettingsButton(), WorkspaceIcon::Settings, L"Settings");
        configure(SidebarButton(), WorkspaceIcon::PanelLeft, L"ToggleSidebar");
        configure(DocumentActionsButton(), WorkspaceIcon::More, L"FileMenu");
        configure(SourceModeButton(), WorkspaceIcon::Source, L"SourceMode");
        configure(FolderNewFileButton(), WorkspaceIcon::NewFile, L"NewFile");
        configure(FolderNewFolderButton(), WorkspaceIcon::NewFolder, L"NewFolder");
        configure(FolderRefreshButton(), WorkspaceIcon::Refresh, L"RefreshFiles");
        configure(FolderCollapseButton(), WorkspaceIcon::Collapse, L"CollapseFolders");
        auto watchPane = [this](auto const& button, WorkspacePane pane) {
            auto select = WindowEvent([this, pane](auto const&, auto const&) {
                if (!updatingRailSelection) SelectWorkspacePane(pane);
            });
            button.Checked(select);
            button.Unchecked(select);
        };
        watchPane(FilesTab(), WorkspacePane::Files);
        watchPane(OutlineTab(), WorkspacePane::Outline);
        watchPane(ChatsTab(), WorkspacePane::Chats);
        RegisterAgentChats();
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
        readingTextLayout = BindReadingTextLayout(ReadingTextLayoutButton(), documentTools);
        for (auto const& flyout : {EncodingButton().Flyout(), DocumentActionsButton().Flyout()}) {
            flyout.Opening(WindowEvent([this](auto const&, auto const&) { documentTools.HoldOpen(true); }));
            flyout.Closed(WindowEvent([this](auto const&, auto const&) { documentTools.HoldOpen(false); }));
        }
        ApplyBarLayout();
    }

    void MainWindow::SelectWorkspacePane(WorkspacePane pane) {
        const bool fromSettings = SettingsActive();
        const bool samePane = sidebarMotion.IsOpen() && workspacePane == pane && !fromSettings;
        if (fromSettings) ReturnToDocumentTab();
        workspacePane = pane;
        if (pane == WorkspacePane::Chats && !samePane) RefreshChatList();
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
        FilesTab().IsChecked(sidebarOpen && workspacePane == WorkspacePane::Files);
        OutlineTab().IsChecked(sidebarOpen && workspacePane == WorkspacePane::Outline);
        ChatsTab().IsChecked(sidebarOpen && workspacePane == WorkspacePane::Chats);
        updatingRailSelection = false;
        ExplorerPane().Visibility(workspacePane == WorkspacePane::Files ? Visibility::Visible : Visibility::Collapsed);
        OutlinePaneContent().Visibility(workspacePane == WorkspacePane::Outline ? Visibility::Visible : Visibility::Collapsed);
        ChatSidebar().Visibility(workspacePane == WorkspacePane::Chats ? Visibility::Visible : Visibility::Collapsed);
        OutlineList().Visibility(HasActiveEditor() ? Visibility::Visible : Visibility::Collapsed);
        OutlineEmptyText().Visibility(HasActiveEditor() && OutlineList().Items().Size() ? Visibility::Collapsed : Visibility::Visible);
        RefreshTabCaptionRegion();
    }

    void MainWindow::ApplyBarLayout() {
        const bool showTools = !SettingsActive() && workspaceDocuments.Active()->kind != WorkspaceTabKind::Chat;
        const bool overlay = showTools && appSettings.appearance.autoHideTools;
        Grid::SetRow(DocumentViewport(), overlay ? 0 : 1);
        Grid::SetRowSpan(DocumentViewport(), overlay ? 2 : 1);
        documentTools.Configure(overlay, showTools);
        if (editorGroups.IsSplit()) {
            auto view = editorGroups.Find(editorGroups.Selected(OtherEditorGroup(editorGroups.ActiveGroup())));
            auto document = view ? workspaceDocuments.Find(view->document) : nullptr;
            const bool available = document && document->kind != WorkspaceTabKind::Settings && document->kind != WorkspaceTabKind::Chat;
            const bool otherOverlay = available && appSettings.appearance.autoHideTools;
            Grid::SetRow(OtherDocumentViewport(), otherOverlay ? 0 : 1);
            Grid::SetRowSpan(OtherDocumentViewport(), otherOverlay ? 2 : 1);
            otherDocumentTools.Configure(otherOverlay, available);
        }
    }
}
