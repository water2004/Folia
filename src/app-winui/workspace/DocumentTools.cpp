#include "pch.h"
import std;
#include "workspace/DocumentTools.h"
#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::Folia {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace Microsoft::UI::Xaml::Media::Animation;
    using namespace std::chrono_literals;

    void DocumentTools::Attach(Grid const& card, Grid const& toolbar, Grid const& viewport) {
        card_ = card;
        toolbar_ = toolbar;
        viewport_ = viewport;
        slide_ = Media::TranslateTransform();
        toolbar_.RenderTransform(slide_);
        clip_ = Media::RectangleGeometry();
        clipOffset_ = Media::TranslateTransform();
        clip_.Transform(clipOffset_);
        toolbarResized_ = toolbar_.SizeChanged(auto_revoke, [this](auto const&, auto const&) { UpdateClip(); });
        viewportResized_ = viewport_.SizeChanged(auto_revoke, [this](auto const&, auto const&) { UpdateClip(); });
        hideTimer_ = card_.DispatcherQueue().CreateTimer();
        hideTimer_.IsRepeating(false);
        hideTimer_.Interval(220ms);
        tick_ = hideTimer_.Tick([this](auto const&, auto const&) {
            if (!held_ && !hovered_ && !HasKeyboardFocus()) Reveal(false, true);
        });
        pointerMoved_ = box_value(Input::PointerEventHandler([this](auto const&, auto const& args) {
            OnPointer(args.GetCurrentPoint(card_).Position());
        }));
        card_.AddHandler(UIElement::PointerMovedEvent(), pointerMoved_, true);
        exit_ = card_.PointerExited(auto_revoke, [this](auto const&, auto const&) {
            hovered_ = false;
            ScheduleHide();
        });
        focus_ = toolbar_.GotFocus(auto_revoke, [this](auto const&, auto const&) {
            if (autoHide_ && HasKeyboardFocus()) Reveal(true, true);
        });
        blur_ = toolbar_.LostFocus(auto_revoke, [this](auto const&, auto const&) { ScheduleHide(); });
    }

    void DocumentTools::UpdateClip() {
        clip_.Rect({0, 0, static_cast<float>(viewport_.ActualWidth()), static_cast<float>(viewport_.ActualHeight())});
        if (!motion_ || motion_.GetCurrentState() != ClockState::Active)
            clipOffset_.Y(autoHide_ && revealed_ ? toolbar_.ActualHeight() : 0.);
    }

    bool DocumentTools::HasKeyboardFocus() const {
        auto root = toolbar_.XamlRoot();
        auto element = root ? Input::FocusManager::GetFocusedElement(root).try_as<UIElement>() : nullptr;
        if (!element || element.FocusState() != FocusState::Keyboard) return false;
        for (DependencyObject item = element; item; item = Media::VisualTreeHelper::GetParent(item))
            if (item == toolbar_) return true;
        return false;
    }

    void DocumentTools::Configure(bool autoHide, bool available) {
        if (!toolbar_ || (autoHide == autoHide_ && available == available_)) return;
        autoHide_ = autoHide;
        available_ = available;
        toolbar_.Visibility(available ? Visibility::Visible : Visibility::Collapsed);
        viewport_.Clip(autoHide && available ? clip_ : nullptr);
        UpdateClip();
        hovered_ = false;
        hideTimer_.Stop();
        Reveal(available && (!autoHide || HasKeyboardFocus()), false);
    }

    void DocumentTools::OnPointer(Windows::Foundation::Point point) {
        if (!autoHide_ || !available_) return;
        hovered_ = point.Y >= 0 && point.Y < (revealed_ ? toolbar_.ActualHeight() : 12.)
            && point.X >= 0 && point.X < card_.ActualWidth();
        if (hovered_) {
            hideTimer_.Stop();
            Reveal(true, true);
        } else ScheduleHide();
    }

    void DocumentTools::HoldOpen(bool hold) {
        held_ = hold;
        if (hold && available_) {
            hideTimer_.Stop();
            Reveal(true, true);
        } else ScheduleHide();
    }

    void DocumentTools::ScheduleHide() {
        if (autoHide_ && available_ && revealed_ && !held_ && !hovered_ && !hideTimer_.IsRunning())
            hideTimer_.Start();
    }

    void DocumentTools::Reveal(bool show, bool animate) {
        if (animate && show == revealed_) return;
        revealed_ = show;
        const double opacity = toolbar_.Opacity();
        const double offset = slide_.Y();
        const double inset = clipOffset_.Y();
        if (motion_) motion_.Stop();
        toolbar_.Opacity(show ? 1. : 0.);
        toolbar_.IsHitTestVisible(show);
        slide_.Y(show ? 0. : -6.);
        const double targetInset = autoHide_ && show ? toolbar_.ActualHeight() : 0.;
        clipOffset_.Y(targetInset);
        if (!animate || !Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) return;

        motion_ = Storyboard();
        auto add = [&](DependencyObject const& target, hstring const& property, double from, double to) {
            DoubleAnimation animation;
            animation.From(from);
            animation.To(to);
            animation.EnableDependentAnimation(target == clipOffset_);
            animation.Duration(DurationHelper::FromTimeSpan(show ? 165ms : 130ms));
            CubicEase easing;
            easing.EasingMode(EasingMode::EaseOut);
            animation.EasingFunction(easing);
            Storyboard::SetTarget(animation, target);
            Storyboard::SetTargetProperty(animation, property);
            motion_.Children().Append(animation);
        };
        add(toolbar_, L"Opacity", opacity, show ? 1. : 0.);
        add(slide_, L"Y", offset, show ? 0. : -6.);
        add(clipOffset_, L"Y", inset, targetInset);
        motion_.Begin();
    }

    void DocumentTools::Stop() {
        if (!card_) return;
        hideTimer_.Stop();
        hideTimer_.Tick(tick_);
        if (motion_) motion_.Stop();
        card_.RemoveHandler(UIElement::PointerMovedEvent(), pointerMoved_);
        exit_.revoke();
        focus_.revoke();
        blur_.revoke();
        toolbarResized_.revoke();
        viewportResized_.revoke();
        card_ = nullptr;
    }
}
