#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/rendering/EditorCaretAnimation.h"

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;
    folia::TextPosition At(std::size_t offset) { return {folia::NodeId{1}, offset, folia::TextAffinity::Downstream}; }
    auto Rect(float x, float y = 20) { return D2D1::RectF(x, y, x + 1, y + 24); }
    suite caret_animation_tests = [] {
        "caret moves with eased retargeting without changing source position"_test = [] {
            EditorCaretAnimation animation;
            folia::ThemeEditor theme;
            animation.Configure(theme, true, .5);
            auto first = animation.Sample(Rect(10), At(0), 0, 0);
            expect(first.bounds.left == 10.f && !first.animating);
            auto start = animation.Sample(Rect(50), At(1), 0, .1);
            expect(start.bounds.left == 10.f && start.animating && start.opacity == 1.f);
            auto middle = animation.Sample(Rect(50), At(1), 0, .155);
            expect(std::abs(middle.bounds.left - 45.f) < .01f);
            auto retarget = animation.Sample(Rect(90), At(2), 0, .155);
            expect(std::abs(retarget.bounds.left - middle.bounds.left) < .01f);
            auto end = animation.Sample(Rect(90), At(2), 0, .3);
            expect(end.bounds.left == 90.f && !end.animating);
            expect(std::isfinite(end.wakeAfter) && end.wakeAfter > 0);
        };
        "caret follows scroll immediately and snaps relayout navigation and document changes"_test = [] {
            EditorCaretAnimation animation;
            animation.Configure({}, true, .5);
            animation.Sample(Rect(10, 100), At(0), 0, 0);
            animation.Sample(Rect(50, 100), At(1), 0, .1);
            auto scrolled = animation.Sample(Rect(50, 60), At(1), 40, .155);
            expect(std::abs(scrolled.bounds.left - 45.f) < .01f);
            expect(scrolled.bounds.top == 60.f);
            auto relayout = animation.Sample(Rect(15, 84), At(1), 40, .16);
            expect(relayout.bounds.left == 15.f && relayout.bounds.top == 84.f && !relayout.animating);
            auto distant = animation.Sample(Rect(15, 1000), At(2), 40, .2);
            expect(distant.bounds.top == 1000.f && !distant.animating);
            animation.Reset();
            auto switched = animation.Sample(Rect(55), At(0), 0, .21);
            expect(switched.bounds.left == 55.f && !switched.animating);
        };
        "caret fades at system blink cadence and sleeps between transitions"_test = [] {
            EditorCaretAnimation animation;
            animation.Configure({}, true, .5);
            auto first = animation.Sample(Rect(10), At(0), 0, 0);
            expect(!first.animating && std::abs(first.wakeAfter - .4) < .001);
            auto fading = animation.Sample(Rect(10), At(0), 0, .45);
            expect(fading.animating && std::abs(fading.opacity - .5f) < .001f);
            auto hidden = animation.Sample(Rect(10), At(0), 0, .6);
            expect(!hidden.animating && hidden.opacity == 0.f && hidden.wakeAfter > .29);
            auto appearing = animation.Sample(Rect(10), At(0), 0, .95);
            expect(appearing.animating && std::abs(appearing.opacity - .5f) < .001f);
            auto typed = animation.Sample(Rect(20), At(1), 0, .96);
            expect(typed.opacity == 1.f);
            auto stationary = animation.Sample(Rect(20), At(1), 0, 1.6);
            expect(stationary.opacity == 0.f);
            auto deleted = animation.Sample(Rect(20), At(1), 0, 1.61, 1);
            expect(deleted.opacity == 1.f && !deleted.animating);
            animation.Focus(false);
            auto unfocused = animation.Sample(Rect(20), At(1), 0, 1);
            expect(unfocused.opacity == 0.f && !unfocused.animating && !std::isfinite(unfocused.wakeAfter));
            animation.Focus(true);
            auto focused = animation.Sample(Rect(20), At(1), 0, 1.1);
            expect(focused.opacity == 1.f && !focused.animating);
            auto selection = animation.Sample({}, At(1), 0, 1.2);
            expect(selection.opacity == 0.f && !std::isfinite(selection.wakeAfter));
        };
        "caret respects disabled animation and disabled system blinking"_test = [] {
            EditorCaretAnimation animation;
            animation.Configure({}, false, .5);
            animation.Sample(Rect(10), At(0), 0, 0);
            auto moved = animation.Sample(Rect(50), At(1), 0, .1);
            expect(moved.bounds.left == 50.f && !moved.animating);
            auto hidden = animation.Sample(Rect(50), At(1), 0, .7);
            expect(hidden.opacity == 0.f && !hidden.animating);
            animation.Configure({}, true, 0);
            animation.Sample(Rect(50), At(1), 0, .7);
            auto solid = animation.Sample(Rect(50), At(1), 0, 10);
            expect(solid.opacity == 1.f && !solid.animating && !std::isfinite(solid.wakeAfter));
        };
    };
}
