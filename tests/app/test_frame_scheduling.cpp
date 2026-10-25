#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/interaction/EditorFrameScheduler.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace {
    template<class Done> void PumpFrames(Done done) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        while (!done() && std::chrono::steady_clock::now() < deadline) {
            MSG message{};
            if (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                ::TranslateMessage(&message);
                ::DispatchMessageW(&message);
            } else ::MsgWaitForMultipleObjectsEx(0, nullptr, 1, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
    }

    suite frame_scheduling_tests = [] {
        "frame scheduling does not wait for recurring normal UI work to drain"_test = [] {
            auto queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
            auto ready = winrt::handle{::CreateEventW(nullptr, TRUE, TRUE, nullptr)};
            winrt::check_bool(ready.get() != nullptr);
            struct WorkState { std::size_t count = 0; bool active = true; };
            auto state = std::make_shared<WorkState>();
            auto work = std::make_shared<std::function<void()>>();
            *work = [state, weak = std::weak_ptr{work}, queue] {
                if (!state->active) return;
                ++state->count;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                if (state->count < 64)
                    if (auto next = weak.lock()) queue.TryEnqueue(*next);
            };
            std::size_t frames = 0, workAtFrame = 0;
            EditorFrameScheduler scheduler;
            scheduler.Attach(queue, ready.get(), ::GetDesktopWindow(), [&](float) {
                ++frames;
                workAtFrame = state->count;
                return false;
            });
            queue.TryEnqueue(*work);
            scheduler.Start();
            PumpFrames([&] { return frames && state->count == 64; });
            scheduler.Detach();
            state->active = false;
            expect(frames == 1_u);
            expect(state->count == 64_u);
            expect(workAtFrame < 64_u) << "a scroll frame must run while normal UI work remains";
            std::fprintf(stderr, "frame scheduling normal work completed before first scroll frame=%zu/64\n", workAtFrame);
        };

        "frame scheduling samples display cadence without rendering cost"_test = [] {
            auto queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
            auto ready = winrt::handle{::CreateEventW(nullptr, TRUE, TRUE, nullptr)};
            winrt::check_bool(ready.get() != nullptr);
            std::vector<float> intervals;
            EditorFrameScheduler scheduler;
            scheduler.Attach(queue, ready.get(), ::GetDesktopWindow(), [&](float elapsed) {
                expect(std::isfinite(elapsed) && elapsed > 0);
                intervals.push_back(elapsed);
                return intervals.size() < 40;
            });
            scheduler.Start();
            PumpFrames([&] { return intervals.size() == 40; });
            scheduler.Detach();
            expect(intervals.size() == 40_u);
            if (intervals.size() <= 4) return;
            intervals.erase(intervals.begin(), intervals.begin() + 4);
            std::ranges::sort(intervals);
            auto median = intervals[intervals.size() / 2];
            std::fprintf(stderr, "frame scheduling ready-signal cadence median_ms=%.3f p95_ms=%.3f callbacks_per_second=%.1f\n",
                median * 1000, intervals[intervals.size() * 95 / 100] * 1000, 1 / median);
            // A manual ready event deliberately removes GPU/compositor cost.
            // These timings diagnose pacing; they are not displayed-frame FPS.
        };

        "frame scheduling detach cancels a pending compositor or GPU wait"_test = [] {
            auto queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
            auto ready = winrt::handle{::CreateEventW(nullptr, TRUE, FALSE, nullptr)};
            winrt::check_bool(ready.get() != nullptr);
            for (int cycle = 0; cycle < 4; ++cycle) {
                EditorFrameScheduler scheduler;
                std::size_t frames = 0;
                scheduler.Attach(queue, ready.get(), ::GetDesktopWindow(), [&](float) { ++frames; return true; });
                scheduler.Start();
                std::this_thread::sleep_for(std::chrono::milliseconds(12));
                auto start = std::chrono::steady_clock::now();
                scheduler.Detach();
                expect(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
                expect(frames == 0_u);
            }
        };
    };
}
