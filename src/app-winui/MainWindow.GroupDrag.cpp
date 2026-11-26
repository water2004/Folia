#include "pch.h"
import std;
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Interop.h>

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

    void MainWindow::RegisterGroupDragHandlers(EditorGroup group) {
        auto tabs = GroupTabs(group);
        tabs.AddHandler(UIElement::PointerPressedEvent(), box_value(Input::PointerEventHandler(
            WindowEvent([this, group](auto const&, Input::PointerRoutedEventArgs const& args) {
                if (!editorGroups.IsSplit() || groupTabDrag || closePending || closingDocument || relocatingFiles) return;
                auto root = Root();
                auto point = args.GetCurrentPoint(root);
                if (!point.Properties().IsLeftButtonPressed()) return;
                for (auto node = args.OriginalSource().try_as<DependencyObject>(); node; node = Media::VisualTreeHelper::GetParent(node)) {
                    if (node.try_as<Primitives::ButtonBase>()) return;
                    if (auto tab = node.try_as<TabViewItem>()) {
                        if (!tab.Tag()) return;
                        auto view = unbox_value<std::uint64_t>(tab.Tag());
                        if (editorGroups.GroupOf(view) != group) return;
                        ActivateView(view);
                        if (root.CapturePointer(args.Pointer())) {
                            groupTabDrag.emplace(GroupTabDrag{view, point.Position(), args.Pointer()});
                            args.Handled(true);
                        }
                        return;
                    }
                }
            }))), true);
    }

    void MainWindow::RegisterGroupDragSurface() {
        auto root = Root();
        auto route = [&](auto event, bool releasing) {
            root.AddHandler(event, box_value(Input::PointerEventHandler(
                WindowEvent([this, releasing](auto const&, Input::PointerRoutedEventArgs const& args) {
                    UpdateGroupDrag(args, releasing);
                }))), true);
        };
        route(UIElement::PointerMovedEvent(), false);
        route(UIElement::PointerReleasedEvent(), true);
        auto cancel = WindowEvent([this](auto const&, Input::PointerRoutedEventArgs const& args) {
            if (groupTabDrag && groupTabDrag->pointer.PointerId() == args.Pointer().PointerId()) CancelGroupDrag();
        });
        root.PointerCaptureLost(cancel);
        root.PointerCanceled(cancel);
        root.AddHandler(UIElement::KeyDownEvent(), box_value(Input::KeyEventHandler(
            WindowEvent([this](auto const&, Input::KeyRoutedEventArgs const& args) {
                if (!groupTabDrag || args.Key() != Windows::System::VirtualKey::Escape) return;
                CancelGroupDrag();
                args.Handled(true);
            }))), true);
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
            return GroupDropTarget{group, index, std::clamp(marker, clip.X, clip.X + clip.Width)};
        };

        if (Contains(ElementBounds(TabViewport(), root), point)) return groupAt(EditorGroup::Left, true);
        if (editorGroups.IsSplit() && Contains(ElementBounds(RightTabViewport(), root), point)) return groupAt(EditorGroup::Right, true);
        if (!Contains(ElementBounds(ReadingMotionFrame(), root), point)) return {};
        const bool right = editorGroups.IsSplit() && point.X >= ElementBounds(EditorGroupSplitter(), root).X + EditorGroupSplitter().ActualWidth() * .5;
        return groupAt(right ? EditorGroup::Right : EditorGroup::Left, false);
    }

    void MainWindow::UpdateGroupDrag(Input::PointerRoutedEventArgs const& args, bool releasing) {
        if (!groupTabDrag || groupTabDrag->pointer.PointerId() != args.Pointer().PointerId()) return;
        auto pointer = args.GetCurrentPoint(Root());
        // Some pointer devices also report a release while another button is held.
        if (releasing && pointer.Properties().IsLeftButtonPressed()) return;
        auto point = pointer.Position();
        auto& drag = *groupTabDrag;
        if (!drag.started) {
            const auto scale = Root().XamlRoot().RasterizationScale();
            drag.started = std::abs(point.X - drag.origin.X) * scale > ::GetSystemMetrics(SM_CXDRAG)
                || std::abs(point.Y - drag.origin.Y) * scale > ::GetSystemMetrics(SM_CYDRAG);
        }
        args.Handled(true);
        auto target = drag.started ? GroupDropAt(point, drag.view) : std::nullopt;
        if (!releasing) {
            auto indicator = GroupTabDropIndicator();
            if (target) {
                auto origin = ElementBounds(TabGroups(), Root());
                indicator.Margin({(std::max)(0.f, target->indicatorX - origin.X - 1.f), 3, 0, 3});
            }
            indicator.Visibility(target ? Visibility::Visible : Visibility::Collapsed);
            return;
        }

        auto finished = drag;
        CancelGroupDrag();
        if (!finished.started) return;
        if (!target) {
            DropGroupTabOutside(finished.view);
            return;
        }
        DispatcherQueue().TryEnqueue([weak = get_weak(), view = finished.view, drop = *target] {
            if (auto window = weak.get(); window && !window->windowClosed) window->MoveEditorView(view, drop.group, drop.index);
        });
    }

    void MainWindow::CancelGroupDrag() {
        if (auto drag = std::exchange(groupTabDrag, std::nullopt)) {
            if (!windowClosed) {
                GroupTabDropIndicator().Visibility(Visibility::Collapsed);
                Root().ReleasePointerCapture(drag->pointer);
            }
        }
    }

    void MainWindow::DropGroupTabOutside(std::uint64_t view) {
        auto tab = tabItems.find(view);
        if (tab == tabItems.end() || editorGroups.ViewCount(tab->second->document) != 1) return;
        POINT point{};
        if (!::GetCursorPos(&point)) return;
        auto root = ::GetAncestor(::WindowFromPoint(point), GA_ROOT);
        if (root == WindowHandle()) return;
        auto target = root ? App::Current()->FindMainWindow(Microsoft::UI::GetWindowIdFromWindow(root)) : nullptr;
        if (target && !target->CanAcceptTab(tab->second)) return;
        std::optional<GroupDropTarget> drop;
        if (target && target->Root().XamlRoot()) {
            auto local = target->Root().XamlRoot().CoordinateConverter().ConvertScreenToLocal(Windows::Graphics::PointInt32{point.x, point.y});
            drop = target->GroupDropAt(local, view);
        }
        DispatcherQueue().TryEnqueue([weak = get_weak(), target = target ? target->get_weak() : weak_ref<MainWindow>{},
                                        item = tab->second, point, drop, create = target == nullptr] {
            auto source = weak.get();
            if (!source || source->windowClosed || source->editorGroups.ViewCount(item->document) != 1) return;
            auto receiving = target.get();
            if (create) {
                auto window = source->CreateTabWindow();
                winrt::check_bool(::SetWindowPos(window->WindowHandle(), nullptr, point.x - 40, point.y - 18,
                    0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
                receiving = window->get_strong();
            }
            if (receiving) {
                auto group = drop ? drop->group : receiving->editorGroups.ActiveGroup();
                auto index = drop ? drop->index : receiving->editorGroups.Views(group).size();
                receiving->TransferTab(item, index, group);
            }
        });
    }
}
