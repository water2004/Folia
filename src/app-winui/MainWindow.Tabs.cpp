#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    void MainWindow::UpdateDocumentTab(std::uint64_t id) {
        auto document = workspaceDocuments.Find(id);
        auto found = tabItems.find(id);
        if (!document || found == tabItems.end()) return;
        auto tab = found->second;
        auto header = tab.Header().as<StackPanel>();
        auto title = header.Children().GetAt(1).as<TextBlock>();
        auto dirty = header.Children().GetAt(2).as<TextBlock>();
        auto name = document->path.empty() ? Localize(L"UntitledMarkdown") : winrt::hstring(document->path.filename().c_str());
        auto label = name;
        for (auto const& other : workspaceDocuments.Items()) {
            if (other != document && !other->path.empty() && other->path.filename() == document->path.filename()) {
                label = name + L" · " + winrt::hstring(document->path.parent_path().filename().c_str());
                break;
            }
        }
        if (title.Text() != label) title.Text(label);
        title.FontStyle(document->kept ? Windows::UI::Text::FontStyle::Normal : Windows::UI::Text::FontStyle::Italic);
        dirty.Visibility(document->controller.IsModified() ? Visibility::Visible : Visibility::Collapsed);
        ToolTipService::SetToolTip(tab, winrt::box_value(document->path.empty() ? name : winrt::hstring(document->path.c_str())));
        Automation::AutomationProperties::SetName(tab, label);
    }

    void MainWindow::RefreshDocumentTabs() {
        if (updatingTabs || windowClosed) return;
        updatingTabs = true;
        struct Reset { bool& value; ~Reset() { value = false; } } reset{updatingTabs};
        auto items = DocumentTabs().TabItems();
        for (std::uint32_t index = items.Size(); index > 0; --index) {
            auto tab = items.GetAt(index - 1).as<TabViewItem>();
            auto id = winrt::unbox_value<std::uint64_t>(tab.Tag());
            if (!workspaceDocuments.Find(id)) { items.RemoveAt(index - 1); tabItems.erase(id); }
        }
        std::uint32_t index = 0;
        for (auto const& document : workspaceDocuments.Items()) {
            auto found = tabItems.find(document->id);
            TabViewItem tab{nullptr};
            if (found == tabItems.end()) {
                tab = TabViewItem();
                tab.Tag(winrt::box_value(document->id));
                StackPanel header;
                header.Orientation(Orientation::Horizontal);
                header.Spacing(8);
                TextBlock title;
                title.FontSize(13);
                title.MaxWidth(210);
                title.TextTrimming(TextTrimming::CharacterEllipsis);
                TextBlock dirty;
                dirty.Text(L"●");
                dirty.FontSize(8);
                dirty.VerticalAlignment(VerticalAlignment::Center);
                header.Children().Append(winrt::Folia::MakeWorkspaceIcon(winrt::Folia::WorkspaceIcon::Document));
                header.Children().Append(title);
                header.Children().Append(dirty);
                tab.Header(header);
                tab.DoubleTapped([weak = get_weak(), id = document->id](auto const&, auto const&) {
                    if (auto window = weak.get(); window && window->workspaceDocuments.Keep(id)) window->UpdateDocumentTab(id);
                });
                tab.Tapped([weak = get_weak(), id = document->id](auto const&, auto const&) {
                    if (auto window = weak.get(); window && window->settingsMode && window->IsActiveDocument(id)) window->SetSettingsMode(false);
                });
                items.InsertAt(index, tab);
                tabItems.emplace(document->id, tab);
            } else {
                tab = found->second;
                std::uint32_t at = 0;
                if (items.IndexOf(tab, at) && at != index) { items.RemoveAt(at); items.InsertAt(index, tab); }
            }
            UpdateDocumentTab(document->id);
            ++index;
        }
        if (auto active = tabItems.find(workspaceDocuments.Active()->id); active != tabItems.end()) DocumentTabs().SelectedItem(active->second);
        AppTitleBar().RecomputeDragRegions();
    }

    winrt::fire_and_forget MainWindow::CloseDocumentAsync(std::uint64_t id) {
        if (closingDocument || closePending || windowClosed) co_return;
        auto document = workspaceDocuments.Find(id);
        if (!document) co_return;
        auto lifetime = get_strong();
        closingDocument = true;
        try {
            if (IsActiveDocument(id)) textInputController.FocusLeave();
            if (co_await document->controller.ConfirmCloseAsync()) {
                if (IsActiveDocument(id)) {
                    auto next = workspaceDocuments.Adjacent(id, 1);
                    if (next == document) next = workspaceDocuments.New(winrt::Folia::WorkspaceOpenMode::Preview);
                    ActivateDocument(next->id);
                }
                workspaceDocuments.Remove(id);
                RefreshDocumentTabs();
            }
        } catch (winrt::hresult_error const& error) { SetStatus(LocalizeFormat(L"StatusSaveFailed", {error.message()})); }
        catch (std::exception const& error) { SetStatus(LocalizeFormat(L"StatusSaveFailed", {winrt::to_hstring(error.what())})); }
        closingDocument = false;
    }

    void MainWindow::RegisterWorkspaceHandlers() {
        using winrt::Folia::MakeWorkspaceIcon;
        using winrt::Folia::WorkspaceIcon;
        ExplorerPaneButton().Content(MakeWorkspaceIcon(WorkspaceIcon::PanelLeft));
        OutlinePaneButton().Content(MakeWorkspaceIcon(WorkspaceIcon::PanelRight));
        RefreshFolderButton().Content(MakeWorkspaceIcon(WorkspaceIcon::Refresh));
        CollapseFoldersButton().Content(MakeWorkspaceIcon(WorkspaceIcon::Collapse));
        CloseOutlineButton().Content(MakeWorkspaceIcon(WorkspaceIcon::PanelRight));
        Automation::AutomationProperties::SetName(ExplorerPaneButton(), Localize(L"Files"));
        Automation::AutomationProperties::SetName(OutlinePaneButton(), Localize(L"ToggleOutline"));
        Automation::AutomationProperties::SetName(RefreshFolderButton(), Localize(L"RefreshFiles"));
        Automation::AutomationProperties::SetName(CollapseFoldersButton(), Localize(L"CollapseFolders"));
        Automation::AutomationProperties::SetName(CloseOutlineButton(), Localize(L"CloseOutline"));
        ExplorerPaneButton().Click([this](auto const&, auto const&) {
            explorerOpen = ExplorerPaneButton().IsChecked().GetBoolean();
            UpdateWorkspaceLayout();
        });
        OutlinePaneButton().Click([this](auto const&, auto const&) {
            outlineOpen = OutlinePaneButton().IsChecked().GetBoolean();
            UpdateWorkspaceLayout();
        });
        DocumentWorkspace().PaneClosing([this](auto const&, auto const&) {
            explorerOpen = false;
            ExplorerPaneButton().IsChecked(explorerOpen);
            ExplorerToggleButton().IsChecked(explorerOpen);
        });
        OutlineSplitView().PaneClosing([this](auto const&, auto const&) {
            outlineOpen = false;
            OutlinePaneButton().IsChecked(outlineOpen);
            SidebarToggleButton().IsChecked(outlineOpen);
        });
        DocumentTabs().SelectionChanged([this](auto const&, auto const&) {
            if (updatingTabs || windowClosed) return;
            if (auto tab = DocumentTabs().SelectedItem().try_as<TabViewItem>()) ActivateDocument(winrt::unbox_value<std::uint64_t>(tab.Tag()));
        });
        DocumentTabs().AddTabButtonClick([this](auto const&, auto const&) { NewDocument(); });
        DocumentTabs().TabCloseRequested([this](auto const&, TabViewTabCloseRequestedEventArgs const& args) {
            CloseDocumentAsync(winrt::unbox_value<std::uint64_t>(args.Tab().Tag()));
        });
        DocumentTabs().TabDragCompleted([this](auto const&, auto const&) {
            std::vector<std::uint64_t> order;
            for (auto item : DocumentTabs().TabItems()) order.push_back(winrt::unbox_value<std::uint64_t>(item.as<TabViewItem>().Tag()));
            workspaceDocuments.Reorder(order);
            AppTitleBar().RecomputeDragRegions();
        });
        DocumentTabs().SizeChanged([this](auto const&, auto const&) { AppTitleBar().RecomputeDragRegions(); });
        AppTitleBar().PaneToggleRequested([this](auto const&, auto const&) {
            explorerOpen = !explorerOpen;
            ExplorerToggleButton().IsChecked(explorerOpen);
            UpdateWorkspaceLayout();
        });
        ExplorerToggleButton().Click([this](auto const&, auto const&) {
            explorerOpen = ExplorerToggleButton().IsChecked();
            UpdateWorkspaceLayout();
        });
        CloseOutlineButton().Click([this](auto const&, auto const&) {
            outlineOpen = false;
            SidebarToggleButton().IsChecked(false);
            UpdateWorkspaceLayout();
        });
        folderExplorer.Attach(FileTree(), [weak = get_weak()](auto path, bool keep) {
            if (auto window = weak.get()) window->OpenWorkspaceFile(std::move(path), keep ? winrt::Folia::WorkspaceOpenMode::Keep : winrt::Folia::WorkspaceOpenMode::Preview);
        }, [weak = get_weak()](auto error) {
            if (auto window = weak.get()) window->SetStatus(LocalizeFormat(L"StatusFolderFailed", {error}));
        });
        RefreshFolderButton().Click([this](auto const&, auto const&) { folderExplorer.Refresh(); });
        CollapseFoldersButton().Click([this](auto const&, auto const&) { folderExplorer.Collapse(); });
        OpenFolderPaneButton().Click([this](auto const&, auto const&) { PickWorkspaceFolder(); });
        RefreshDocumentTabs();
        RegisterApplicationShortcuts();
        UpdateWorkspaceLayout();
    }

    void MainWindow::UpdateWorkspaceLayout() {
        auto width = WorkspaceHost().ActualWidth();
        DocumentWorkspace().OpenPaneLength(248);
        DocumentWorkspace().DisplayMode(width > 0 && width < 760 ? SplitViewDisplayMode::Overlay : SplitViewDisplayMode::Inline);
        OutlineSplitView().DisplayMode(width > 0 && width < 1040 ? SplitViewDisplayMode::Overlay : SplitViewDisplayMode::Inline);
        DocumentWorkspace().IsPaneOpen(explorerOpen);
        OutlineSplitView().IsPaneOpen(outlineOpen);
        ExplorerPaneButton().IsChecked(explorerOpen);
        OutlinePaneButton().IsChecked(outlineOpen);
        ExplorerToggleButton().IsChecked(explorerOpen);
        SidebarToggleButton().IsChecked(outlineOpen);
    }
}
