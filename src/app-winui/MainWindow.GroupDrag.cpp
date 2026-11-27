#include "pch.h"
import std;
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <dwmapi.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    namespace {
        Windows::Foundation::Rect ElementBounds(FrameworkElement const& element, UIElement const& root) {
            return element.TransformToVisual(root).TransformBounds({0, 0,
                static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight())});
        }

        bool Contains(Windows::Foundation::Rect bounds, Windows::Foundation::Point point) {
            return point.X >= bounds.X && point.X < bounds.X + bounds.Width
                && point.Y >= bounds.Y && point.Y < bounds.Y + bounds.Height;
        }
    }

    std::optional<MainWindow::GroupDropTarget> MainWindow::GroupDropAt(Windows::Foundation::Point point, std::uint64_t view) {
        auto root = Root();
        auto groupAt = [&](EditorGroup group, bool onStrip) -> std::optional<GroupDropTarget> {
            if (!onStrip && editorGroups.GroupOf(view) == group) return {};
            auto tabs = GroupTabs(group);
            auto strip = group == EditorGroup::Left ? TabViewport() : RightTabViewport();
            auto clip = ElementBounds(strip, root);
            auto views = editorGroups.Views(group);
            float marker = clip.X;
            auto index = views.size();
            for (std::uint32_t i = 0; i < views.size(); ++i) {
                auto item = tabs.ContainerFromIndex(i).try_as<TabViewItem>();
                if (!item || !item.IsLoaded()) continue;
                auto bounds = ElementBounds(item, root);
                if (onStrip && point.X < bounds.X + bounds.Width * .5f) {
                    index = i;
                    marker = bounds.X;
                    break;
                }
                marker = bounds.X + bounds.Width;
            }
            return GroupDropTarget{group, index, std::clamp(marker, clip.X, clip.X + clip.Width), onStrip};
        };

        if (Contains(ElementBounds(TabViewport(), root), point)) return groupAt(EditorGroup::Left, true);
        if (editorGroups.IsSplit() && Contains(ElementBounds(RightTabViewport(), root), point)) return groupAt(EditorGroup::Right, true);
        if (!Contains(ElementBounds(ReadingMotionFrame(), root), point)) return {};
        const bool right = editorGroups.IsSplit() && point.X >= ElementBounds(EditorGroupSplitter(), root).X + EditorGroupSplitter().ActualWidth() * .5;
        return groupAt(right ? EditorGroup::Right : EditorGroup::Left, false);
    }

    void MainWindow::SetTabDropIndicator(std::optional<GroupDropTarget> const& target) {
        auto indicator = GroupTabDropIndicator();
        if (target) {
            auto origin = ElementBounds(TabGroups(), Root());
            indicator.Margin({(std::max)(0.f, target->indicatorX - origin.X - 1.f), 3, 0, 3});
        }
        indicator.Visibility(target ? Visibility::Visible : Visibility::Collapsed);
    }

    void MainWindow::StartNativeTabDrag(Microsoft::UI::Input::EnteringMoveSizeEventArgs const& args) {
        auto screen = nativeTabPress.value_or(args.PointerScreenPoint());
        BeginTabPointerGesture(screen);
        if (!tabGrabOffset || closePending || closingDocument || relocatingFiles) return;
        auto hit = HitTestVisibleTab(screen);
        if (!hit) return;
        ActivateView(hit->id);
        auto tab = tabItems.at(hit->id);
        if (editorGroups.Views(EditorGroup::Left).size() + editorGroups.Views(EditorGroup::Right).size() == 1) {
            draggingTab = tab;
            tab->tearOutWindow = get_weak();
            return;
        }
        auto host = BeginTabTearOut(tab);
        // The native move loop owns one HWND. Redirect it to a prepared host whenever other views stay behind.
        WINDOWPLACEMENT placement{sizeof(placement)};
        winrt::check_bool(::GetWindowPlacement(WindowHandle(), &placement));
        auto normal = placement.rcNormalPosition;
        RECT bounds{};
        winrt::check_bool(::GetWindowRect(WindowHandle(), &bounds));
        winrt::check_bool(::SetWindowPos(host->WindowHandle(), WindowHandle(), bounds.left, bounds.top,
            normal.right - normal.left, normal.bottom - normal.top, SWP_NOACTIVATE | SWP_SHOWWINDOW));
        host->Root().UpdateLayout();
        args.MoveSizeWindowId(host->AppWindow().Id());
    }

    namespace {
        HWND WindowBelow(Windows::Graphics::PointInt32 screen, HWND floating) {
            POINT point{screen.X, screen.Y};
            auto top = ::GetAncestor(::WindowFromPoint(point), GA_ROOT);
            if (top != floating) return top;
            for (auto window = ::GetWindow(floating, GW_HWNDNEXT); window; window = ::GetWindow(window, GW_HWNDNEXT)) {
                RECT rect{};
                if (!::IsWindowVisible(window) || ::IsIconic(window) || !::GetWindowRect(window, &rect) || !::PtInRect(&rect, point)) continue;
                DWORD cloaked{};
                if (SUCCEEDED(::DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) continue;
                if (::GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TRANSPARENT) continue;
                return window;
            }
            return nullptr;
        }
    }

    void MainWindow::TrackNativeTabDrag(Microsoft::UI::Input::WindowRectChangingEventArgs const& args) {
        if (!draggingTab || !tabMoveInProgress) return;
        auto owner = draggingTab->owner.get(), host = draggingTab->tearOutWindow.get();
        if (!owner || !host) return;
        auto screen = args.PointerScreenPoint();
        const auto view = unbox_value<std::uint64_t>(draggingTab->item);
        auto hwnd = WindowBelow(screen, host->WindowHandle());
        auto target = hwnd ? App::Current()->FindMainWindow(Microsoft::UI::GetWindowIdFromWindow(hwnd)) : nullptr;
        std::optional<GroupDropTarget> drop;
        if (target && !target->windowClosed && (target == owner.get() || target->CanDockTab(draggingTab))) {
            auto point = target->Root().XamlRoot().CoordinateConverter().ConvertScreenToLocal(screen);
            drop = target->GroupDropAt(point, view);
            // Dropping back onto the page a tab was just pulled from should leave it in its floating window.
            if (drop && !drop->onStrip && detachedFrom.get().get() == target &&
                (!target->editorGroups.IsSplit() || drop->group == detachedGroup)) drop.reset();
        }
        if (auto old = dropWindow.get(); old && (!drop || old.get() != target)) old->SetTabDropIndicator({});
        dropWindow = drop ? target->get_weak() : weak_ref<MainWindow>{};
        pendingDrop = drop;
        if (drop) target->SetTabDropIndicator(drop);

        if (owner != host && !drop) {
            auto group = *owner->editorGroups.GroupOf(view);
            auto views = owner->editorGroups.Views(group);
            auto index = std::ranges::find(views, view, &WorkspaceEditorView::id) - views.begin();
            if (!CompleteTabTearOut(draggingTab)) return;
            detachedFrom = owner->get_weak();
            detachedGroup = group;
            detachedIndex = index;
            host->AppWindow().Show(false);
            host->Root().UpdateLayout();
            auto tab = host->DocumentTabs().ContainerFromItem(draggingTab->item).try_as<TabViewItem>();
            if (tab) {
                auto rect = ElementBounds(tab, host->Root());
                auto anchor = host->Root().XamlRoot().CoordinateConverter().ConvertLocalToScreen(
                    Windows::Foundation::Point{rect.X + (std::min)(*tabGrabOffset, rect.Width), rect.Y + rect.Height / 2});
                RECT bounds{};
                ::GetWindowRect(host->WindowHandle(), &bounds);
                windowGrab = {anchor.X - bounds.left, anchor.Y - bounds.top};
            }
        }
        if (detachedFrom.get()) {
            auto rect = args.NewWindowRect();
            rect.X = screen.X - windowGrab.X;
            rect.Y = screen.Y - windowGrab.Y;
            args.NewWindowRect(rect);
        }
    }

    void MainWindow::FinishNativeTabDrag() {
        auto target = dropWindow.get();
        auto drop = std::exchange(pendingDrop, {});
        dropWindow = {};
        if (target && !target->windowClosed) target->SetTabDropIndicator({});
        if (!draggingTab) return;
        auto owner = draggingTab->owner.get(), host = draggingTab->tearOutWindow.get();
        if (!owner || !host) return;
        auto view = unbox_value<std::uint64_t>(draggingTab->item);
        bool cancelled = (::GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (cancelled) {
            target = detachedFrom.get();
            drop = GroupDropTarget{detachedGroup, detachedIndex, 0, true};
        }
        detachedFrom = {};
        windowGrab = {};
        if (target && !target->windowClosed && drop) {
            if (owner == target) target->MoveEditorView(view, drop->group, drop->index);
            else if (target->TransferTab(draggingTab, drop->index, drop->group)) target->Activate();
        }
        if (draggingTab->owner.get() != host) {
            // Closing from inside the native move callback can reenter input teardown.
            host->AppWindow().Hide();
            host->DispatcherQueue().TryEnqueue([weak = host->get_weak()] {
                if (auto window = weak.get(); window && !window->windowClosed && (window->preparedTabWindow || window->transferPlaceholder)) {
                    window->closeApproved = true;
                    window->Close();
                }
            });
        } else host->Activate();
    }
}
