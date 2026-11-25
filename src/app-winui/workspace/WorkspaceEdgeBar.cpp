#include "pch.h"
import std;
#include "workspace/WorkspaceEdgeBar.h"
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::Folia {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace Microsoft::UI::Xaml::Media::Animation;
    using namespace std::chrono_literals;

    void WorkspaceEdgeBar::Attach(Grid const& root, Grid const& bar, Grid const& viewport, Edge edge, Microsoft::UI::WindowId window,
                                  std::function<void(bool)> visibilityChanged) {
        bar_ = bar;
        root_ = root;
        viewport_ = viewport;
        edge_ = edge;
        visibilityChanged_ = std::move(visibilityChanged);
        slide_ = Media::TranslateTransform();
        bar_.RenderTransform(slide_);
        clip_ = Media::RectangleGeometry();
        clip_.Transform(slide_);
        viewportResized_ = viewport_.SizeChanged(auto_revoke, [this](auto const&, auto const&) { UpdateViewportClip(); });
        hideTimer_ = bar_.DispatcherQueue().CreateTimer();
        hideTimer_.IsRepeating(false);
        hideTimer_.Interval(260ms);
        hideTick_ = hideTimer_.Tick([this](auto const&, auto const&) {
            if (!held_ && !tabDragActive_ && !pointerOver_ && !HasKeyboardFocus()) Reveal(false, true);
        });
        auto exit = [this](auto const&, auto const&) { pointerOver_ = false; ScheduleHide(); };
        rootExit_ = root_.PointerExited(auto_revoke, exit);
        // Windows also owns the resize border. Observe it without consuming
        // non-client input, so hovering still reveals a bar while resizing works.
        nativePointer_ = Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(window);
        auto nativeHover = [this](auto const&, auto const&) {
            if (!autoHide_ || !available_) return;
            POINT p{};
            if (auto xaml = bar_.XamlRoot(); xaml && ::GetCursorPos(&p)) {
                auto point = xaml.CoordinateConverter().ConvertScreenToLocal(Windows::Graphics::PointInt32{p.x, p.y});
                OnPointer(xaml.Content().TransformToVisual(root_).TransformPoint(point));
            }
        };
        nativeEnter_ = nativePointer_.PointerEntered(nativeHover);
        nativeMove_ = nativePointer_.PointerMoved(nativeHover);
        nativeExit_ = nativePointer_.PointerExited(exit);
        focus_ = bar_.GotFocus(auto_revoke, [this](auto const&, auto const&) {
            if (autoHide_ && available_ && HasKeyboardFocus()) Reveal(true, true);
        });
        blur_ = bar_.LostFocus(auto_revoke, [this](auto const&, auto const&) { ScheduleHide(); });
        resized_ = bar_.SizeChanged(auto_revoke, [this](auto const&, auto const&) {
            UpdateViewportClip();
            if (autoHide_ && !revealed_) Reveal(false, false);
        });
    }

    void WorkspaceEdgeBar::UpdateViewportClip() {
        if (!autoHide_ || !available_) return;
        // Sharing the bar's transform exposes the window backdrop along the moving edge
        // without shifting the document or running another animation.
        const float height = static_cast<float>(bar_.ActualHeight());
        clip_.Rect({0, edge_ == Edge::Top ? height : -height,
            static_cast<float>(viewport_.ActualWidth()), static_cast<float>(viewport_.ActualHeight())});
    }

    void WorkspaceEdgeBar::OnPointer(Windows::Foundation::Point point) {
        if (!autoHide_ || !available_) return;
        const auto distance = edge_ == Edge::Top ? point.Y : root_.ActualHeight() - point.Y;
        const double band = revealed_ ? bar_.ActualHeight() : (edge_ == Edge::Top ? 32. : 10.);
        pointerOver_ = distance >= 0 && distance <= band && point.X >= coveredWidth_ && point.X < root_.ActualWidth();
        if (pointerOver_) {
            hideTimer_.Stop();
            Reveal(true, true);
        } else {
            ScheduleHide();
        }
    }

    bool WorkspaceEdgeBar::HasKeyboardFocus() const {
        auto root = bar_.XamlRoot();
        if (!root) return false;
        auto element = Input::FocusManager::GetFocusedElement(root).try_as<UIElement>();
        if (!element || element.FocusState() != FocusState::Keyboard) return false;
        for (DependencyObject item = element; item; item = Media::VisualTreeHelper::GetParent(item))
            if (item == bar_) return true;
        return false;
    }

    void WorkspaceEdgeBar::Configure(bool autoHide, bool available) {
        if (!bar_) return;
        bool changed = autoHide != autoHide_ || available != available_;
        autoHide_ = autoHide;
        available_ = available;
        if (autoHide_ && available_ && !pointerMoved_) {
            // Observe moves even over controls that handle them. The scrollbar
            // and document keep their full hit targets up to the window edge.
            pointerMoved_ = winrt::box_value(Input::PointerEventHandler([this](auto const&, auto const& args) {
                OnPointer(args.GetCurrentPoint(root_).Position());
            }));
            root_.AddHandler(UIElement::PointerMovedEvent(), pointerMoved_, true);
        } else if ((!autoHide_ || !available_) && pointerMoved_) {
            root_.RemoveHandler(UIElement::PointerMovedEvent(), pointerMoved_);
            pointerMoved_ = nullptr;
        }
        bar_.Visibility(available ? Visibility::Visible : Visibility::Collapsed);
        if (changed) {
            UpdateViewportClip();
            viewport_.Clip(autoHide && available ? clip_ : nullptr);
            hideTimer_.Stop();
            pointerOver_ = false;
            Reveal(available && (!autoHide || held_ || tabDragActive_ || HasKeyboardFocus()), false);
        }
    }

    void WorkspaceEdgeBar::ScheduleHide() {
        if (autoHide_ && available_ && revealed_ && !held_ && !tabDragActive_ && !pointerOver_ && !hideTimer_.IsRunning()) hideTimer_.Start();
    }

    void WorkspaceEdgeBar::HoldOpen(bool hold) {
        held_ = hold;
        if (!autoHide_ || !available_) return;
        if (held_) { hideTimer_.Stop(); Reveal(true, true); }
        else ScheduleHide();
    }

    void WorkspaceEdgeBar::SetTabDragActive(bool active) {
        if (tabDragActive_ == active) return;
        tabDragActive_ = active;
        if (!autoHide_ || !available_) return;
        if (active) {
            hideTimer_.Stop();
            // TabView snapshots screen rectangles at the start of a move.
            // Finish even an in-flight hover animation before that snapshot.
            Reveal(true, false);
            root_.UpdateLayout();
        } else {
            ScheduleHide();
        }
    }

    void WorkspaceEdgeBar::Reveal(bool show, bool animate) {
        if (animate && show == revealed_) return;
        const bool changed = revealed_ != show;
        revealed_ = show;
        const double destination = show ? 0. : (edge_ == Edge::Top ? -1. : 1.) * bar_.ActualHeight();
        const double from = slide_.Y();
        if (animation_) { animation_.Stop(); animation_ = nullptr; }
        slide_.Y(destination);
        if (changed && visibilityChanged_) visibilityChanged_(show);
        if (!animate || from == destination || !Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) return;
        DoubleAnimation motion;
        motion.From(from);
        motion.To(destination);
        motion.Duration(DurationHelper::FromTimeSpan(show ? 170ms : 140ms));
        CubicEase ease;
        ease.EasingMode(EasingMode::EaseOut);
        motion.EasingFunction(ease);
        Storyboard::SetTarget(motion, slide_);
        Storyboard::SetTargetProperty(motion, L"Y");
        animation_ = Storyboard();
        animation_.Children().Append(motion);
        animation_.Begin();
    }

    void WorkspaceEdgeBar::Stop() {
        if (nativePointer_) {
            nativePointer_.PointerEntered(nativeEnter_);
            nativePointer_.PointerMoved(nativeMove_);
            nativePointer_.PointerExited(nativeExit_);
            nativePointer_ = nullptr;
        }
        if (hideTimer_) {
            hideTimer_.Stop();
            hideTimer_.Tick(hideTick_);
        }
        if (animation_) animation_.Stop();
        if (root_ && pointerMoved_) root_.RemoveHandler(UIElement::PointerMovedEvent(), pointerMoved_);
        pointerMoved_ = nullptr;
        rootExit_.revoke();
        viewportResized_.revoke();
        focus_.revoke(); blur_.revoke(); resized_.revoke();
        hideTimer_ = nullptr;
        animation_ = nullptr;
    }
}
