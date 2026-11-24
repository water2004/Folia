#pragma once

import folia.markdown.text_edit;
import folia.markdown.ids;
import folia.core.theme;

namespace winrt::Folia {
    struct EditorMotion {
        static float Progress(double elapsed, double duration);
        static D2D1_RECT_F Interpolate(D2D1_RECT_F from, D2D1_RECT_F to, float progress);
    };

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
            float scrollOffset, double now, std::uint64_t contentRevision = 0, bool steady = false);
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

    struct EditorSelectionRect {
        D2D1_RECT_F bounds{};
        bool rtl = false;
    };

    // Presentation rectangles keyed by the layout owner. Geometry used by
    // hit testing always follows the logical selection, independently of easing.
    class EditorSelectionAnimation {
    public:
        void Configure(double duration);
        void Focus(bool value);
        void Reset();
        void BeginFrame(folia::TextSelection selection, bool forward, float scrollOffset,
            double now, std::uint64_t revision);
        std::span<const D2D1_RECT_F> Sample(folia::NodeId owner, std::vector<EditorSelectionRect> target);
        void EndFrame();
        bool HitTest(float x, float y) const;
        bool Animating() const { return animating; }

    private:
        struct Transition {
            D2D1_RECT_F from, to;
            bool rtl;
        };
        struct Track {
            std::vector<EditorSelectionRect> target;
            std::vector<Transition> motion;
            double began = 0;
            bool seen = false;
        };
        std::unordered_map<std::uint64_t, Track> tracks;
        std::vector<D2D1_RECT_F> displayed;
        folia::TextSelection selection{};
        std::uint64_t revision = 0;
        double now = 0, duration = 0;
        float scroll = 0;
        bool focused = true, initialized = false, animateChange = false, animating = false;
        bool previousForward = true, forward = true;
    };
}
