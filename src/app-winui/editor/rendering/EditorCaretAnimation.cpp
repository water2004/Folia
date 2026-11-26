#include "pch.h"
import std;
#include "editor/rendering/EditorCaretAnimation.h"

namespace winrt::Folia {
    float EditorMotion::Progress(double elapsed, double duration) {
        auto progress = duration > 0 ? std::clamp(elapsed / duration, 0.0, 1.0) : 1.0;
        return static_cast<float>(1 - std::pow(1 - progress, 3));
    }

    D2D1_RECT_F EditorMotion::Interpolate(D2D1_RECT_F from, D2D1_RECT_F to, float eased) {
        return {std::lerp(from.left, to.left, eased), std::lerp(from.top, to.top, eased),
            std::lerp(from.right, to.right, eased), std::lerp(from.bottom, to.bottom, eased)};
    }

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
        return EditorMotion::Interpolate(from, to, EditorMotion::Progress(now - movedAt, duration));
    }
    EditorCaretFrame EditorCaretAnimation::Sample(std::optional<D2D1_RECT_F> target,
        folia::TextPosition position, float scrollOffset, double now, std::uint64_t contentRevision, bool steady) {
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
        if (halfPeriod > 0 && !steady) {
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

    namespace {
        auto RectOrder(EditorSelectionRect const& rect) {
            return std::pair{rect.bounds.top, rect.bounds.left};
        }

        bool SameGeometry(EditorSelectionRect const& a, EditorSelectionRect const& b) {
            auto nearlyEqual = [](float x, float y) { return std::abs(x - y) < .05f; };
            return a.rtl == b.rtl && nearlyEqual(a.bounds.left, b.bounds.left)
                && nearlyEqual(a.bounds.right, b.bounds.right) && nearlyEqual(a.bounds.top, b.bounds.top)
                && nearlyEqual(a.bounds.bottom, b.bounds.bottom);
        }

        bool OverlappingLine(D2D1_RECT_F a, D2D1_RECT_F b) {
            return std::abs(a.top - b.top) < .5f && a.left <= b.right + .5f && b.left <= a.right + .5f;
        }

        D2D1_RECT_F Collapsed(EditorSelectionRect const& rect, bool forward) {
            auto bounds = rect.bounds;
            bounds.left = bounds.right = forward != rect.rtl ? bounds.left : bounds.right;
            return bounds;
        }
    }

    void EditorSelectionAnimation::Configure(double value) {
        if (duration == value) return;
        duration = value;
        Reset();
    }

    void EditorSelectionAnimation::Focus(bool value) {
        if (focused == value) return;
        focused = value;
        Reset();
    }

    void EditorSelectionAnimation::Reset() {
        tracks.clear();
        displayed.clear();
        initialized = animating = false;
    }

    void EditorSelectionAnimation::BeginFrame(folia::TextSelection value, bool direction,
        float scrollOffset, double time, std::uint64_t contentRevision) {
        auto continuous = initialized && revision == contentRevision && selection.anchor == value.anchor;
        if (!continuous) tracks.clear();
        animateChange = continuous && selection != value && focused && duration > 0;
        previousForward = forward;
        forward = direction;
        selection = value;
        scroll = scrollOffset;
        now = time;
        revision = contentRevision;
        initialized = true;
        animating = false;
        for (auto& [owner, track] : tracks) track.seen = false;
    }

    std::span<const D2D1_RECT_F> EditorSelectionAnimation::Sample(
        folia::NodeId owner, std::vector<EditorSelectionRect> target) {
        displayed.clear();
        auto found = tracks.find(owner.v);
        if (target.empty() && found == tracks.end()) return displayed;
        auto& track = found == tracks.end() ? tracks[owner.v] : found->second;
        track.seen = true;
        for (auto& rect : target) {
            rect.bounds.top += scroll;
            rect.bounds.bottom += scroll;
        }
        std::ranges::sort(target, {}, RectOrder);
        if (!std::ranges::equal(track.target, target, SameGeometry)) {
            std::vector<EditorSelectionRect> from;
            if (animateChange) {
                auto progress = EditorMotion::Progress(now - track.began, duration);
                if (!track.motion.empty() && progress < 1) {
                    from.reserve(track.motion.size());
                    for (auto const& item : track.motion)
                        from.push_back({EditorMotion::Interpolate(item.from, item.to, progress), item.rtl});
                    std::ranges::sort(from, {}, RectOrder);
                } else {
                    from = std::move(track.target);
                }
            }
            track.motion.clear();
            if (animateChange) {
                auto a = from.begin(), b = target.begin();
                while (a != from.end() || b != target.end()) {
                    if (a != from.end() && b != target.end() && OverlappingLine(a->bounds, b->bounds)) {
                        track.motion.push_back({a->bounds, b->bounds, b->rtl});
                        ++a; ++b;
                    } else if (b == target.end() || (a != from.end() && RectOrder(*a) < RectOrder(*b))) {
                        track.motion.push_back({a->bounds, Collapsed(*a, previousForward), a->rtl});
                        ++a;
                    } else {
                        track.motion.push_back({Collapsed(*b, forward), b->bounds, b->rtl});
                        ++b;
                    }
                }
                track.began = now;
            }
            track.target = std::move(target);
        }
        auto append = [&](D2D1_RECT_F rect) {
            rect.top -= scroll;
            rect.bottom -= scroll;
            if (rect.left < rect.right) displayed.push_back(rect);
        };
        auto progress = EditorMotion::Progress(now - track.began, duration);
        if (!track.motion.empty() && progress < 1) {
            animating = true;
            for (auto const& item : track.motion) append(EditorMotion::Interpolate(item.from, item.to, progress));
        } else {
            track.motion.clear();
            for (auto const& rect : track.target) append(rect.bounds);
        }
        return displayed;
    }

    void EditorSelectionAnimation::EndFrame() {
        std::erase_if(tracks, [](auto const& entry) {
            auto const& track = entry.second;
            return !track.seen || (track.target.empty() && track.motion.empty());
        });
    }

    bool EditorSelectionAnimation::HitTest(float x, float y) const {
        y += scroll;
        for (auto const& [owner, track] : tracks)
            for (auto const& rect : track.target)
                if (x >= rect.bounds.left && x < rect.bounds.right
                    && y >= rect.bounds.top && y < rect.bounds.bottom) return true;
        return false;
    }

    void EditorTableSelectionAnimation::Configure(double value) {
        if (duration == value) return;
        duration = value;
        Reset();
    }

    void EditorTableSelectionAnimation::Reset() {
        table = {};
        animateChange = animating = seen = false;
    }

    void EditorTableSelectionAnimation::BeginFrame(folia::TextSelection value,
        std::uint64_t contentRevision, double time) {
        if (revision != contentRevision || !value.selects_cells()) Reset();
        animateChange = duration > 0 && selection != value;
        selection = value;
        revision = contentRevision;
        now = time;
        seen = false;
    }

    D2D1_RECT_F EditorTableSelectionAnimation::Sample(folia::NodeId owner, D2D1_RECT_F target) {
        const auto sample = [&] {
            return EditorMotion::Interpolate(from, to, EditorMotion::Progress(now - began, duration));
        };
        if (table != owner || target.left != to.left || target.right != to.right
            || target.top != to.top || target.bottom != to.bottom) {
            animating = table == owner && animateChange;
            from = animating ? sample() : target;
            to = target;
            began = now;
        }
        table = owner;
        seen = true;
        animating = animating && now < began + duration;
        return animating ? sample() : to;
    }

    void EditorTableSelectionAnimation::EndFrame() {
        if (!seen) Reset();
    }
}
