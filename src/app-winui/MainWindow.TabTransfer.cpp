#include "pch.h"
import std;
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <winrt/Microsoft.UI.Content.h>

#pragma comment(lib, "comctl32.lib")

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    void MainWindow::SynchronizeTabOrder() {
        if (updatingTabs) return;
        std::vector<std::uint64_t> order;
        for (auto group : {EditorGroup::Left, EditorGroup::Right}) {
            std::vector<std::uint64_t> views;
            for (auto item : group == EditorGroup::Left ? tabSource : rightTabSource)
                views.push_back(winrt::unbox_value<std::uint64_t>(item));
            if (!editorGroups.Reorder(group, views)) return;
            for (auto const& view : editorGroups.Views(group))
                if (std::ranges::find(order, view.document) == order.end()) order.push_back(view.document);
        }
        if (order.size() == workspaceDocuments.Items().size()) workspaceDocuments.Reorder(order);
    }
    void MainWindow::QueueTabOrderSync() {
        if (updatingTabs || tabOrderSyncPending || windowClosed) return;
        tabOrderSyncPending = true;
        DispatcherQueue().TryEnqueue([weak = get_weak()] {
            if (auto window = weak.get()) {
                window->tabOrderSyncPending = false;
                if (!window->windowClosed) window->SynchronizeTabOrder();
            }
        });
    }
    bool MainWindow::CanAcceptTab(std::shared_ptr<DocumentTab> const& tab) const {
        if (!tab || closePending || closingDocument || relocatingFiles || windowClosed) return false;
        auto source = tab->owner.get();
        if (!source || source.get() == this || source->windowClosed || source->closePending || source->closingDocument || source->relocatingFiles) return false;
        auto id = tab->document;
        if (source->editorGroups.ViewCount(id) != 1) return false;
        auto document = source->workspaceDocuments.Find(id);
        if (document && document->kind == WorkspaceTabKind::Settings && workspaceDocuments.FindSettings()) return false;
        return document && !workspaceDocuments.Find(id)
            && (document->path.empty() || !workspaceDocuments.Find(document->path));
    }
    bool MainWindow::TransferTab(std::shared_ptr<DocumentTab> tab, std::size_t index, EditorGroup group) {
        if (!CanAcceptTab(tab)) return false;
        auto source = tab->owner.get();
        auto view = winrt::unbox_value<std::uint64_t>(tab->item);
        auto id = tab->document;
        auto document = source->workspaceDocuments.Find(id);
        source->SynchronizeTabOrder(); SynchronizeTabOrder();
        tabItems.reserve(tabItems.size() + 1);
        struct Mutating {
            MainWindow& from; MainWindow& to;
            Mutating(MainWindow& a, MainWindow& b) : from(a), to(b) { from.updatingTabs = to.updatingTabs = true; }
            ~Mutating() { from.updatingTabs = to.updatingTabs = false; }
        };
        {
            Mutating mutation(*source, *this);
            if (source->IsActiveDocument(id)) {
                if (auto next = source->editorGroups.Replacement(view)) source->ActivateView(next);
                else {
                    auto placeholder = source->workspaceDocuments.New(WorkspaceOpenMode::Preview);
                    source->transferPlaceholder = placeholder->id;
                    source->ActivateDocument(placeholder->id);
                }
            }
            auto entry = *source->editorGroups.Find(view);
            source->workspaceDocuments.TransferTo(id, workspaceDocuments, index);
            source->editorGroups.Remove(view);
            for (auto items : {source->tabSource, source->rightTabSource}) {
                std::uint32_t at = 0;
                if (items.IndexOf(tab->item, at)) items.RemoveAt(at);
            }
            source->tabItems.erase(view);
            tab->owner = get_weak();
            // XAML containers retain their TabView parent and XamlRoot. Only
            // move the stable data item; the target template owns a fresh view.
            editorGroups.Insert(group, entry, index);
            tabItems.emplace(view, tab);
            if (document->editor) document->editor->themeRevision = 0;
            ActivateView(view);
            if (transferPlaceholder && *transferPlaceholder != id) {
                auto placeholder = workspaceDocuments.Find(*transferPlaceholder);
                if (placeholder && placeholder->path.empty() && !placeholder->IsModified()
                    && !placeholder->kept) {
                    editorGroups.ForgetDocument(placeholder->id);
                    workspaceDocuments.Remove(placeholder->id);
                }
                transferPlaceholder.reset();
            }
        }
        source->RefreshDocumentTabs(); RefreshDocumentTabs();
        // The native move loop resumes immediately after this callback. Its
        // next bounds check must use the receiving strip's completed layout.
        GroupTabs(group).UpdateLayout();
        RevealTabWindow();
        return true;
    }
    MainWindow* MainWindow::CreateTabWindow() {
        auto app = App::Current();
        auto window = app->CreateMainWindow();
        auto target = winrt::get_self<MainWindow>(window.as<winrt::Folia::MainWindow>());
        target->transferPlaceholder = target->workspaceDocuments.Active()->id;
        target->preparedTabWindow = true;
        target->preparedTabOwner = get_weak();
        target->preparedTabActivation.Attach(target->WindowHandle());
        target->tabWindowStyle = ::GetWindowLongPtrW(target->WindowHandle(), GWL_EXSTYLE);
        ::SetWindowLongPtrW(target->WindowHandle(), GWL_EXSTYLE,
            target->tabWindowStyle | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
        target->AppWindow().IsShownInSwitchers(false);
        BOOL hidden = TRUE;
        winrt::check_hresult(::DwmSetWindowAttribute(target->WindowHandle(), DWMWA_CLOAK, &hidden, sizeof(hidden)));
        return target;
    }
    void MainWindow::RevealTabWindow() {
        if (!preparedTabWindow) return;
        preparedTabActivation.Reset();
        ::SetWindowLongPtrW(WindowHandle(), GWL_EXSTYLE, tabWindowStyle);
        AppWindow().IsShownInSwitchers(true);
        BOOL hidden = FALSE;
        winrt::check_hresult(::DwmSetWindowAttribute(WindowHandle(), DWMWA_CLOAK, &hidden, sizeof(hidden)));
        preparedTabWindow = false;
        preparedTabOwner = {};
    }
    std::shared_ptr<MainWindow::DocumentTab> MainWindow::FindTransferTab(Windows::Foundation::IInspectable const& item) {
        auto id = winrt::unbox_value<std::uint64_t>(item);
        if (auto owner = App::Current()->FindTabWindow(id))
            if (auto found = owner->tabItems.find(id); found != owner->tabItems.end() && found->second->item == item) return found->second;
        return {};
    }
    MainWindow* MainWindow::BeginTabTearOut(std::shared_ptr<DocumentTab> const& tab) {
        EndTabDrag();
        auto target = CreateTabWindow();
        draggingTab = tab;
        tab->tearOutWindow = target->get_weak();
        return target;
    }
    bool MainWindow::CompleteTabTearOut(std::shared_ptr<DocumentTab> const& tab) {
        if (!tab) return false;
        dragPresentation.Stop();
        if (auto target = tab->tearOutWindow.get()) return target->TransferTab(tab, 0);
        return false;
    }
    void MainWindow::EndTabDrag() {
        dragPresentation.Stop(!windowClosed);
        if (draggingTab) {
            draggingTab->tearOutWindow = {};
        }
        draggingTab.reset();
    }
    bool MainWindow::PrepareExternalTab(Windows::Foundation::IInspectable const& item) {
        auto tab = FindTransferTab(item);
        // WinUI enumerates loaded TabViews, including hidden/preparation hosts.
        // Such windows are not user-visible drop destinations.
        if (preparedTabWindow || !AppWindow().IsVisible() || !CanAcceptTab(tab)) return false;
        if (!tab->tearOutWindow.get()) {
            // A one-tab drag moves its existing window and raises no
            // TabTearOutWindowRequested event.
            if (auto source = tab->owner.get()) {
                source->draggingTab = tab;
                tab->tearOutWindow = source;
            }
        }
        return true;
    }
    bool MainWindow::ContainsTabDragPoint(Windows::Graphics::PointInt32 point) {
        auto tabs = DocumentTabs();
        auto list = tabs.as<IControlProtected>().GetTemplateChild(L"TabListView").try_as<FrameworkElement>();
        if (!list || !list.IsLoaded() || list.ActualWidth() <= 0 || list.ActualHeight() <= 0) return false;
        // Use precisely the element and coordinate system used by WinUI's
        // DragTabWithinTabView. PopulateTabViewList also includes AddButton;
        // accepting that extra region can cause an immediate tear-out again.
        auto bounds = list.TransformToVisual(nullptr).TransformBounds({0, 0,
            static_cast<float>(list.ActualWidth()), static_cast<float>(list.ActualHeight())});
        auto local = list.XamlRoot().CoordinateConverter().ConvertScreenToLocal(point);
        return local.X >= bounds.X && local.X <= bounds.X + bounds.Width
            && local.Y >= bounds.Y && local.Y <= bounds.Y + bounds.Height;
    }
    std::optional<MainWindow::TabHit> MainWindow::HitTestVisibleTab(Windows::Graphics::PointInt32 point) {
        auto tabs = DocumentTabs();
        if (ContainsTabDragPoint(point)) {
            const auto local = tabs.XamlRoot().CoordinateConverter().ConvertScreenToLocal(point);
            auto viewer = TabStripScroller();
            auto viewport = viewer ? viewer.as<IControlProtected>().GetTemplateChild(L"ScrollContentPresenter").try_as<FrameworkElement>() : nullptr;
            auto bounds = [](FrameworkElement const& element) {
                return element.TransformToVisual(nullptr).TransformBounds({0, 0,
                    static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight())});
            };
            auto contains = [&](Windows::Foundation::Rect r) {
                return local.X >= r.X && local.X < r.X + r.Width && local.Y >= r.Y && local.Y < r.Y + r.Height;
            };
            // Use the same container geometry as TabView::GetTabAtPoint, clipped
            // to the viewport so offscreen tabs cannot claim either scroll arrow.
            if (viewport && contains(bounds(viewport))) {
                for (std::uint32_t i = 0; i < tabSource.Size(); ++i) {
                    if (auto tab = tabs.ContainerFromIndex(i).try_as<TabViewItem>()) {
                        auto rect = bounds(tab);
                        if (contains(rect)) return TabHit{
                            winrt::unbox_value<std::uint64_t>(tabSource.GetAt(i)), local.X - rect.X, tab.IsClosable()};
                    }
                }
            }
        }
        return std::nullopt;
    }
    void MainWindow::BeginTabPointerGesture(Windows::Graphics::PointInt32 point) {
        if (tabMoveInProgress) return;
        tabPointerDown = true;
        EndTabDrag();
        auto hit = HitTestVisibleTab(point);
        tabGrabOffset = hit ? std::optional{hit->grabOffset} : std::nullopt;
        // The native TabView move loop must only participate in a gesture
        // that actually started on a visible tab. Caption movement remains
        // ordinary window movement, including after a previous tab click.
        UpdateTabTransferAvailability();
    }
    void MainWindow::UpdateTabTransferAvailability() {
        if (tabMoveInProgress || windowClosed) return;
        auto tabs = DocumentTabs();
        const bool nativeDrag = !editorGroups.IsSplit() && !closePending && !closingDocument && !relocatingFiles;
        // Split groups use the workspace's pointer capture and drop indicator.
        // Leaving WinUI drop enabled also activates its tab drag presentation.
        if (tabs.AllowDrop() != nativeDrag) tabs.AllowDrop(nativeDrag);
        if (tabs.CanDragTabs() != nativeDrag) tabs.CanDragTabs(nativeDrag);
        if (tabs.CanReorderTabs() != nativeDrag) tabs.CanReorderTabs(nativeDrag);
        const bool tearOut = nativeDrag && (!tabPointerDown || tabGrabOffset.has_value());
        if (tabs.CanTearOutTabs() != tearOut) tabs.CanTearOutTabs(tearOut);
    }

    void MainWindow::RefreshTabCaptionRegion() {
        if (tabRegionsPending) return;
        tabRegionsPending = true;
        DispatcherQueue().TryEnqueue([weak = get_weak()] {
            auto window = weak.get();
            if (!window) return;
            window->tabRegionsPending = false;
            if (window->windowClosed || window->tabMoveInProgress) return;
            auto tabs = window->DocumentTabs();
            auto list = tabs.as<IControlProtected>().GetTemplateChild(L"TabListView").try_as<FrameworkElement>();
            if (!list || !list.IsLoaded()) return;
            auto button = window->NewTabButton();
            if (!button) return;
            auto id = window->AppWindow().Id();
            auto island = tabs.XamlRoot().CoordinateConverter();
            auto native = Microsoft::UI::Content::ContentCoordinateConverter::CreateForWindowId(id);
            auto bounds = [](FrameworkElement const& element) { return element.TransformToVisual(nullptr).TransformBounds(
                {0, 0, static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight())}); };
            std::vector<Windows::Graphics::RectInt32> caption, passthrough;
            auto add = [&](auto& region, Windows::Foundation::Rect rect) {
                if (rect.Width <= 0 || rect.Height <= 0) return;
                auto r = native.ConvertScreenToLocal(island.ConvertLocalToScreen(rect));
                region.push_back({static_cast<int>(r.X), static_cast<int>(r.Y), static_cast<int>(r.Width), static_cast<int>(r.Height)});
            };
            if (window->editorGroups.IsSplit()) {
                auto left = bounds(window->TabViewport());
                auto right = bounds(window->RightTabViewport());
                auto addGroup = [&](Windows::Foundation::Rect strip, Button const& button) {
                    const auto end = strip.X + strip.Width;
                    auto blank = end;
                    if (button && button.IsLoaded()) {
                        const auto action = bounds(button);
                        blank = std::clamp(action.X + action.Width, strip.X, end);
                    }
                    add(caption, {blank, strip.Y, end - blank, strip.Height});
                };
                addGroup(left, button);
                addGroup(right, window->NewTabButton(EditorGroup::Right));
                const auto leftEnd = left.X + left.Width;
                add(caption, {leftEnd, left.Y, right.X - leftEnd, left.Height});
            } else {
                auto strip = bounds(window->TopChrome());
                auto tabList = bounds(list);
                auto actions = bounds(button);
                auto captionStart = bounds(window->CaptionInset()).X;
                // The shelf above the tabs and the space after the add button
                // move the window. WinUI's tab list owns the tear-out gesture.
                add(caption, {tabList.X, strip.Y, captionStart - tabList.X, tabList.Y - strip.Y});
                add(caption, {actions.X + actions.Width, tabList.Y, captionStart - actions.X - actions.Width, tabList.Height});
                if (tabs.CanTearOutTabs()) add(caption, tabList);
                // WinUI claims the full tab list as caption for tear-out. Keep
                // its buttons in client input even when those rectangles overlap.
                if (auto viewer = window->TabStripScroller()) {
                    auto parts = viewer.as<IControlProtected>();
                    for (auto const& reference : window->tabStripParts[static_cast<std::size_t>(EditorGroup::Left)].arrows)
                        if (auto arrow = reference.get(); arrow && viewer.ComputedHorizontalScrollBarVisibility() == Visibility::Visible)
                            add(passthrough, bounds(arrow));
                    if (auto viewport = parts.GetTemplateChild(L"ScrollContentPresenter").try_as<FrameworkElement>()) {
                        const auto clip = bounds(viewport);
                        for (std::uint32_t index = 0; index < window->tabSource.Size(); ++index) {
                            auto tab = tabs.ContainerFromIndex(index).try_as<Control>();
                            if (!tab) continue;
                            auto close = tab.as<IControlProtected>().GetTemplateChild(L"CloseButton").try_as<Button>();
                            if (!close || close.Visibility() != Visibility::Visible) continue;
                            auto rect = bounds(close);
                            const auto left = (std::max)(rect.X, clip.X);
                            const auto right = (std::min)(rect.X + rect.Width, clip.X + clip.Width);
                            add(passthrough, {left, rect.Y, right - left, rect.Height});
                        }
                    }
                }
            }
            auto pointer = Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(id);
            using Microsoft::UI::Input::NonClientRegionKind;
            if (!std::ranges::equal(pointer.GetRegionRects(NonClientRegionKind::Caption), caption))
                pointer.SetRegionRects(NonClientRegionKind::Caption, caption);
            if (!std::ranges::equal(pointer.GetRegionRects(NonClientRegionKind::Passthrough), passthrough))
                pointer.SetRegionRects(NonClientRegionKind::Passthrough, passthrough);
        });
    }
    void MainWindow::RestoreIdleTabTransfer() {
        DispatcherQueue().TryEnqueue([weak = get_weak()] {
            if (auto window = weak.get(); window && !window->windowClosed
                && !window->tabPointerDown && !window->tabMoveInProgress)
                window->UpdateTabTransferAvailability();
        });
    }
    void MainWindow::EndTabPointerGesture() {
        tabPointerDown = false;
        if (!tabMoveInProgress) RestoreIdleTabTransfer();
    }
    void MainWindow::EnterTabMove(Microsoft::UI::Input::InputNonClientPointerSource const& pointer) {
        tabMoveInProgress = true;
        if (!tabPointerDown || !tabGrabOffset) return;
        if (draggingTab && IsActiveDocument(draggingTab->document))
            dragPresentation.Start(DocumentTabs(), draggingTab->item, *tabGrabOffset, pointer);
    }
    void MainWindow::ExitTabMove() {
        tabMoveInProgress = false;
        animationController.SetPaused(false);
        EndTabDrag();
        EndTabPointerGesture();
    }
    LRESULT CALLBACK MainWindow::TabPressSubclass(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
        UINT_PTR id, DWORD_PTR context) {
        if (message == WM_NCDESTROY) ::RemoveWindowSubclass(window, TabPressSubclass, id);
        auto owner = reinterpret_cast<MainWindow*>(context);
        if (message == WM_CAPTURECHANGED || message == WM_CANCELMODE) owner->CancelTabMiddlePress();
        if (message == WM_NCLBUTTONDOWN && wparam == HTCAPTION) {
            auto self = owner->get_strong();
            const Windows::Graphics::PointInt32 point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (!self->tabMoveInProgress && self->DocumentTabs().CanTearOutTabs()) {
                if (auto tab = self->HitTestVisibleTab(point)) {
                    // The SDK starts its tear-out loop on mouse-down. Wait for
                    // the system drag threshold before forwarding a tab press.
                    if (!(::GetAsyncKeyState(VK_LBUTTON) & 0x8000) || !::DragDetect(window, {point.X, point.Y})) {
                        if (!self->windowClosed) {
                            self->ActivateView(tab->id);
                            self->EndTabPointerGesture();
                        }
                        return 0;
                    }
                }
            }
        }
        return ::DefSubclassProc(window, message, wparam, lparam);
    }
    void MainWindow::RegisterTabTransferHandlers() {
        tabDragEnded.Attach(Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(AppWindow().Id()),
            WindowEvent([this](auto const&, auto const& args) {
                // A tab click can finish as client input without a non-client
                // release. Resolve every move from its own native start point.
                if (args.MoveSizeOperation() == Microsoft::UI::Input::MoveSizeOperation::Move) {
                    BeginTabPointerGesture(args.PointerScreenPoint());
                } else tabGrabOffset.reset();
            }),
            WindowEvent([this](auto const&, auto const&) { ExitTabMove(); }),
            WindowEvent([this](auto const& pointer, auto const& args) {
                EnterTabMove(pointer);
                // DWM already moves the retained surface with the window.
                // Idle caret and selection animations need no editor redraw here.
                if (args.MoveSizeOperation() == Microsoft::UI::Input::MoveSizeOperation::Move && !tabGrabOffset)
                    animationController.SetPaused(true);
            }),
            WindowEvent([this](auto const&, Microsoft::UI::Input::NonClientPointerEventArgs const& args) {
                if (args.PointerDeviceType() == Microsoft::UI::Input::PointerDeviceType::Mouse
                    && !(::GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;
                POINT point{};
                if (::GetCursorPos(&point)) BeginTabPointerGesture({point.x, point.y});
            }),
            WindowEvent([this](auto const&, auto const&) {
                if (!(::GetAsyncKeyState(VK_LBUTTON) & 0x8000)) EndTabPointerGesture();
            }));
        winrt::check_bool(::SetWindowSubclass(WindowHandle(), TabPressSubclass, 0, reinterpret_cast<DWORD_PTR>(this)));
        // Register the origin check before TabView attaches its move callbacks.
        UpdateTabTransferAvailability();
        DocumentColumn().SizeChanged(WindowEvent([this](auto const&, auto const&) { RefreshTabCaptionRegion(); }));
        DocumentTabs().TabItemsChanged(WindowEvent([this](auto const&, auto const&) {
            QueueTabOrderSync();
            RefreshTabCaptionRegion();
        }));
        RightDocumentTabs().TabItemsChanged(WindowEvent([this](auto const&, auto const&) {
            QueueTabOrderSync();
            RefreshTabCaptionRegion();
        }));
        DocumentTabs().TabTearOutWindowRequested(WindowEvent([this](auto const&, auto const& args) {
            auto items = args.Items();
            if (items.size() == 1)
                if (auto tab = FindTransferTab(items[0])) args.NewWindowId(BeginTabTearOut(tab)->AppWindow().Id());
        }));
        DocumentTabs().TabTearOutRequested(WindowEvent([this](auto const&, auto const& args) {
            // The shipped SDK never fills args.NewWindowId(). The gesture's
            // host follows the data item even after it merges into another view.
            auto items = args.Items();
            if (items.size() == 1) CompleteTabTearOut(FindTransferTab(items[0]));
        }));
        DocumentTabs().ExternalTornOutTabsDropping(WindowEvent([this](auto const&, auto const& args) {
            auto items = args.Items();
            POINT point{};
            args.AllowDrop(items.size() == 1 && ::GetCursorPos(&point)
                && ContainsTabDragPoint({point.x, point.y}) && PrepareExternalTab(items[0]));
        }));
        DocumentTabs().ExternalTornOutTabsDropped(WindowEvent([this](auto const&, auto const& args) {
            auto items = args.Items();
            if (items.size() == 1) TransferTab(FindTransferTab(items[0]), static_cast<std::size_t>((std::max)(0, args.DropIndex())));
        }));
    }
}
