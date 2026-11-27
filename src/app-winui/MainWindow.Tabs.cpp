#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "App.xaml.h"
#include "localization/Localization.h"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.Graphics.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    namespace {
        struct TabScrollArrow {
            wchar_t const* part;
            wchar_t const* label;
            double direction;
        };
        constexpr std::array tabScrollArrows{
            TabScrollArrow{L"PreviousTabScrollArrow", L"ScrollTabsLeft", -1.},
            TabScrollArrow{L"NextTabScrollArrow", L"ScrollTabsRight", 1.},
        };

        bool ScrollTabStrip(ScrollViewer const& viewer, double steps) {
            if (!viewer || !steps || viewer.ScrollableWidth() <= 0.) return false;
            const auto offset = std::clamp(viewer.HorizontalOffset() + steps * viewer.ViewportWidth() / 3.,
                0., viewer.ScrollableWidth());
            if (offset != viewer.HorizontalOffset()) viewer.ChangeView(offset, nullptr, nullptr, true);
            return true;
        }

        void UpdateTabScrollArrows(ScrollViewer const& viewer) {
            auto parts = viewer.as<IControlProtected>();
            const auto offset = viewer.HorizontalOffset();
            const auto end = viewer.ScrollableWidth();
            for (auto const& arrow : tabScrollArrows)
                if (auto button = parts.GetTemplateChild(arrow.part).try_as<Primitives::RepeatButton>())
                    button.IsEnabled(end > .1 && (arrow.direction < 0. ? offset : end - offset) > .1);
        }
    }

    void MainWindow::UpdateDocumentTab(std::uint64_t id) {
        auto document = workspaceDocuments.Find(id);
        if (!document) return;
        for (auto group : {EditorGroup::Left, EditorGroup::Right}) {
            for (auto const& view : editorGroups.Views(group)) {
                if (view.document != id) continue;
                auto found = tabItems.find(view.id);
                if (found != tabItems.end())
                    if (auto tab = GroupTabs(group).ContainerFromItem(found->second->item).try_as<TabViewItem>())
                        UpdateTabHeader(tab, *document);
            }
        }
    }

    void MainWindow::UpdateTabHeader(TabViewItem const& tab, WorkspaceDocument const& document) {
        auto header = tab.Header().try_as<Grid>();
        if (!header) return;
        auto iconHost = header.Children().GetAt(0).as<ContentControl>();
        const bool settings = document.kind == WorkspaceTabKind::Settings;
        const bool chat = document.kind == WorkspaceTabKind::Chat;
        auto icon = chat ? WorkspaceIcon::Chat : document.kind == WorkspaceTabKind::Image ? WorkspaceIcon::Image : document.path.empty()
            ? WorkspaceIcon::Markdown : WorkspaceFileIcon(ClassifyWorkspaceFile(document.path));
        auto iconId = settings ? -2 : static_cast<int>(icon);
        if (winrt::unbox_value_or<int>(iconHost.Tag(), -1) != iconId) {
            if (settings) {
                iconHost.Content(MakeWorkspaceIcon(WorkspaceIcon::Settings));
            } else iconHost.Content(MakeWorkspaceIcon(icon));
            iconHost.Tag(winrt::box_value(iconId));
        }
        auto title = header.Children().GetAt(1).as<TextBlock>();
        auto dirty = header.Children().GetAt(2).as<TextBlock>();
        auto name = chat ? to_hstring(document.chatTitle) : settings ? Localize(L"Settings") : document.path.empty()
            ? Localize(L"UntitledMarkdown") : winrt::hstring(document.path.filename().c_str());
        auto label = name;
        for (auto const& other : workspaceDocuments.Items()) {
            if (other.get() != &document && !other->path.empty() && other->path.filename() == document.path.filename()) {
                label = name + L" · " + winrt::hstring(document.path.parent_path().filename().c_str());
                break;
            }
        }
        if (title.Text() != label) title.Text(label);
        title.FontStyle(document.kept ? Windows::UI::Text::FontStyle::Normal : Windows::UI::Text::FontStyle::Italic);
        dirty.Visibility(document.IsModified() ? Visibility::Visible : Visibility::Collapsed);
        ToolTipService::SetToolTip(tab, winrt::box_value(document.path.empty() ? name : winrt::hstring(document.path.c_str())));
        Automation::AutomationProperties::SetName(tab, label);
    }

    void MainWindow::RefreshDocumentTabs() {
        if (updatingTabs || windowClosed) return;
        ReleaseClosedAgentChats();
        updatingTabs = true;
        struct Reset { bool& value; ~Reset() { value = false; } } reset{updatingTabs};
        std::vector<std::uint64_t> removed;
        for (auto group : {EditorGroup::Left, EditorGroup::Right})
            for (auto const& view : editorGroups.Views(group))
                if (!workspaceDocuments.Find(view.document)) removed.push_back(view.id);
        for (auto view : removed) editorGroups.Remove(view);
        for (auto const& document : workspaceDocuments.Items())
            if (!editorGroups.ViewCount(document->id)) editorGroups.Add(editorGroups.ActiveGroup(), document->id);

        for (auto group : {EditorGroup::Left, EditorGroup::Right}) {
            auto items = group == EditorGroup::Left ? tabSource : rightTabSource;
            for (std::uint32_t index = items.Size(); index > 0; --index) {
                auto view = winrt::unbox_value<std::uint64_t>(items.GetAt(index - 1));
                if (editorGroups.GroupOf(view) != group) items.RemoveAt(index - 1);
            }
        }
        std::erase_if(tabItems, [this](auto const& entry) { return !editorGroups.Find(entry.first); });
        for (auto group : {EditorGroup::Left, EditorGroup::Right}) {
            auto items = group == EditorGroup::Left ? tabSource : rightTabSource;
            std::uint32_t index = 0;
            for (auto const& view : editorGroups.Views(group)) {
                auto& binding = tabItems[view.id];
                if (!binding) {
                    binding = std::make_shared<DocumentTab>();
                    binding->owner = get_weak();
                    binding->document = view.document;
                    binding->item = winrt::box_value(view.id);
                }
                auto item = binding->item;
                std::uint32_t at = 0;
                if (!items.IndexOf(item, at)) items.InsertAt(index, item);
                else if (at != index) { items.RemoveAt(at); items.InsertAt(index, item); }
                ++index;
            }
        }
        for (auto const& document : workspaceDocuments.Items()) UpdateDocumentTab(document->id);
        SelectDocumentTab();
        UpdateChromeMetrics();
        UpdateEditorGroupLayout();
    }

    void MainWindow::SelectDocumentTab() {
        const auto previous = std::exchange(updatingTabs, true);
        struct Restore { bool& value; bool previous; ~Restore() { value = previous; } } restore{updatingTabs, previous};
        for (auto group : {EditorGroup::Left, EditorGroup::Right}) {
            auto selection = tabItems.find(editorGroups.Selected(group));
            auto tabs = GroupTabs(group);
            if (selection == tabItems.end()) continue;
            auto const& item = selection->second->item;
            auto items = group == EditorGroup::Left ? tabSource : rightTabSource;
            std::uint32_t index{};
            // TabView accepts SelectedItem before it belongs to the strip, but its
            // internal list then keeps the old selection and swallows the next click.
            if (items.IndexOf(item, index) && (tabs.SelectedItem() != item || tabs.SelectedIndex() != static_cast<int>(index)))
                tabs.SelectedIndex(static_cast<int>(index));
        }
        UpdateSelectedTabBackdrop();
        UpdateReadingGroupCorners();
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
        if (item != tabItems.end())
            if (auto document = workspaceDocuments.Find(item->second->document)) UpdateTabHeader(tab, *document);
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
        if (auto document = workspaceDocuments.Find(found->second->document)) UpdateTabHeader(tab, *document);
    }

    void MainWindow::DocumentTabDoubleTapped(Windows::Foundation::IInspectable const& sender, Input::DoubleTappedRoutedEventArgs const&) {
        if (windowClosed) return;
        auto view = editorGroups.Find(winrt::unbox_value<std::uint64_t>(sender.as<TabViewItem>().Tag()));
        if (view && workspaceDocuments.Keep(view->document)) UpdateDocumentTab(view->document);
    }

    winrt::fire_and_forget MainWindow::CloseDocumentAsync(std::uint64_t id) {
        CloseEditorViewAsync(FindDocumentView(id));
        co_return;
    }

    winrt::fire_and_forget MainWindow::CloseEditorViewAsync(std::uint64_t view) {
        if (closingDocument || closePending || windowClosed) co_return;
        auto entry = editorGroups.Find(view);
        if (!entry) co_return;
        auto id = entry->document;
        auto document = workspaceDocuments.Find(id);
        if (!document) co_return;
        if (editorGroups.ViewCount(id) > 1) {
            if (presentedEditorView == view) ActivateView(editorGroups.Replacement(view));
            editorGroups.Remove(view);
            RefreshDocumentTabs();
            co_return;
        }
        auto lifetime = get_strong();
        closingDocument = true;
        UpdateLoadingState();
        try {
            if (presentedEditorView == view) textInputController.FocusLeave();
            if (!document->editor || App::Current()->OtherDocumentWindow(this, id) || co_await document->editor->controller.ConfirmCloseAsync()) {
                if (presentedEditorView == view) {
                    auto next = SettingsActive() && previousDocumentTab ? FindDocumentView(*previousDocumentTab) : 0;
                    if (!next) next = editorGroups.Replacement(view);
                    if (next) ActivateView(next);
                    else ActivateDocument(workspaceDocuments.New(winrt::Folia::WorkspaceOpenMode::Preview)->id);
                }
                editorGroups.Remove(view);
                workspaceDocuments.Remove(id);
                RefreshDocumentTabs();
            }
        } catch (winrt::hresult_error const& error) { ShowError(LocalizeFormat(L"StatusSaveFailed", {error.message()})); }
        catch (std::exception const& error) { ShowError(LocalizeFormat(L"StatusSaveFailed", {winrt::to_hstring(error.what())})); }
        closingDocument = false;
        if (!windowClosed) UpdateLoadingState();
    }

    bool MainWindow::BeginTabMiddlePress(Windows::Graphics::PointInt32 point) {
        if (windowClosed || tabMoveInProgress || ::GetCapture()) return false;
        auto hit = HitTestVisibleTab(point);
        if (!hit || !hit->closable) return false;

        auto window = WindowHandle();
        ::SetCapture(window);
        if (::GetCapture() != window) return false;
        middlePressedTab = hit->id;
        return true;
    }

    bool MainWindow::ReleaseTabMiddlePress(Windows::Graphics::PointInt32 point) {
        auto pressed = middlePressedTab;
        if (!pressed) return false;
        CancelTabMiddlePress();
        if (windowClosed) return true;

        if (auto hit = HitTestVisibleTab(point); hit && hit->closable && hit->id == *pressed) {
            DispatcherQueue().TryEnqueue([weak = get_weak(), tab = *pressed] {
                if (auto target = weak.get()) target->CloseEditorViewAsync(tab);
            });
        }
        return true;
    }

    void MainWindow::CancelTabMiddlePress() {
        if (std::exchange(middlePressedTab, std::nullopt) && ::GetCapture() == WindowHandle()) ::ReleaseCapture();
    }

    TabView MainWindow::GroupTabs(EditorGroup group) {
        return group == EditorGroup::Left ? DocumentTabs() : RightDocumentTabs();
    }

    ScrollViewer MainWindow::TabStripScroller(EditorGroup group) {
        auto list = GroupTabs(group).as<IControlProtected>().GetTemplateChild(L"TabListView").try_as<Control>();
        return list ? list.as<IControlProtected>().GetTemplateChild(L"ScrollViewer").try_as<ScrollViewer>() : nullptr;
    }

    Button MainWindow::NewTabButton(EditorGroup group) {
        return GroupTabs(group).as<IControlProtected>().GetTemplateChild(L"AddButton").try_as<Button>();
    }

    bool MainWindow::ScrollTabsWithWheel(int delta, EditorGroup group) {
        return ScrollTabStrip(TabStripScroller(group), -static_cast<double>(delta) / WHEEL_DELTA);
    }

    void MainWindow::RegisterGroupTabs(EditorGroup group) {
        auto tabs = GroupTabs(group);
        tabs.TabItemsSource(group == EditorGroup::Left ? tabSource : rightTabSource);
        tabs.AddTabButtonClick(WindowEvent([this, group](auto const&, auto const&) { NewDocumentInGroup(group); }));
        tabs.AddHandler(UIElement::PointerWheelChangedEvent(), box_value(Input::PointerEventHandler(
            WindowEvent([this, group](auto const& sender, Input::PointerRoutedEventArgs const& args) {
                auto properties = args.GetCurrentPoint(sender.template as<UIElement>()).Properties();
                if ((!properties.IsHorizontalMouseWheel() || !args.Handled()) && ScrollTabsWithWheel(properties.MouseWheelDelta(), group))
                    args.Handled(true);
            }))), true);
        tabs.Loaded(WindowEvent([this, group](auto const&, auto const&) { PrepareGroupTabStrip(group); }));
        tabs.SelectionChanged(WindowEvent([this, group](auto const&, auto const&) {
            RefreshTabCaptionRegion();
            if (updatingTabs || windowClosed) return;
            if (auto item = GroupTabs(group).SelectedItem()) ActivateView(winrt::unbox_value<std::uint64_t>(item));
        }));
        tabs.TabCloseRequested(WindowEvent([this](auto const&, TabViewTabCloseRequestedEventArgs const& args) {
            CloseEditorViewAsync(winrt::unbox_value<std::uint64_t>(args.Item()));
        }));
        auto viewport = group == EditorGroup::Left ? TabViewport() : RightTabViewport();
        viewport.SizeChanged(WindowEvent([this, group](auto const&, SizeChangedEventArgs const& args) {
            PrepareGroupTabStrip(group);
            auto size = args.NewSize();
            auto clip = group == EditorGroup::Left ? TabViewportClip() : RightTabViewportClip();
            clip.Rect({0, 0, size.Width, size.Height});
            UpdateChromeMetrics();
            EnsureSelectedTabVisible();
            RefreshTabCaptionRegion();
            UpdateSelectedTabBackdrop();
        }));
    }

    void MainWindow::PrepareGroupTabStrip(EditorGroup group) {
        auto tabs = GroupTabs(group);
        if (!tabs.IsLoaded()) return;
        tabs.ApplyTemplate();
        auto control = tabs.as<IControlProtected>();
        if (group == EditorGroup::Right) {
            // WinUI leaves room for the first tab's lower arc. This strip starts at the reading card's edge.
            if (auto leading = control.GetTemplateChild(L"LeftContentColumn").try_as<ColumnDefinition>(); leading && leading.MinWidth() != 0.)
                leading.MinWidth(0.);
        }
        if (auto button = NewTabButton(group); button && !button.Content().try_as<Viewbox>()) {
            button.Content(MakeWorkspaceIcon(WorkspaceIcon::Add));
            auto label = Localize(L"NewDocument");
            Automation::AutomationProperties::SetName(button, label);
            ToolTipService::SetToolTip(button, box_value(label));
        }
        auto& parts = tabStripParts[static_cast<std::size_t>(group)];
        if (auto list = control.GetTemplateChild(L"TabListView").try_as<FrameworkElement>(); list && list != parts.list.get()) {
            parts.list = make_weak(list);
            list.SizeChanged(WindowEvent([this](auto const&, auto const&) { RefreshTabCaptionRegion(); }));
        }
        auto viewer = TabStripScroller(group);
        if (!viewer) return;
        viewer.ApplyTemplate();
        auto scrollParts = viewer.as<IControlProtected>();
        for (std::size_t index = 0; index < tabScrollArrows.size(); ++index) {
            auto const& arrow = tabScrollArrows[index];
            auto button = scrollParts.GetTemplateChild(arrow.part).try_as<Primitives::RepeatButton>();
            if (!button || button == parts.arrows[index].get()) continue;
            parts.arrows[index] = make_weak(button);
            auto label = Localize(arrow.label);
            Automation::AutomationProperties::SetName(button, label);
            ToolTipService::SetToolTip(button, box_value(label));
            button.Click([weak = make_weak(viewer), direction = arrow.direction](auto const&, auto const&) {
                if (auto scroll = weak.get()) ScrollTabStrip(scroll, direction);
            });
        }
        UpdateTabScrollArrows(viewer);
        if (viewer == parts.scroller.get()) return;
        parts.scroller = make_weak(viewer);
        viewer.ViewChanged(WindowEvent([this](auto const& sender, auto const&) {
            UpdateTabScrollArrows(sender.template as<ScrollViewer>());
            UpdateReadingGroupCorners();
            UpdateSelectedTabBackdrop();
            RefreshTabCaptionRegion();
        }));
        // Width can change at offset zero without a ViewChanged notification.
        viewer.RegisterPropertyChangedCallback(ScrollViewer::ScrollableWidthProperty(), [](DependencyObject const& sender, DependencyProperty const&) {
            auto scroll = sender.as<ScrollViewer>();
            scroll.DispatcherQueue().TryEnqueue([weak = winrt::make_weak(scroll)] {
                if (auto view = weak.get(); view && view.IsLoaded()) UpdateTabScrollArrows(view);
            });
        });
    }

    void MainWindow::RegisterWorkspaceHandlers() {
        OutlineList().Items().VectorChanged(WindowEvent([this](auto const&, auto const&) { UpdateWorkspaceLayout(); }));
        RegisterEditorGroups();
        RegisterGroupTabs(EditorGroup::Left);
        RegisterGroupTabs(EditorGroup::Right);
        RegisterTabTransferHandlers();
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
                for (auto group : {EditorGroup::Left, EditorGroup::Right}) {
                    auto tabs = window->GroupTabs(group);
                    if (auto selected = tabs.ContainerFromItem(tabs.SelectedItem()).try_as<TabViewItem>(); selected && selected.IsLoaded()) {
                        BringIntoViewOptions options;
                        options.AnimationDesired(false);
                        selected.StartBringIntoView(options);
                    }
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
