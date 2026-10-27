#pragma once

import folia.markdown.text_edit;
import folia.core.theme;

namespace winrt::Folia {
    struct EditorCaretFrame {
        D2D1_RECT_F bounds{};
        float opacity = 0;
        bool animating = false;
        double wakeAfter = std::numeric_limits<double>::infinity();
    };

    // Presentation state only. Selection and IME geometry never use this cache.
    // Motion uses document coordinates, so scrolling is applied without easing.
    class EditorCaretAnimation {
    public:
        static double Now();
        void Configure(folia::ThemeEditor const& style, bool animations, double blinkHalfPeriod);
        void Focus(bool value);
        void Reset();
        EditorCaretFrame Sample(std::optional<D2D1_RECT_F> target, folia::TextPosition position,
            float scrollOffset, double now, std::uint64_t contentRevision = 0);
        EditorCaretFrame const& Frame() const { return frame; }
    private:
        D2D1_RECT_F Interpolate(double now) const;
        folia::ThemeEditor style;
        folia::TextPosition targetPosition{};
        D2D1_RECT_F from{}, to{};
        EditorCaretFrame frame;
        double movedAt = 0, activeAt = 0, halfPeriod = 0;
        std::uint64_t revision = 0;
        bool initialized = false, focused = true, animate = false;
    };
}
