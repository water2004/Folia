#pragma once

#include <winrt/Microsoft.UI.Composition.h>

namespace winrt::Folia::chat {
    struct ChatPalette;
    // A single vector visual is composited before the picker fades or slides.
    class ChatEffortScale {
    public:
        void Attach(Microsoft::UI::Xaml::FrameworkElement const& host);
        void SetPalette(ChatPalette const& palette);
        void Draw(Microsoft::UI::Xaml::FrameworkElement const& host, std::size_t stops, std::optional<std::size_t> selected, bool animate);

    private:
        using Geometry = Microsoft::UI::Composition::CompositionRoundedRectangleGeometry;
        using Shape = Microsoft::UI::Composition::CompositionSpriteShape;
        using Paint = Microsoft::UI::Composition::CompositionColorBrush;
        Microsoft::UI::Composition::ShapeVisual face_{nullptr};
        Microsoft::UI::Composition::CompositionVisualSurface artwork_{nullptr};
        Microsoft::UI::Composition::CompositionPropertySet motion_{nullptr};
        Geometry rail_{nullptr}, fill_{nullptr};
        Shape active_{nullptr}, thumb_{nullptr};
        Microsoft::UI::Composition::CompositionContainerShape ticks_{nullptr};
        Paint accent_{nullptr}, surface_{nullptr}, border_{nullptr}, track_{nullptr}, muted_{nullptr}, onAccent_{nullptr};
    };
}
