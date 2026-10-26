#include "pch.h"
#include "support/folia_test.hpp"
#include "media/MathJaxRenderer.h"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorContentGeometry.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    suite math_preparation_tests = []
    {
        "math opening batches span sparse blocks without retaining SVG"_test = []
        {
            for (bool sparse : {false, true})
            {
                EditorSession session;
                std::wstring source;
                for (int i = 0; i < 65; ++i)
                {
                    source += L"Formula $x+" + std::to_wstring(i) + L"$\n\n";
                    if (sparse) source += L"Plain text.\n\nMore text.\n\n";
                }
                session.SetText(winrt::hstring(source));
                const auto original = session.TextUtf8();
                EditorMathPreparationContext context{folia::default_theme_profile(), 1, 900, 600, true};
                auto prepared = EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {});
                auto stats = prepared->math.Snapshot();
                expect(prepared->formulaCount == 65_u);
                expect(stats.nativeBatches == 3_u);
                expect(stats.entries == 65_u && stats.nativeRenders == 65_u);
                expect(stats.pending == 0_u && stats.residentSvgBytes == 0_u);
                expect(stats.fileReads == 0_u && stats.fileWrites > 0_u);
                expect(session.TextUtf8() == original);
                auto sheet = CreateEditorStyleSheet(context.theme);
                auto width = EditorDocumentContentWidth(900, sheet.horizontalPadding);
                auto color = SvgColor(sheet.textColor);
                // Every owner survives a batch crossing model-walk boundaries.
                // Geometry lookup itself must not issue a disk read.
                for (int i = 0; i < 65; ++i)
                {
                    auto formula = "x+" + std::to_string(i);
                    auto geometry = prepared->math.Lookup(formula, false, sheet.body.size, width, color);
                    expect(geometry != nullptr);
                    if (geometry)
                    {
                        expect(static_cast<bool>(*geometry));
                        for (auto const& fragment : geometry->fragments) expect(!fragment.svg);
                    }
                }
                expect(prepared->math.Snapshot().fileReads == 0_u);
            }
        };
        "math opening reuses duplicates and cancels staged requests"_test = []
        {
            EditorMathPreparationContext context{folia::default_theme_profile(), 1, 900, 600, true};
            EditorSession session;
            std::wstring source;
            for (int i = 0; i < 96; ++i) source += L"Same $x+1$ and $y+2$.\n\n";
            session.SetText(winrt::hstring(source));
            auto prepared = EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {});
            auto stats = prepared->math.Snapshot();
            expect(prepared->formulaCount == 192_u);
            expect(stats.entries == 2_u && stats.nativeRenders == 2_u);
            expect(stats.nativeBatches == 1_u && stats.pending == 0_u);
            expect(stats.residentSvgBytes == 0_u);
            std::stop_source stop;
            bool canceled = false;
            try
            {
                (void)EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, stop.get_token(),
                    [&](std::size_t completed, std::size_t) { if (completed == 17) stop.request_stop(); });
            }
            catch (winrt::hresult_canceled const&) { canceled = true; }
            expect(canceled);
            // A canceled preparation must not affect a later request/session.
            auto recovered = EditorSurfaceRenderer::PrepareMath(session.RenderFrame(), context, {}, {});
            expect(recovered->formulaCount == 192_u);
            expect(recovered->math.Snapshot().pending == 0_u);
        };
    };
}
