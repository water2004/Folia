#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.Graphics.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    void MainWindow::UpdateDocumentTab(std::uint64_t id) {
        auto document = workspaceDocuments.Find(id);
        auto found = tabItems.find(id);
        if (!document || found == tabItems.end()) return;
        auto tab = DocumentTabs().ContainerFromItem(found->second->item).try_as<TabViewItem>();
        if (!tab) return;
        auto header = tab.Header().try_as<Grid>();
        if (!header) return;
        auto iconHost = header.Children().GetAt(0).as<ContentControl>();
        const bool settings = document->kind == WorkspaceTabKind::Settings;
        auto icon = document->kind == WorkspaceTabKind::Image ? WorkspaceIcon::Image : document->path.empty()
            ? WorkspaceIcon::Markdown : WorkspaceFileIcon(ClassifyWorkspaceFile(document->path));
        auto iconId = settings ? -2 : static_cast<int>(icon);
        if (winrt::unbox_value_or<int>(iconHost.Tag(), -1) != iconId) {
            if (settings) {
                iconHost.Content(MakeWorkspaceIcon(WorkspaceIcon::Settings));
            } else iconHost.Content(MakeWorkspaceIcon(icon));
            iconHost.Tag(winrt::box_value(iconId));
        }
        auto title = header.Children().GetAt(1).as<TextBlock>();
        auto dirty = header.Children().GetAt(2).as<TextBlock>();
        auto name = settings ? Localize(L"Settings") : document->path.empty()
            ? Localize(L"UntitledMarkdown") : winrt::hstring(document->path.filename().c_str());
        auto label = name;
        for (auto const& other : workspaceDocuments.Items()) {
            if (other != document && !other->path.empty() && other->path.filename() == document->path.filename()) {
                label = name + L" · " + winrt::hstring(document->path.parent_path().filename().c_str());
                break;
            }
        }
        if (title.Text() != label) title.Text(label);
        title.FontStyle(document->kept ? Windows::UI::Text::FontStyle::Normal : Windows::UI::Text::FontStyle::Italic);
        dirty.Visibility(document->IsModified() ? Visibility::Visible : Visibility::Collapsed);
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
    }

    void MainWindow::SelectDocumentTab() {
        auto active = tabItems.find(workspaceDocuments.Active()->id);
        if (active != tabItems.end() && DocumentTabs().SelectedItem() != active->second->item)
            DocumentTabs().SelectedItem(active->second->item);
        UpdateSelectedTabBackdrop();
        EnsureSelectedTabVisible();
    }

    void MainWindow::DocumentTabLoaded(Windows::Foundation::IInspectable const& sender, RoutedEventArgs const&) {
        if (windowClosed) return;
        auto tab = sender.as<TabViewItem>();
        auto id = winrt::unbox_value<std::uint64_t>(tab.DataContext());
        auto item = tabItems.find(id);
        if (item != tabItems.end())
            if (auto close = tab.as<IControlProtected>().GetTemplateChild(L"CloseButton").try_as<Button>())
                item->second->closeButtonSizeChanged = close.SizeChanged(winrt::auto_revoke,
                    WindowEvent([this](auto const&, auto const&) { RefreshTabCaptionRegion(); }));
        UpdateDocumentTab(id);
        RefreshTabCaptionRegion();
        UpdateSelectedTabBackdrop();
    }

    void MainWindow::DocumentTabPrepared(FrameworkElement const& sender, DataContextChangedEventArgs const&) {
        if (windowClosed) return;
        auto tab = sender.as<TabViewItem>();
        if (!tab.DataContext()) return;
        auto id = winrt::unbox_value<std::uint64_t>(tab.DataContext());
        auto found = tabItems.find(id);
        if (found == tabItems.end()) return;
        tab.Tag(found->second->item);
        UpdateDocumentTab(id);
    }

    void MainWindow::DocumentTabDoubleTapped(Windows::Foundation::IInspectable const& sender, Input::DoubleTappedRoutedEventArgs const&) {
        if (windowClosed) return;
        auto id = winrt::unbox_value<std::uint64_t>(sender.as<TabViewItem>().Tag());
        if (workspaceDocuments.Keep(id)) UpdateDocumentTab(id);
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
            if (!document->editor || co_await document->editor->controller.ConfirmCloseAsync()) {
                if (IsActiveDocument(id)) {
                    auto next = SettingsActive() && previousDocumentTab ? workspaceDocuments.Find(*previousDocumentTab) : nullptr;
                    if (!next) next = workspaceDocuments.Adjacent(id, 1);
                    if (next == document) next = workspaceDocuments.New(winrt::Folia::WorkspaceOpenMode::Preview);
                    ActivateDocument(next->id);
                }
                workspaceDocuments.Remove(id);
                RefreshDocumentTabs();
            }
        } catch (winrt::hresult_error const& error) { ShowError(LocalizeFormat(L"StatusSaveFailed", {error.message()})); }
        catch (std::exception const& error) { ShowError(LocalizeFormat(L"StatusSaveFailed", {winrt::to_hstring(error.what())})); }
        closingDocument = false;
        if (!windowClosed) UpdateLoadingState();
    }

    ScrollViewer MainWindow::TabStripScroller() {
        auto list = DocumentTabs().as<IControlProtected>().GetTemplateChild(L"TabListView").try_as<Control>();
        return list ? list.as<IControlProtected>().GetTemplateChild(L"ScrollViewer").try_as<ScrollViewer>() : nullptr;
    }

    Button MainWindow::NewTabButton() {
        return DocumentTabs().as<IControlProtected>().GetTemplateChild(L"AddButton").try_as<Button>();
    }

    bool MainWindow::ScrollTabsWithWheel(int delta) {
        auto view = TabStripScroller();
        if (!view || !delta || view.ScrollableWidth() <= 0) return false;
        auto offset = std::clamp(view.HorizontalOffset() - delta * view.ViewportWidth() / (3. * WHEEL_DELTA),
            0., view.ScrollableWidth());
        if (offset != view.HorizontalOffset()) view.ChangeView(offset, nullptr, nullptr, true);
        return true;
    }

    void MainWindow::RegisterWorkspaceHandlers() {
        OutlineList().Items().VectorChanged(WindowEvent([this](auto const&, auto const&) { UpdateWorkspaceLayout(); }));
        DocumentTabs().TabItemsSource(tabSource);
        DocumentTabs().AddHandler(UIElement::PointerWheelChangedEvent(), box_value(Input::PointerEventHandler(
            WindowEvent([this](auto const&, Input::PointerRoutedEventArgs const& args) {
                auto properties = args.GetCurrentPoint(TabViewport()).Properties();
                if ((!properties.IsHorizontalMouseWheel() || !args.Handled()) && ScrollTabsWithWheel(properties.MouseWheelDelta()))
                    args.Handled(true);
            }))), true);
        DocumentTabs().Loaded(WindowEvent([this](auto const&, auto const&) {
            if (auto button = NewTabButton()) {
                button.Content(MakeWorkspaceIcon(WorkspaceIcon::Add));
                auto label = Localize(L"NewDocument");
                Automation::AutomationProperties::SetName(button, label);
                ToolTipService::SetToolTip(button, box_value(label));
            }
            if (auto list = DocumentTabs().as<IControlProtected>().GetTemplateChild(L"TabListView").try_as<FrameworkElement>())
                list.SizeChanged(WindowEvent([this](auto const&, auto const&) { RefreshTabCaptionRegion(); }));
            if (auto viewer = TabStripScroller()) {
                viewer.ViewChanged(WindowEvent([this](auto const&, auto const&) {
                    UpdateSelectedTabBackdrop();
                    RefreshTabCaptionRegion();
                }));
                // TabView handles ViewChanged, but resizing the viewport at offset
                // zero can expose more tabs without raising that event.
                viewer.RegisterPropertyChangedCallback(ScrollViewer::ScrollableWidthProperty(), [](DependencyObject const& sender, DependencyProperty const&) {
                    auto scroll = sender.as<ScrollViewer>();
                    scroll.DispatcherQueue().TryEnqueue([weak = winrt::make_weak(scroll)] {
                        if (auto view = weak.get(); view && view.IsLoaded()) {
                            auto parts = view.as<IControlProtected>();
                            auto back = parts.GetTemplateChild(L"ScrollDecreaseButton").try_as<Primitives::RepeatButton>();
                            auto next = parts.GetTemplateChild(L"ScrollIncreaseButton").try_as<Primitives::RepeatButton>();
                            if (back) back.IsEnabled(view.HorizontalOffset() > 0.1);
                            if (next) next.IsEnabled(view.ScrollableWidth() - view.HorizontalOffset() > 0.1);
                        }
                    });
                });
            }
        }));
        DocumentTabs().SelectionChanged(WindowEvent([this](auto const&, auto const&) {
            RefreshTabCaptionRegion();
            if (updatingTabs || windowClosed) return;
            if (auto item = DocumentTabs().SelectedItem()) ActivateDocument(winrt::unbox_value<std::uint64_t>(item));
        }));
        DocumentTabs().TabCloseRequested(WindowEvent([this](auto const&, TabViewTabCloseRequestedEventArgs const& args) {
            CloseDocumentAsync(winrt::unbox_value<std::uint64_t>(args.Item()));
        }));
        RegisterTabTransferHandlers();
        TabViewport().SizeChanged(WindowEvent([this](auto const&, SizeChangedEventArgs const& args) {
            auto size = args.NewSize();
            TabViewportClip().Rect({0, 0, size.Width, size.Height});
            UpdateChromeMetrics();
            EnsureSelectedTabVisible();
            RefreshTabCaptionRegion();
            UpdateSelectedTabBackdrop();
        }));
        folderExplorer.Attach(FileTree(), [weak = get_weak()](auto path, bool keep) {
            if (auto window = weak.get()) window->OpenWorkspaceFile(std::move(path), keep ? winrt::Folia::WorkspaceOpenMode::Keep : winrt::Folia::WorkspaceOpenMode::Preview);
        }, [weak = get_weak()](auto error) {
            if (auto window = weak.get()) window->ShowError(LocalizeFormat(L"StatusFolderFailed", {error}));
        }, FolderHeader());
        folderExplorer.SetOperationHandler([weak = get_weak()](folia::platform::WorkspaceFileAction action) -> Windows::Foundation::IAsyncOperation<winrt::hstring> {
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

}
