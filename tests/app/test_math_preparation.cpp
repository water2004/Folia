#include "pch.h"
#include "support/folia_test.hpp"
#include "windows/media/math/MathJaxRenderer.h"
#include "windows/media/math/NativeMathSession.h"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/layout/EditorContentPreparation.h"
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
        "math reflow reuses ordinary SVGs at different widths"_test = []
        {
            using namespace folia::platform;
            for (auto purpose : {MathRenderPurpose::Document, MathRenderPurpose::Preview})
            for (bool display : {false, true})
            {
                MathJaxRenderer renderer;
                auto layoutAt = [&](float width) {
                    renderer.BeginBlock(1);
                    auto result = renderer.Layout(R"(\frac{x+1}{y})", display, 18, width, "#222222", true, purpose);
                    renderer.EndBlock();
                    return result;
                };
                layoutAt(900);
                renderer.WaitForIdle();
                auto initial = layoutAt(900);
                expect(fatal(initial.svg && initial.svg->reusableWidthFrom && !initial.pending));
                std::vector<std::uint64_t> visible;
                for (auto const& fragment : initial.svg->fragments) visible.push_back(fragment.renderId);
                renderer.SetVisibleSvgIds(visible);
                renderer.WaitForIdle();
                const auto before = renderer.Snapshot();
                for (auto width : {640.f, 480.f, 820.f, 950.f})
                {
                    auto resized = layoutAt(width);
                    expect(resized.svg == initial.svg);
                    expect(!resized.pending);
                    for (auto const& fragment : initial.svg->fragments)
                        expect(bool(renderer.GetSvg(fragment.renderId)));
                    renderer.SetVisibleSvgIds(visible);
                }
                auto after = renderer.Snapshot();
                expect(after.entries == 1_u && after.nativeRenders == before.nativeRenders);
                expect(after.fileWrites == before.fileWrites && after.fileReads == before.fileReads);
            }
        };
        "math reflow keeps a drawable tagged formula through native layout and disk reads"_test = []
        {
            using namespace folia::platform;
            MathJaxRenderer renderer;
            static constexpr std::string_view tex = R"(a+b=c\tag{4.9})";
            auto layoutAt = [&](float width) {
                renderer.BeginBlock(1);
                auto result = renderer.Layout(tex, true, 18, width, "#222222", true, MathRenderPurpose::Document);
                renderer.EndBlock();
                return result;
            };
            auto draw = [&](MathJaxLayoutResult const& result) {
                expect(fatal(result.svg && bool(*result.svg)));
                std::vector<std::uint64_t> visible;
                for (auto const& fragment : result.svg->fragments)
                {
                    expect(bool(renderer.GetSvg(fragment.renderId)));
                    visible.push_back(fragment.renderId);
                }
                renderer.SetVisibleSvgIds(visible);
            };
            layoutAt(900);
            renderer.WaitForIdle();
            auto wide = layoutAt(900);
            expect(fatal(wide.svg && !wide.svg->reusableWidthFrom && !wide.pending));
            std::vector<std::uint64_t> initialIds;
            for (auto const& fragment : wide.svg->fragments) initialIds.push_back(fragment.renderId);
            renderer.SetVisibleSvgIds(initialIds);
            renderer.WaitForIdle();
            draw(wide);

            draw(layoutAt(600));
            renderer.WaitForIdle();
            auto narrow = layoutAt(600);
            draw(narrow);
            expect(!narrow.pending && narrow.svg != wide.svg);
            expect(narrow.svg->width < wide.svg->width);
            expect(renderer.Snapshot().entries == 1_u);

            // Premeasure the next width independently, retaining only geometry
            // on disk, just as the document-opening batch does.
            {
                renderer.BeginBlock(2);
                auto batch = renderer.BeginBatch();
                batch.Render(tex, true, 18, 720, "#222222");
                batch.Submit();
                renderer.EndBlock();
            }
            renderer.WaitForIdle();
            auto disk = renderer.Lookup(tex, true, 18, 720, "#222222");
            expect(fatal(disk && !disk->fragments.empty()));
            expect(!renderer.GetSvg(disk->fragments.front().renderId));
            const auto before = renderer.Snapshot();
            draw(layoutAt(720));
            renderer.WaitForIdle();
            auto restored = layoutAt(720);
            draw(restored);
            expect(!restored.pending && restored.svg == disk);
            expect(renderer.Snapshot().fileReads > before.fileReads);
        };
        "math reflow stops reusing unwrapped display math at its natural width"_test = []
        {
            folia::platform::NativeMathSession native;
            std::string tex = "x";
            for (int i = 0; i < 24; ++i) tex += "+x";
            auto wide = native.Render(tex, true, 18, 1200);
            auto narrow = native.Render(tex, true, 18, 160);
            expect(fatal(bool(wide) && bool(narrow) && wide.reusableWidthFrom.has_value()));
            expect(*wide.reusableWidthFrom > 160.0 && *wide.reusableWidthFrom <= 1200.0);
            expect(!narrow.reusableWidthFrom);
            expect(narrow.height > wide.height);
        };
    };
}
