#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorDocumentBlockPreparer.h"
#include "editor/rendering/EditorDocumentRenderPass.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/EditorContentGeometry.h"

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;

    struct NativeInlineFrame {
        EditorStyleSheet sheet;
        EditorRenderResources resources;
        EditorRenderCache cache;
        MathJaxRenderer math;
        MermaidRenderer mermaid;
        SvgNormalizer normalizer;
        TreeSitterHighlighter syntax;
        EditorInteractionMap interaction;
        std::vector<D2D1_RECT_F> mathRects;
        std::vector<D2D1_COLOR_F> mathColors;
        ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> target;
        float width = EditorDocumentContentWidth(600, sheet.horizontalPadding);

        explicit NativeInlineFrame(folia::Theme theme = folia::Theme::Light)
            : sheet(CreateEditorStyleSheet(folia::default_theme_profile(theme))) {
            resources.InitializeOffscreen(sheet, 600, 400);
            resources.EnsureFrameResources(sheet);
            auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            winrt::check_hresult(resources.d2dContext->CreateBitmap({600, 400}, nullptr, 0, properties, target.GetAddressOf()));
        }

        std::vector<std::uint32_t> Paint(EditorSession& session,
            std::span<const winrt::Folia::detail::EditorSearchHighlight> highlights = {}, bool paintSvg = true,
            bool showSelection = false) {
            auto frame = session.RenderFrame();
            if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
            frame.searchHighlights = highlights;
            auto selection = showSelection ? frame.selection : folia::TextSelection{};
            interaction = {}; mathRects.clear(); mathColors.clear();
            std::vector<D2D1_RECT_F> nonInteractive;
            EditorTextLayoutEngine engine(resources, sheet);
            EditorSvgPainter svg(resources, cache);
            EditorInlineImageRenderer images(resources, cache, sheet, normalizer, svg, frame.baseDirectory);
            EditorDocumentPainter painter(resources, sheet, interaction, syntax, {}, false, 20 + width,
                selection, highlights, frame.renderModel.editable_index);
            EditorDocumentBlockPreparer preparer(frame, resources, cache, sheet, engine, images,
                painter, math, mermaid, normalizer, syntax, selection.active, width, true, true, false, 0, 0);
            EditorPreparedDocument document;
            for (auto const& block : frame.renderModel.blocks) document.blocks.push_back(preparer.Prepare(block, true, true));
            math.WaitForIdle();
            std::vector<EditorBlockGeometryIndex::Entry> geometry;
            for (std::size_t i = 0; i < document.blocks.size(); ++i) {
                document.blocks[i] = preparer.Prepare(frame.renderModel.blocks[i], true, true);
                geometry.push_back({0, document.blocks[i].height, 8});
            }
            document.geometry.Reset(std::move(geometry), 20);
            std::vector<std::uint64_t> visibleIds;
            for (auto const& block : document.blocks) {
                for (auto const& overlay : block.display.mathOverlays) visibleIds.push_back(overlay.fragment.renderId);
                if (block.table) for (auto const& display : block.table->displays)
                    for (auto const& overlay : display.mathOverlays) visibleIds.push_back(overlay.fragment.renderId);
            }
            math.SetVisibleSvgIds(visibleIds);
            math.WaitForIdle();
            EditorDocumentRenderPass pass(resources, sheet, interaction, nonInteractive, images, painter,
                [&](auto const& fragment, auto origin, auto color) {
                    mathRects.push_back({origin.x, origin.y, origin.x + fragment.width, origin.y + fragment.height});
                    mathColors.push_back(color);
                    if (!paintSvg) return true;
                    auto payload = fragment.svg ? fragment.svg : math.GetSvg(fragment.renderId);
                    return payload && svg.Draw(fragment.renderId, *payload, fragment.width, fragment.height, origin);
                }, [&](auto, auto) { expect(false) << "native formula unexpectedly fell back"; },
                [](auto const&, float, float, auto) { return true; });
            resources.d2dContext->SetTarget(target.Get());
            resources.d2dContext->BeginDraw();
            resources.d2dContext->Clear(sheet.canvasColor);
            pass.Paint(frame, document, selection, selection.active, 20, 20 + width, width, 0, 0, 0,
                0, document.blocks.size(), false, {}, {}, {});
            winrt::check_hresult(resources.d2dContext->EndDraw());
            resources.d2dContext->SetTarget(nullptr);
            auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> readback;
            winrt::check_hresult(resources.d2dContext->CreateBitmap({600, 400}, nullptr, 0, properties, readback.GetAddressOf()));
            winrt::check_hresult(readback->CopyFromBitmap(nullptr, target.Get(), nullptr));
            D2D1_MAPPED_RECT pixels{};
            winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ, &pixels));
            std::vector<std::uint32_t> result(600 * 400);
            for (std::size_t y = 0; y < 400; ++y) std::memcpy(result.data() + y * 600, pixels.bits + y * pixels.pitch, 600 * 4);
            readback->Unmap();
            return result;
        }


    };

    suite inline_presentation_paths = [] {
        "production flow and table paint search hits without changing the document"_test = [] {
            for (auto table : {false, true}) {
                EditorSession session;
                session.SetText(table ? L"| head |\n| --- |\n| needle |\n" : L"needle\n");
                auto original = session.Text();
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                auto owner = table ? frame.renderModel.blocks.front().special().table_cell_spans.back().container_id
                    : frame.renderModel.blocks.front().id;
                NativeInlineFrame native;
                auto plain = native.Paint(session);
                std::array<winrt::Folia::detail::EditorSearchHighlight, 1> hits{{{{owner, {0, 6}}, true}}};
                auto marked = native.Paint(session, hits);
                auto changed = std::inner_product(plain.begin(), plain.end(), marked.begin(), std::size_t{0},
                    std::plus<>{}, [](auto a, auto b) -> std::size_t { return a != b; });
                expect(changed > 10_u) << (table ? "table search highlight missing" : "flow search highlight missing");
                expect(session.Text() == original);
            }
        };

        "production flow and table math have one centered deletion line"_test = [] {
            for (auto table : {false, true}) {
              for (auto tex : {L"x", L"x+1", LR"(\frac{g_j}{1+x})"}) {
                EditorSession session;
                auto body = std::wstring{L"~~$"} + tex + L"$~~";
                session.SetText(winrt::hstring{table ? L"| head |\n| --- |\n| " + body + L" |\n" : body + L"\n"});
                NativeInlineFrame native;
                auto pixels = native.Paint(session, {}, false);
                expect(!native.mathRects.empty());
                if (native.mathRects.empty()) continue;
                auto rect = native.mathRects.front();
                for (auto const& next : native.mathRects) {
                    rect.left = (std::min)(rect.left, next.left); rect.right = (std::max)(rect.right, next.right);
                    rect.top = (std::min)(rect.top, next.top); rect.bottom = (std::max)(rect.bottom, next.bottom);
                }
                auto expectedY = rect.top + (rect.bottom - rect.top) * .52f;
                std::vector<int> rows;
                for (int y = static_cast<int>(rect.top); y < std::ceil(rect.bottom); ++y) {
                    std::size_t ink = 0;
                    for (int x = static_cast<int>(rect.left + 2); x < rect.right - 2; ++x) {
                        auto pixel = pixels[y * 600 + x];
                        if ((pixel & 0xff) < 150 && ((pixel >> 8) & 0xff) < 150) ++ink;
                    }
                    if (ink > 3) rows.push_back(y);
                }
                expect(!rows.empty());
                if (!rows.empty()) {
                    expect(rows.size() <= 3_u) << "more than one deletion band";
                    expect(std::abs((rows.front() + rows.back() + 1) * .5f - expectedY) < 1.f)
                        << (table ? "table" : "flow") << "deletion line does not follow the SVG bounds";
                }
              }
            }
        };

        "batch and visible math inherit the semantic inline foreground"_test = [] {
            for (auto table : {false, true}) {
                EditorSession session;
                session.SetText(table ? L"| head |\n| --- |\n| <span style=\"color:#cc3355\">text $x+1$</span> |\n"
                    : L"<span style=\"color:#cc3355\">text $x+1$</span>\n");
                auto frame = session.RenderFrame();
                auto theme = folia::default_theme_profile(folia::Theme::Light);
                EditorMathPreparationContext context{theme, 1, 600, 400, true};
                auto prepared = EditorSurfaceRenderer::PrepareMath(frame, context, {}, {});
                auto sheet = CreateEditorStyleSheet(theme);
                auto width = EditorDocumentContentWidth(600, sheet.horizontalPadding);
                if (table) width = EditorTableCellContentWidth(width, 1);
                auto colored = prepared->math.Lookup("x+1", false, sheet.body.size, width, "#CC3355");
                expect(static_cast<bool>(colored)) << "batch cache did not use the inherited foreground";
                NativeInlineFrame native;
                native.math = std::move(prepared->math);
                auto rendered = native.math.Snapshot().nativeRenders;
                native.Paint(session);
                expect(native.math.Snapshot().nativeRenders == rendered) << "visible math must reuse the batch request key";
                expect(!native.mathColors.empty());
                if (!native.mathColors.empty()) {
                    auto color = native.mathColors.front();
                    expect(std::abs(color.r - .8f) < .001f && std::abs(color.g - .2f) < .001f
                        && std::abs(color.b - 1.f / 3) < .001f) << "visible paint lost the inherited foreground";
                }
            }
        };
    };
}
