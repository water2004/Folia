#include "pch.h"
import std;
#include "workspace/TabDragPresentation.h"
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::Folia {
    using namespace Microsoft::UI::Xaml;
    using namespace Controls;

    void TabDragPresentation::Start(TabView const& tabs, Windows::Foundation::IInspectable const& item, float grabOffset,
                                    Microsoft::UI::Input::InputNonClientPointerSource const& pointer) {
        Stop();
        tabs_ = tabs;
        item_ = item;
        grabOffset_ = grabOffset;
        pointer_ = pointer;
        // Subscribe for the duration of this native move, after TabView's own
        // handler. A reorder can replace the container for the same data item.
        move_ = pointer_.WindowRectChanging([this](auto const&, auto const& args) { MoveTo(args.PointerScreenPoint()); });
    }

    void TabDragPresentation::MoveTo(Windows::Graphics::PointInt32 screen) {
        if (!tabs_ || !tabs_.XamlRoot()) return;
        auto tab = tabs_.ContainerFromItem(item_).try_as<TabViewItem>();
        if (tab != container_) {
            ReleaseContainer(false);
            if (!tab) return;
            auto artwork = tab.as<IControlProtected>().GetTemplateChild(L"LayoutRoot").try_as<FrameworkElement>();
            if (!artwork) return;
            container_ = tab;
            slide_ = Media::TranslateTransform();
            artwork.RenderTransform(slide_);
            Canvas::SetZIndex(tab, 1);
        }
        if (!slide_) return;
        auto point = tabs_.XamlRoot().CoordinateConverter().ConvertScreenToLocal(screen);
        auto origin = tab.TransformToVisual(nullptr).TransformPoint({0, 0});
        slide_.X(std::clamp<double>(point.X - origin.X - grabOffset_, -tab.ActualWidth(), tab.ActualWidth()));
    }

    void TabDragPresentation::ReleaseContainer(bool settle) {
        if (!container_) return;
        Canvas::SetZIndex(container_, 0);
        double previous = slide_.X();
        slide_.X(0);
        if (settle && std::abs(previous) > .5 && Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
            using namespace Media::Animation;
            DoubleAnimation motion;
            motion.From(previous);
            motion.To(0.0);
            motion.Duration(DurationHelper::FromTimeSpan(std::chrono::milliseconds(125)));
            CubicEase ease;
            ease.EasingMode(EasingMode::EaseOut);
            motion.EasingFunction(ease);
            motion.FillBehavior(FillBehavior::Stop);
            Storyboard::SetTarget(motion, slide_);
            Storyboard::SetTargetProperty(motion, L"X");
            Storyboard finish;
            finish.Children().Append(motion);
            finish.Begin();
        }
        slide_ = nullptr;
        container_ = nullptr;
    }

    void TabDragPresentation::Stop(bool settle) {
        if (pointer_) {
            pointer_.WindowRectChanging(move_);
            pointer_ = nullptr;
        }
        ReleaseContainer(settle);
        tabs_ = nullptr;
        item_ = nullptr;
    }
}
