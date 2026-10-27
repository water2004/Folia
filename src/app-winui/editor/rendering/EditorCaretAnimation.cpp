#include "pch.h"
import std;
#include "editor/rendering/EditorCaretAnimation.h"

namespace winrt::Folia {
    double EditorCaretAnimation::Now() {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    void EditorCaretAnimation::Configure(folia::ThemeEditor const& value, bool animations, double blinkHalfPeriod) {
        if (style == value && animate == animations && halfPeriod == blinkHalfPeriod) return;
        style = value; animate = animations; halfPeriod = blinkHalfPeriod;
        Reset();
    }
    void EditorCaretAnimation::Focus(bool value) {
        if (focused == value) return;
        focused = value;
        Reset();
    }
    void EditorCaretAnimation::Reset() { initialized = false; frame = {}; }
    D2D1_RECT_F EditorCaretAnimation::Interpolate(double now) const {
        auto duration = animate ? style.caret_move_ms / 1000.0 : 0.0;
        auto progress = duration > 0 ? std::clamp((now - movedAt) / duration, 0.0, 1.0) : 1.0;
        auto eased = static_cast<float>(1 - std::pow(1 - progress, 3));
        return {std::lerp(from.left, to.left, eased), std::lerp(from.top, to.top, eased),
            std::lerp(from.right, to.right, eased), std::lerp(from.bottom, to.bottom, eased)};
    }
    EditorCaretFrame EditorCaretAnimation::Sample(std::optional<D2D1_RECT_F> target,
        folia::TextPosition position, float scrollOffset, double now, std::uint64_t contentRevision) {
        frame = {};
        if (!focused || !target) { initialized = false; return frame; }
        auto rect = *target;
        rect.top += scrollOffset; rect.bottom += scrollOffset;
        bool moved = initialized && targetPosition != position;
        if (!initialized) {
            from = to = rect; movedAt = activeAt = now;
        } else if (moved) {
            from = Interpolate(now); to = rect;
            // A distant navigation should not sweep across unrelated paragraphs.
            if (std::hypot(to.left - from.left, to.top - from.top) > (to.bottom - to.top) * 8)
                from = to;
            movedAt = activeAt = now;
        } else if (std::abs(to.left - rect.left) > .01f || std::abs(to.top - rect.top) > .01f
            || std::abs(to.bottom - rect.bottom) > .01f) {
            // Relayout is immediate; only a changed source caret is eased.
            from = to = rect; movedAt = now;
        }
        if (initialized && revision != contentRevision) activeAt = now;
        initialized = true; targetPosition = position; revision = contentRevision;
        frame.bounds = Interpolate(now);
        frame.bounds.top -= scrollOffset; frame.bounds.bottom -= scrollOffset;
        frame.opacity = 1;
        auto duration = animate ? style.caret_move_ms / 1000.0 : 0.0;
        frame.animating = duration > 0 && now < movedAt + duration
            && (std::abs(from.left - to.left) > .01f || std::abs(from.top - to.top) > .01f
                || std::abs(from.bottom - to.bottom) > .01f);
        if (halfPeriod > 0) {
            auto phase = std::fmod((std::max)(0.0, now - activeAt), 2 * halfPeriod);
            auto fade = animate ? (std::min)(halfPeriod, style.caret_fade_ms / 1000.0) : 0.0;
            auto smooth = [](double t) { return static_cast<float>(t * t * (3 - 2 * t)); };
            if (fade > 0 && phase >= halfPeriod - fade && phase < halfPeriod) {
                frame.opacity = 1 - smooth((phase - halfPeriod + fade) / fade);
                frame.animating = true;
            } else if (fade > 0 && phase >= 2 * halfPeriod - fade) {
                frame.opacity = smooth((phase - 2 * halfPeriod + fade) / fade);
                frame.animating = true;
            } else {
                frame.opacity = phase < halfPeriod ? 1.f : 0.f;
                frame.wakeAfter = (phase < halfPeriod ? halfPeriod : 2 * halfPeriod) - fade - phase;
            }
        }
        return frame;
    }
}
