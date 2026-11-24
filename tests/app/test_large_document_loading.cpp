#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorTextLayoutEngine.h"
#include <psapi.h>

using namespace boost::ut;

namespace
{
    suite large_document_loading_tests = []
    {
        "display append offsets preserve UTF16 mapping across small Unicode runs"_test = []
        {
            winrt::Folia::DisplayInlineText display;
            std::u32string expected;
            std::size_t offset = 0;
            for (auto text : {U"abc", U"\U0001f600", U"e\u0301", U"\r\n", U"中文"}) {
                auto begin = display.displayToSource.size();
                std::u32string piece(text);
                folia::platform::editor::AppendSourceText(display, piece, {folia::NodeId{7}, {offset, offset + piece.size()}}, {}, false);
                expect(display.ranges.back().start == begin);
                for (auto ch : piece) {
                    expect(display.displayToSource[begin++].source_offset == offset);
                    if (ch > 0xffff) expect(display.displayToSource[begin++].source_offset == offset);
                    ++offset;
                }
                expected += piece;
            }
            expect(bool(display.text == expected));
            winrt::Folia::DisplayInlineText tail;
            folia::platform::editor::AppendGeneratedText(tail, U"xy", {folia::NodeId{8}, 4, folia::TextAffinity::Upstream}, {});
            tail.displayToSource.push_back({folia::NodeId{8}, 4, folia::TextAffinity::Upstream});
            auto start = display.displayToSource.size();
            winrt::Folia::MergeDisplayText(display, std::move(tail));
            expect(display.ranges.back().start == start);
            expect(display.displayToSource.size() == start + 2);
            expect(display.displayToSource.back().container_id == folia::NodeId{8});
        };

        "large document loading stages remain bounded"_test = []
        {
            std::string source = "Sample Markdown file\nGenerated locally by TrueFileSize.\n";
            constexpr std::string_view section =
                "Sample markdown list item for parser and upload testing.\n";
            while (source.size() < 10 * 1024 * 1024) source += section;
            auto start = std::chrono::steady_clock::now();
            auto log = [&](std::string_view stage)
            {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                PROCESS_MEMORY_COUNTERS memory{sizeof(memory)};
                GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory));
                std::cerr << stage << ": " << ms << " ms, working set "
                    << memory.WorkingSetSize / 1024 / 1024 << " MB, peak "
                    << memory.PeakWorkingSetSize / 1024 / 1024 << " MB\n";
            };
            winrt::Folia::EditorSession session;
            session.Open(nullptr, winrt::to_hstring(source), {}, {},
                [&](folia::ParseProgress progress)
                {
                    if (progress.total && progress.completed == progress.total) log("parse phase complete");
                });
            log("session open");
            expect(session.RenderModel().blocks.size() == 1_u);
            expect(session.RenderModel().blocks.front().inline_items.size() == 1_u);
            auto serialized = session.TextUtf8();
            log("serialization");
            expect(serialized == source);
            winrt::Folia::EditorMathPreparationContext context{
                folia::default_theme_profile(), 0, 1200, 800, true};
            auto prepared = winrt::Folia::EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {});
            log("math preparation");
            expect(prepared != nullptr);
            auto const& block = session.RenderModel().blocks.front();
            folia::platform::MathJaxRenderer math;
            folia::platform::SvgNormalizer normalizer;
            auto display = winrt::Folia::BuildDisplayInlineText(block.inline_items, {},
                {block.id, source.size() - 1, folia::TextAffinity::Downstream},
                math, normalizer, {}, 18, 1000, true, false, false);
            log("display projection");
            expect(display.displayToSource.size() == display.text.size() + 1);
            expect(display.displayToSource.storage_bytes() < 4096_u);
            expect(display.ranges.size() == 1_u);
            auto sheet = winrt::Folia::CreateEditorStyleSheet(context.theme);
            winrt::Folia::EditorRenderResources resources;
            resources.InitializeOffscreen(sheet, 1200, 800);
            winrt::Folia::EditorTextLayoutEngine engine(resources, sheet);
            auto layout = engine.CreateFlow(display, resources.textFormat.Get(), 1000, {});
            expect(layout != nullptr);
            expect(engine.MeasureHeight(layout.Get(), 24) > 24);
            log("text layout");
        };
    };
}
