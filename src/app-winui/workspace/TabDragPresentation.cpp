#include "pch.h"
import std;
#include "workspace/TabDragPresentation.h"
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
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
            artwork_ = artwork;
            Canvas::SetZIndex(tab, 1);
        }
        if (!artwork_) return;
        auto point = tabs_.XamlRoot().CoordinateConverter().ConvertScreenToLocal(screen);
        auto origin = tab.TransformToVisual(nullptr).TransformPoint({0, 0});
        auto offset = std::clamp<double>(point.X - origin.X - grabOffset_, -tab.ActualWidth(), tab.ActualWidth());
        artwork_.Translation({static_cast<float>(offset), 0, 0});
    }

    void TabDragPresentation::ReleaseContainer(bool settle) {
        if (!container_) return;
        Canvas::SetZIndex(container_, 0);
        auto previous = artwork_.Translation();
        artwork_.Translation({});
        if (settle && std::abs(previous.x) > .5 && Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
            auto compositor = Hosting::ElementCompositionPreview::GetElementVisual(artwork_).Compositor();
            auto motion = compositor.CreateVector3KeyFrameAnimation();
            motion.Target(L"Translation");
            motion.InsertKeyFrame(0, previous);
            motion.InsertKeyFrame(1, {}, compositor.CreateCubicBezierEasingFunction({.22f, 1}, {.36f, 1}));
            motion.Duration(std::chrono::milliseconds(125));
            artwork_.StartAnimation(motion);
        }
        artwork_ = nullptr;
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
