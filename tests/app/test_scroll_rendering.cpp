#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

import folia.markdown.instrumentation;

using namespace boost::ut;
using namespace winrt::Folia;

namespace {
    suite scroll_rendering_tests = [] {
        "scroll rendering preserves the document through continuous input and its inertia tail"_test = [] {
          const char* stage = "read document";
          auto set_stage = [&](const char* value) {
              stage = value;
              std::fprintf(stderr, "scroll rendering stage=%s\n", stage);
              std::fflush(stderr);
          };
          try {
            wchar_t path[32768]{};
            std::string bytes;
            if (::GetEnvironmentVariableW(L"FOLIA_SCROLL_TEST_DOCUMENT", path, static_cast<DWORD>(std::size(path)))) {
                auto size = std::filesystem::file_size(path);
                expect(size < std::uintmax_t{16 * 1024 * 1024});
                if (size >= 16 * 1024 * 1024) return;
                std::ifstream file(path, std::ios::binary);
                bytes.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
            } else {
                for (int i = 0; i < 500; ++i)
                    bytes += "Paragraph **bold** with plain text, a [link](https://example.com), and `code`.\n\n";
            }
            set_stage("initialize XAML");
            auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel;
            EditorSession session;
            set_stage("parse document");
            session.SetText(winrt::to_hstring(bytes));
            auto original = session.Text();
            auto selection = session.Selection();
            EditorSurfaceRenderer renderer;
            set_stage("initialize renderer");
            renderer.Initialize(panel);
            renderer.Resize(panel, 1280, 800);
            auto context = renderer.CaptureMathPreparationContext();
            set_stage("prepare math");
            renderer.AdoptMathPreparation(EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {}));
            set_stage("first render");
            renderer.Render(session.RenderFrame());
            std::fprintf(stderr, "scroll rendering document_bytes=%zu maximum_offset=%.3f\n", bytes.size(), renderer.MaximumScrollOffset());
            struct Sample { int run, frame; float offset, delta; bool active; double milliseconds; };
            std::vector<Sample> samples;
            set_stage("scroll frames");
            folia::reset_core_operation_counters();
            for (int run = 0; run < 2; ++run) {
                renderer.SetScrollOffset(run == 0 ? 0.f : renderer.MaximumScrollOffset() * .8f);
                bool active = false;
                for (int frame = 0; frame < 300; ++frame) {
                    auto before = renderer.ScrollOffset();
                    if (frame < 120 && frame % 6 == 0) renderer.QueueScrollBy(180);
                    active = renderer.AdvanceScrollAnimation(1.f / 180.f);
                    auto start = std::chrono::steady_clock::now();
                    renderer.Render(session.RenderFrame());
                    auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                    auto offset = renderer.ScrollOffset();
                    expect(std::isfinite(offset) && offset >= 0);
                    samples.push_back({run, frame, offset, offset - before, active, elapsed});
                    // Let asynchronous viewport resources reach the UI just as
                    // they do between real scroll frames, without repaint recursion.
                    MSG message{};
                    for (int pending = 0; pending < 16 && ::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++pending) {
                        ::TranslateMessage(&message);
                        ::DispatchMessageW(&message);
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                expect(!active) << "inertia must finish in the real document" << run;
            }
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(session.Text() == original);
            expect(session.Selection() == selection);
            for (int run = 0; run < 2; ++run)
            for (bool tail : {false, true}) {
                std::vector<double> times;
                for (auto const& sample : samples)
                    if (sample.run == run && (sample.frame >= 120) == tail) times.push_back(sample.milliseconds);
                std::ranges::sort(times);
                std::fprintf(stderr, "scroll rendering run=%d phase=%s median_ms=%.3f p95_ms=%.3f max_ms=%.3f\n",
                    run, tail ? "inertia" : "input", times[times.size() / 2], times[times.size() * 95 / 100], times.back());
            }
            if (::GetEnvironmentVariableW(L"FOLIA_SCROLL_TEST_OUTPUT", path, static_cast<DWORD>(std::size(path)))) {
                std::ofstream out(path);
                out << "Run,Frame,Offset,Delta,Active,RenderMs\n";
                for (auto const& sample : samples)
                    out << sample.run << ',' << sample.frame << ',' << sample.offset << ',' << sample.delta << ',' << sample.active << ',' << sample.milliseconds << '\n';
            }
          } catch (winrt::hresult_error const& error) {
              throw std::runtime_error(std::format("scroll rendering {}: HRESULT 0x{:08X}: {}", stage,
                  static_cast<std::uint32_t>(error.code().value), winrt::to_string(error.message())));
          }
        };
    };
}
