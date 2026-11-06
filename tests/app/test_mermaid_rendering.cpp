#include "pch.h"
#include "support/folia_test.hpp"
#include "media/MermaidRenderer.h"
#include "media/SvgNormalizer.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    MermaidSvg AwaitMermaid(MermaidRenderer& renderer, std::string_view source, bool dark)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        do
        {
            if (auto result = renderer.GetOrQueue(source, dark)) return *result;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error("native Mermaid worker exceeded 15 seconds");
    }

    suite native_mermaid_worker_tests = []
    {
        "native Mermaid worker normalizes SVG and separates source and theme cache entries"_test = []
        {
            std::atomic_size_t completions{0};
            MermaidRenderer renderer;
            renderer.SetCompletionCallback([&] { ++completions; });
            constexpr std::string_view source = "flowchart TD\n A[Start 中文] --> B[Finish]";
            expect(!renderer.GetOrQueue(source, false, false).has_value());
            auto light = AwaitMermaid(renderer, source, false);
            auto dark = AwaitMermaid(renderer, source, true);
            auto edited = AwaitMermaid(renderer, "flowchart TD\n A[Edited] --> B[Finish]", false);
            for (auto const* result : {&light, &dark, &edited})
            {
                expect(static_cast<bool>(*result)) << result->error;
                expect(result->svg.find("<svg") != std::string::npos);
                expect(result->svg.find("<text") == std::string::npos);
                expect(result->svg.find("<foreignObject") == std::string::npos);
            }
            expect(light.svg != dark.svg);
            expect(light.svg != edited.svg);
            expect(light.renderId != dark.renderId && light.renderId != edited.renderId);
            auto cached = renderer.GetOrQueue(source, false, false);
            expect(cached.has_value());
            if (cached) expect(cached->renderId == light.renderId && cached->svg == light.svg);
            // Completion executes after storing; destruction joins any final callback.
            renderer.SetCompletionCallback({});
            expect(completions.load() >= 2u);
        };

        "native Mermaid normalizer supports synchronous use before its first queued request"_test = []
        {
            SvgNormalizer normalizer;
            constexpr std::string_view source = R"(<svg xmlns="http://www.w3.org/2000/svg" width="120" height="80"><text x="5" y="30">中文 SVG</text></svg>)";
            auto synchronous = normalizer.NormalizeNow(source, 16);
            expect(static_cast<bool>(synchronous)) << synchronous.error;
            expect(!normalizer.GetOrQueue(source, 16, false).has_value());
            expect(!normalizer.GetOrQueue(source, 16).has_value());
            // Synchronous work remains safe while the lazy queue worker runs.
            auto concurrent = normalizer.NormalizeNow(source, 16);
            expect(static_cast<bool>(concurrent));
            std::optional<NormalizedSvg> queued;
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            do {
                queued = normalizer.GetOrQueue(source, 16, false);
                if (queued) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            } while (std::chrono::steady_clock::now() < deadline);
            expect(fatal(queued.has_value()));
            expect(static_cast<bool>(*queued)) << queued->error;
            if (synchronous && *queued) {
                expect(*queued->svg == *synchronous.svg);
                expect(queued->width == synchronous.width && queued->height == synchronous.height);
            }
        };

        "native Mermaid worker preserves the Info intrinsic viewport"_test = []
        {
            MermaidRenderer renderer;
            auto info = AwaitMermaid(renderer, "info", false);
            expect(static_cast<bool>(info)) << info.error;
            expect(info.width == 400.0f && info.height == 100.0f);
            expect(info.svg.find("width=\"400\"") != std::string::npos);
            expect(info.svg.find("height=\"100\"") != std::string::npos);
        };

        "native Mermaid worker reports errors and clears in-flight generations"_test = []
        {
            MermaidRenderer renderer;
            auto error = AwaitMermaid(renderer, "not a Mermaid diagram", false);
            expect(!static_cast<bool>(error) && !error.error.empty());
            constexpr std::string_view source = "sequenceDiagram\n Alice->>Bob: Hello";
            // A clear may race the initial render. Only the new generation is
            // allowed to erase its queued key or publish into its cache.
            renderer.GetOrQueue(source, false);
            renderer.Clear();
            expect(!renderer.GetOrQueue(source, false, false).has_value());
            auto rendered = AwaitMermaid(renderer, source, false);
            expect(static_cast<bool>(rendered)) << rendered.error;
            renderer.Clear();
            expect(!renderer.GetOrQueue(source, false, false).has_value());
            auto again = AwaitMermaid(renderer, source, false);
            expect(static_cast<bool>(again)) << again.error;
            expect(again.renderId > rendered.renderId);
            expect(again.width == rendered.width && again.height == rendered.height);
        };
    };
}
