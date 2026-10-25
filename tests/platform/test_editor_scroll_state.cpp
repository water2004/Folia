#include "support/folia_test.hpp"

import folia.platform.editor_scroll_state;

using namespace boost::ut;
using folia::platform::editor::EditorScrollState;

suite editor_scroll_state_tests = [] {

"wheel starts immediately and retains a quarter second deceleration"_test = [] {
    EditorScrollState state;
    state.Queue(480.0f, 2000.0f);
    state.Advance(1.0f / 60.0f);
    expect(state.Offset() > 0.0_f);
    expect(state.Offset() < 96.0_f);
    for (auto frame = 1; frame < 6; ++frame) state.Advance(1.0f / 60.0f);
    expect(state.Offset() >= 240.0_f);
    expect(state.Offset() <= 384.0_f);
    for (auto frame = 6; frame < 15; ++frame) state.Advance(1.0f / 60.0f);
    expect(state.Offset() >= 432.0_f);
    expect(state.Offset() <= state.Target());
};

"queued scroll is monotonic and frame-rate independent"_test = [] {
    EditorScrollState sixtyHz;
    EditorScrollState oneTwentyHz;
    sixtyHz.Queue(480.0f, 2000.0f);
    oneTwentyHz.Queue(480.0f, 2000.0f);

    auto previous = sixtyHz.Offset();
    for (auto frame = 0; frame < 60; ++frame)
    {
        sixtyHz.Advance(1.0f / 60.0f);
        expect(sixtyHz.Offset() >= previous);
        expect(sixtyHz.Offset() <= sixtyHz.Target());
        previous = sixtyHz.Offset();
    }
    for (auto frame = 0; frame < 120; ++frame)
        oneTwentyHz.Advance(1.0f / 120.0f);

    expect(std::fabs(sixtyHz.Offset() - oneTwentyHz.Offset()) < 0.01f);
};

"direction reversal starts from the visible offset"_test = [] {
    EditorScrollState state;
    state.Set(500.0f, 2000.0f);
    state.Queue(300.0f, 2000.0f);
    state.Advance(1.0f / 60.0f);
    auto visible = state.Offset();
    state.Queue(-100.0f, 2000.0f);

    expect(state.Target() < visible);
    expect(std::fabs(state.Target() - (visible - 100.0f)) < 0.01f);
};

"animation state covers queued motion until it settles"_test = [] {
    EditorScrollState state;
    expect(!state.Animating());
    state.Queue(240.0f, 1000.0f);
    expect(state.Animating());
    for (auto frame = 0; frame < 240 && state.Animating(); ++frame)
        state.Advance(1.0f / 120.0f);
    expect(!state.Advance(1.0f / 120.0f));
    expect(!state.Animating());
};

"inertia settles at large document offsets across refresh rates"_test = [] {
    for (float start : {0.f, 1000.f, 100000.f, 131072.f, 500000.f, 1000000.f, 8000000.f})
    for (float delta : {-1440.f, -180.f, 180.f, 1440.f})
    for (int rate : {30, 60, 120, 144, 180, 240, 500}) {
        EditorScrollState state;
        state.Set(start, 10000000.f);
        state.Queue(delta, 10000000.f);
        auto target = state.Target();
        bool monotonic = true;
        bool active = true;
        for (int frame = 0; frame < rate * 3 && active; ++frame) {
            auto previous = state.Offset();
            active = state.Advance(1.f / rate);
            monotonic = monotonic && (delta < 0 ? state.Offset() <= previous : state.Offset() >= previous);
        }
        expect(monotonic);
        expect(!active) << "animation must stop" << start << rate;
        expect(!state.Animating()) << "offset/rate" << start << rate;
        expect(state.Offset() == target) << "inertia must reach its target" << start << rate;
    }
};

"extent changes and anchor shifts clamp offset and target together"_test = [] {
    EditorScrollState state;
    state.Set(900.0f, 1000.0f);
    state.Queue(100.0f, 1000.0f);
    state.Shift(80.0f, 1200.0f);
    expect(state.Offset() == 980.0_f);
    expect(state.Target() == 1080.0_f);

    state.Clamp(700.0f);
    expect(state.Offset() == 700.0_f);
    expect(state.Target() == 700.0_f);

    auto snapshot = state.Save();
    state.Set(50.0f, 700.0f);
    state.Restore(snapshot, 600.0f);
    expect(state.Offset() == 600.0_f);
    expect(state.Target() == 600.0_f);
};

}; // suite editor_scroll_state_tests
