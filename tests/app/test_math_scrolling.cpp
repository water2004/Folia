#include "pch.h"
#include "support/folia_test.hpp"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/session/EditorSession.h"
#include "editor/interaction/EditorFrameScheduler.h"
#include "editor/rendering/EditorContentPreparation.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    suite math_scrolling_tests = []
    {
        "math scrolling canceled work does not request another repaint"_test = []
        {
            MathJaxRenderer math;
            auto completions = std::make_shared<std::atomic_size_t>(0);
            auto published = std::make_shared<winrt::handle>(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
            winrt::check_bool(published->get() != nullptr);
            math.SetCompletionCallback([&math, completions, published]
            {
                ++*completions;
                if (math.Lookup("x+2", false, 16, 500, "#000000")) ::SetEvent(published->get());
            });
            std::stop_source stop;
            auto batch = math.BeginBatch();
            math.BeginBlock(1);
            batch.Render("x+1", false, 16, 500, "#000000", stop.get_token());
            math.EndBlock();
            stop.request_stop();
            batch.Submit();
            math.WaitForIdle();
            expect(math.Snapshot().pending == 0_u);
            expect(math.Snapshot().entries == 0_u);
            // Returning to this owner must still be able to render normally.
            math.BeginBlock(1);
            math.Render("x+2", false, 16, 500, "#000000");
            math.EndBlock();
            math.WaitForIdle();
            expect(::WaitForSingleObject(published->get(), 5000) == WAIT_OBJECT_0);
            expect(completions->load() == 1_u);
            expect(math.Lookup("x+2", false, 16, 500, "#000000") != nullptr);
            expect(math.Snapshot().entries == 1_u);
            math.SetCompletionCallback({});
        };

        "math scrolling offscreen inline layout does not start native work"_test = []
        {
            EditorSession session;
            session.SetText(L"Text $x+1$ here.\n");
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
            auto const& block = frame.renderModel.blocks.front();
            MathJaxRenderer math;
            SvgNormalizer normalizer;
            auto prepare = [&](std::uint64_t owner, bool visible)
            {
                math.BeginBlock(owner);
                auto display = BuildDisplayInlineText(block.inline_items, {}, {}, math, normalizer,
                    D2D1::ColorF(D2D1::ColorF::Black), 16, 500, true, true, visible);
                math.EndBlock();
                return display;
            };
            auto offscreen = prepare(1, false);
            expect(offscreen.pendingMath);
            expect(math.Snapshot().entries == 0_u);
            expect(math.Snapshot().nativeBatches == 0_u);
            prepare(1, true);
            math.WaitForIdle();
            expect(math.Snapshot().nativeBatches == 1_u);
            auto cached = prepare(2, false);
            std::uint64_t retained = 2;
            math.RetainBlocks(std::span{&retained, std::size_t{1}});
            expect(!cached.pendingMath);
            expect(math.Snapshot().entries == 1_u);
            expect(math.Snapshot().nativeBatches == 1_u);
            expect(math.Snapshot().fileReads == 0_u);
            expect(math.Snapshot().residentSvgBytes == 0_u);
        };

        "math scrolling offscreen display layout does not start native work"_test = []
        {
            EditorSession session;
            session.SetText(L"$$\nx+1\n$$\n");
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
            auto const& block = frame.renderModel.blocks.front();
            MathJaxRenderer math;
            SvgNormalizer normalizer;
            math.BeginBlock(block.id.v);
            auto display = BuildMathBlockText(block, {}, math, normalizer,
                D2D1::ColorF(D2D1::ColorF::Black), 16, 500, true, true, false);
            math.EndBlock();
            expect(display.pendingMath);
            expect(math.Snapshot().entries == 0_u);
            expect(math.Snapshot().nativeBatches == 0_u);
        };

        "math scrolling frame scheduling lets ordinary UI work run"_test = []
        {
            auto queue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
            auto frameReady = winrt::handle{::CreateEventW(nullptr, TRUE, TRUE, nullptr)};
            winrt::check_bool(frameReady.get() != nullptr);
            EditorFrameScheduler scheduler;
            std::size_t frames = 0;
            std::size_t inputAtFrame = 0;
            scheduler.Attach(queue, frameReady.get(), nullptr, [&](float)
            {
                ++frames;
                if (frames == 1) queue.TryEnqueue([&] { inputAtFrame = frames; });
                if (frames < 12) scheduler.Start();
                // A costly frame can outlast the display interval; another
                // scroll update may already have requested its successor.
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                return frames < 12;
            });
            scheduler.Start();
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while ((!inputAtFrame || frames < 12) && std::chrono::steady_clock::now() < deadline)
            {
                MSG message{};
                if (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    ::TranslateMessage(&message);
                    ::DispatchMessageW(&message);
                }
            }
            scheduler.Detach();
            expect(frames == 12_u);
            expect(inputAtFrame > 0_u && inputAtFrame <= 2_u);
            std::cout << "math scroll input served at frame=" << inputAtFrame << '\n';
        };

        "math scrolling returns to input queue before deferred layout frames"_test = []
        {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSurfaceRenderer renderer;
            EditorSession session;
            std::wstring source;
            for (int i = 0; i < 600; ++i)
                source += L"Paragraph $x+" + std::to_wstring(i) + L"$ with text.\n\n";
            session.SetText(winrt::hstring(source));
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            auto context = renderer.CaptureMathPreparationContext();
            auto preparation = EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {});
            renderer.AdoptMathPreparation(std::move(preparation));
            std::size_t callbacks = 0;
            std::size_t depth = 0;
            std::size_t maximumDepth = 0;
            bool inputProcessed = false;
            auto queue = panel.DispatcherQueue();
            queue.TryEnqueue([&] { inputProcessed = true; });
            renderer.SetInvalidateCallback([&]
            {
                ++callbacks;
                ++depth;
                maximumDepth = (std::max)(maximumDepth, depth);
                if (callbacks < 80) renderer.Render(session.RenderFrame());
                --depth;
            });
            auto frame = session.RenderFrame();
            auto materialize = frame.materializeBlocks;
            bool newerFrameRequested = false;
            frame.materializeBlocks = [&](std::size_t begin, std::size_t end)
            {
                if (!newerFrameRequested)
                {
                    newerFrameRequested = true;
                    renderer.Render(session.RenderFrame());
                }
                if (materialize) materialize(begin, end);
            };
            renderer.Render(frame);
            expect(callbacks == 0_u);
            expect(newerFrameRequested);
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!inputProcessed && std::chrono::steady_clock::now() < deadline)
            {
                MSG message{};
                if (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    ::TranslateMessage(&message);
                    ::DispatchMessageW(&message);
                }
            }
            expect(inputProcessed);
            while (callbacks == 0 && std::chrono::steady_clock::now() < deadline)
            {
                MSG message{};
                if (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    ::TranslateMessage(&message);
                    ::DispatchMessageW(&message);
                }
            }
            expect(callbacks > 0_u);
            expect(maximumDepth == 1_u);
            std::cout << "math scroll callback count=" << callbacks << " max depth=" << maximumDepth << '\n';
            renderer.SetInvalidateCallback({});
        };

        "math scrolling rapid viewport replacements settle without blocking UI"_test = []
        {
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSurfaceRenderer renderer;
            EditorSession session;
            std::wstring source;
            for (int i = 0; i < 400; ++i)
                source += L"Text $\\frac{x+" + std::to_wstring(i) + L"}{y+1}$ end.\n\n";
            session.SetText(winrt::hstring(source));
            auto original = session.TextUtf8();
            auto selection = session.Selection();
            renderer.Initialize(panel);
            renderer.Resize(panel, 900, 600);
            renderer.AdoptMathPreparation(EditorSurfaceRenderer::PrepareMath(
                session.RenderFrame(), renderer.CaptureMathPreparationContext(), {}, {}));
            std::size_t frames = 0;
            std::size_t depth = 0;
            std::size_t maximumDepth = 0;
            auto lastFrame = std::chrono::steady_clock::now();
            renderer.SetInvalidateCallback([&]
            {
                ++frames;
                ++depth;
                maximumDepth = (std::max)(maximumDepth, depth);
                if (frames < 100) renderer.Render(session.RenderFrame());
                lastFrame = std::chrono::steady_clock::now();
                --depth;
            });
            renderer.Render(session.RenderFrame());
            for (int i = 0; i < 20; ++i)
            {
                auto fraction = static_cast<float>((i * 37) % 101) / 100.0f;
                renderer.SetScrollOffset(renderer.MaximumScrollOffset() * fraction);
                renderer.Render(session.RenderFrame());
            }
            bool inputProcessed = false;
            panel.DispatcherQueue().TryEnqueue([&] { inputProcessed = true; });
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
            while (std::chrono::steady_clock::now() < deadline)
            {
                MSG message{};
                if (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    ::TranslateMessage(&message);
                    ::DispatchMessageW(&message);
                }
                ::MsgWaitForMultipleObjectsEx(0, nullptr, 1, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                if (inputProcessed
                    && std::chrono::steady_clock::now() - lastFrame > std::chrono::milliseconds(150)) break;
            }
            renderer.SetInvalidateCallback({});
            expect(inputProcessed);
            // If direct frames already finished the work, zero followup
            // frames is correct; only a continuing redraw loop is a failure.
            expect(frames < 100_u);
            expect(maximumDepth <= 1_u);
            expect(session.TextUtf8() == original);
            expect(session.Selection() == selection);
            std::cout << "rapid math scroll replacements=20 followup frames=" << frames
                      << " maximum depth=" << maximumDepth << '\n';
        };
    };
}
