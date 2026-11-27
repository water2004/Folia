#include "pch.h"
#include "agent/ChatTranscript.h"

namespace winrt::Folia::chat {
    using namespace Microsoft::UI::Xaml;
    using namespace Controls;

    void ChatTranscript::AttachScrolling(Grid const& page) {
        auto weak = weak_from_this();
        wheelFrames_.Attach([weak](float elapsed) {
            auto view = weak.lock();
            if (!view || !view->scroll_.IsLoaded()) return false;
            view->wheel_.Clamp(static_cast<float>(view->scroll_.ScrollableHeight()));
            const auto active = view->wheel_.Advance(std::clamp(elapsed, 0.f, .5f));
            view->scroll_.ChangeView(nullptr, static_cast<double>(view->wheel_.Offset()), nullptr, true);
            return active;
        });
        // Handle wheel input inside the content before ScrollViewer starts its own inertia.
        page.Background(Media::SolidColorBrush(Microsoft::UI::Colors::Transparent()));
        page.PointerWheelChanged([weak](auto const&, Input::PointerRoutedEventArgs const& args) {
            if (auto view = weak.lock()) view->Wheel(args);
        });
        scroll_.AddHandler(UIElement::PointerPressedEvent(), box_value(Input::PointerEventHandler(
            [weak](auto const&, auto const&) { if (auto view = weak.lock()) view->wheelFrames_.Stop(); })), true);
        scroll_.KeyDown([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->wheelFrames_.Stop(); });
        scroll_.DirectManipulationStarted([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->wheelFrames_.Stop(); });
        scroll_.Unloaded([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->wheelFrames_.Stop(); });
        scroll_.ViewChanging([weak](auto const&, ScrollViewerViewChangingEventArgs const& args) {
            if (auto view = weak.lock(); view && args.NextView().VerticalOffset() + 1 < view->scroll_.VerticalOffset())
                view->following_ = false;
        });
        scroll_.ViewChanged([weak](auto const&, auto const&) {
            if (auto view = weak.lock()) {
                auto y = view->scroll_.VerticalOffset();
                if (y + 1 < view->offset_) view->following_ = false;
                const auto bottom = view->scroll_.ScrollableHeight() - 2;
                if (y >= bottom && view->boundaries_.empty() &&
                    (!view->wheelFrames_.Running() || view->wheel_.Target() >= bottom)) view->following_ = true;
                view->offset_ = y;
                view->UpdateNavigation();
            }
        });
        column_.SizeChanged([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->Follow(); });
    }

    void ChatTranscript::Wheel(Input::PointerRoutedEventArgs const& args) {
        auto properties = args.GetCurrentPoint(scroll_).Properties();
        auto delta = -static_cast<float>(properties.MouseWheelDelta());
        auto maximum = static_cast<float>(scroll_.ScrollableHeight());
        if (properties.IsHorizontalMouseWheel() || !delta || maximum <= 0.f) return;
        if (delta < 0.f) following_ = false;
        if (args.Pointer().PointerDeviceType() == Microsoft::UI::Input::PointerDeviceType::Touchpad) {
            wheelFrames_.Stop();
            scroll_.ChangeView(nullptr, std::clamp(scroll_.VerticalOffset() + delta, 0., scroll_.ScrollableHeight()), nullptr, true);
        } else {
            if (!wheelFrames_.Running()) wheel_.Set(static_cast<float>(scroll_.VerticalOffset()), maximum);
            wheel_.Queue(delta, maximum);
            if (wheel_.Animating()) wheelFrames_.Start();
        }
        args.Handled(true);
    }

    void ChatTranscript::Follow() {
        if (posted_ || !following_ || !boundaries_.empty()) return;
        posted_ = true;
        root_.DispatcherQueue().TryEnqueue([weak = weak_from_this()] {
            if (auto view = weak.lock()) {
                view->posted_ = false;
                if (view->following_ && view->boundaries_.empty())
                    view->scroll_.ChangeView(nullptr, view->scroll_.ScrollableHeight(), nullptr, true);
            }
        });
    }

    void ChatTranscript::Latest() {
        wheelFrames_.Stop();
        following_ = true;
        if (!boundaries_.empty()) { boundaries_.clear(); Show(false); }
        Follow();
        UpdateNavigation();
    }
}
