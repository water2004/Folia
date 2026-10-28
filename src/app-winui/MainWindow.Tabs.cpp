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
        auto tab = DocumentTabs().ContainerFromItem(found->second->item).try_as<TabViewItem>();
        if (!tab) return;
        auto header = tab.Header().try_as<StackPanel>();
        if (!header) return;
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
        auto items = tabSource;
        for (std::uint32_t index = items.Size(); index > 0; --index) {
            auto id = winrt::unbox_value<std::uint64_t>(items.GetAt(index - 1));
            if (!workspaceDocuments.Find(id)) { items.RemoveAt(index - 1); tabItems.erase(id); }
        }
        std::uint32_t index = 0;
        for (auto const& document : workspaceDocuments.Items()) {
            auto found = tabItems.find(document->id);
            if (found == tabItems.end()) {
                auto binding = std::make_shared<DocumentTab>();
                binding->owner = get_weak();
                binding->item = winrt::box_value(document->id);
                tabItems.emplace(document->id, binding);
                items.InsertAt(index, binding->item);
            } else {
                auto item = found->second->item;
                std::uint32_t at = 0;
                if (items.IndexOf(item, at) && at != index) { items.RemoveAt(at); items.InsertAt(index, item); }
            }
            UpdateDocumentTab(document->id);
            ++index;
        }
        SelectDocumentTab();
        UpdateChromeMetrics();
        EnsureSelectedTabVisible();
    }

    void MainWindow::SelectDocumentTab() {
        auto active = tabItems.find(workspaceDocuments.Active()->id);
        if (active != tabItems.end() && DocumentTabs().SelectedItem() != active->second->item)
            DocumentTabs().SelectedItem(active->second->item);
        EnsureSelectedTabVisible();
    }

    void MainWindow::DocumentTabLoaded(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&) {
        if (windowClosed) return;
        UpdateDocumentTab(winrt::unbox_value<std::uint64_t>(sender.as<TabViewItem>().DataContext()));
        EnsureSelectedTabVisible();
    }
    void MainWindow::DocumentTabPrepared(FrameworkElement const& sender, DataContextChangedEventArgs const&) {
        if (windowClosed) return;
        auto tab = sender.as<TabViewItem>();
        if (!tab.DataContext()) return;
        auto id = winrt::unbox_value<std::uint64_t>(tab.DataContext());
        auto found = tabItems.find(id);
        if (found == tabItems.end()) return;
        tab.Tag(found->second->item);
        if (!tab.Header().try_as<StackPanel>()) {
            tab.Style(Root().Resources().Lookup(winrt::box_value(L"WorkspaceTabStyle")).as<Style>());
            tab.BorderBrush(Root().Resources().Lookup(winrt::box_value(L"FoliaShellAccentBrush")).as<Media::Brush>());
            StackPanel header;
            header.Orientation(Orientation::Horizontal);
            header.Spacing(8);
            TextBlock title;
            title.FontSize(14);
            title.VerticalAlignment(VerticalAlignment::Center);
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
            tab.DoubleTapped(WindowEvent([this](auto const& sender, auto const&) {
                auto id = winrt::unbox_value<std::uint64_t>(sender.template as<TabViewItem>().DataContext());
                if (workspaceDocuments.Keep(id)) UpdateDocumentTab(id);
            }));
            tab.Tapped(WindowEvent([this](auto const& sender, auto const&) {
                auto id = winrt::unbox_value<std::uint64_t>(sender.template as<TabViewItem>().DataContext());
                if (settingsMode && IsActiveDocument(id)) SetSettingsMode(false);
            }));
        }
        UpdateDocumentTab(id);
        EnsureSelectedTabVisible();
    }

    winrt::fire_and_forget MainWindow::CloseDocumentAsync(std::uint64_t id) {
        if (closingDocument || closePending || windowClosed) co_return;
        auto document = workspaceDocuments.Find(id);
        if (!document) co_return;
        auto lifetime = get_strong();
        closingDocument = true;
        UpdateLoadingState();
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
        if (!windowClosed) UpdateLoadingState();
    }

    void MainWindow::RegisterWorkspaceHandlers() {
        using winrt::Folia::MakeWorkspaceIcon;
        using winrt::Folia::WorkspaceIcon;
        ExplorerPaneButton().Content(MakeWorkspaceIcon(WorkspaceIcon::PanelLeft));
        OutlinePaneButton().Content(MakeWorkspaceIcon(WorkspaceIcon::PanelRight));
        Automation::AutomationProperties::SetName(ExplorerPaneButton(), Localize(L"Files"));
        Automation::AutomationProperties::SetName(OutlinePaneButton(), Localize(L"ToggleOutline"));
        ExplorerPaneButton().Click(WindowEvent([this](auto const&, auto const&) {
            explorerOpen = ExplorerPaneButton().IsChecked().GetBoolean();
            UpdateWorkspaceLayout();
        }));
        OutlinePaneButton().Click(WindowEvent([this](auto const&, auto const&) {
            outlineOpen = OutlinePaneButton().IsChecked().GetBoolean();
            UpdateWorkspaceLayout();
        }));
        DocumentWorkspace().PaneClosing(WindowEvent([this](auto const&, auto const&) {
            explorerOpen = false;
            ExplorerPaneButton().IsChecked(explorerOpen);
            ExplorerToggleButton().IsChecked(explorerOpen);
        }));
        OutlineSplitView().PaneClosing(WindowEvent([this](auto const&, auto const&) {
            outlineOpen = false;
            OutlinePaneButton().IsChecked(outlineOpen);
            SidebarToggleButton().IsChecked(outlineOpen);
        }));
        DocumentTabs().TabItemsSource(tabSource);
        DocumentTabs().SelectionChanged(WindowEvent([this](auto const&, auto const&) {
            if (updatingTabs || windowClosed) return;
            if (auto item = DocumentTabs().SelectedItem()) ActivateDocument(winrt::unbox_value<std::uint64_t>(item));
        }));
        DocumentTabs().AddTabButtonClick(WindowEvent([this](auto const&, auto const&) { NewDocument(); }));
        DocumentTabs().TabCloseRequested(WindowEvent([this](auto const&, TabViewTabCloseRequestedEventArgs const& args) {
            CloseDocumentAsync(winrt::unbox_value<std::uint64_t>(args.Item()));
        }));
        RegisterTabTransferHandlers();
        DocumentTabs().SizeChanged(WindowEvent([this](auto const&, auto const&) {
            UpdateChromeMetrics();
            EnsureSelectedTabVisible();
        }));
        ExplorerToggleButton().Click(WindowEvent([this](auto const&, auto const&) {
            explorerOpen = ExplorerToggleButton().IsChecked();
            UpdateWorkspaceLayout();
        }));
        folderExplorer.Attach(FileTree(), [weak = get_weak()](auto path, bool keep) {
            if (auto window = weak.get()) window->OpenWorkspaceFile(std::move(path), keep ? winrt::Folia::WorkspaceOpenMode::Keep : winrt::Folia::WorkspaceOpenMode::Preview);
        }, [weak = get_weak()](auto error) {
            if (auto window = weak.get()) window->SetStatus(LocalizeFormat(L"StatusFolderFailed", {error}));
        });
        folderExplorer.SetOperationHandler([weak = get_weak()](winrt::Folia::WorkspaceFileAction action) -> Windows::Foundation::IAsyncOperation<winrt::hstring> {
            auto window = weak.get();
            if (!window) throw winrt::hresult_illegal_method_call();
            co_return co_await window->ApplyExplorerActionAsync(std::move(action));
        });
        OpenFolderPaneButton().Click(WindowEvent([this](auto const&, auto const&) { PickWorkspaceFolder(); }));
        RefreshDocumentTabs();
        RegisterApplicationShortcuts();
        UpdateWorkspaceLayout();
    }

    void MainWindow::EnsureSelectedTabVisible() {
        if (selectedTabVisibilityPending || windowClosed) return;
        selectedTabVisibilityPending = true;
        // Let TabView finish its selection/overflow layout first. Coalesce
        // requests and resolve the current selection when the callback runs.
        if (!DispatcherQueue().TryEnqueue(Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [weak = get_weak()] {
            if (auto window = weak.get()) {
                window->selectedTabVisibilityPending = false;
                if (window->windowClosed) return;
                if (auto selected = window->DocumentTabs().ContainerFromItem(window->DocumentTabs().SelectedItem()).try_as<TabViewItem>(); selected && selected.IsLoaded()) {
                    BringIntoViewOptions options;
                    options.AnimationDesired(false);
                    selected.StartBringIntoView(options);
                }
            }
        })) selectedTabVisibilityPending = false;
    }

    void MainWindow::UpdateChromeMetrics() {
        auto root = Root().XamlRoot();
        const double scale = root ? root.RasterizationScale() : static_cast<double>(::GetDpiForWindow(WindowHandle())) / 96.0;
        CaptionInset().Width(AppWindow().TitleBar().RightInset() / (std::max)(1.0, scale));
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
