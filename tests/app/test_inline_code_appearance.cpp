#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorTextLayoutEngine.h"
#include <dwrite_1.h>

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;

    std::size_t BackgroundPixels(EditorRenderResources& resources, ID2D1Bitmap1* target, D2D1_COLOR_F color) {
        auto size = target->GetPixelSize();
        auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> readback;
        winrt::check_hresult(resources.d2dContext->CreateBitmap(size, nullptr, 0, properties, readback.GetAddressOf()));
        winrt::check_hresult(readback->CopyFromBitmap(nullptr, target, nullptr));
        D2D1_MAPPED_RECT pixels{};
        winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ, &pixels));
        auto channel = [](float value) { return static_cast<int>(std::lround(value * 255)); };
        std::size_t matched = 0;
        for (UINT32 y = 0; y < size.height; ++y)
        for (UINT32 x = 0; x < size.width; ++x) {
            auto pixel = pixels.bits + y * pixels.pitch + x * 4;
            if (std::abs(pixel[0] - channel(color.b)) <= 1 && std::abs(pixel[1] - channel(color.g)) <= 1
                && std::abs(pixel[2] - channel(color.r)) <= 1 && pixel[3] == 255) ++matched;
        }
        readback->Unmap();
        return matched;
    }

    suite inline_code_appearance = [] {
        "code padding belongs to shaped boundary clusters and preserves source mapping"_test = [] {
            for (auto source : {L"甲`a\U0001f600e\u0301`乙\n", L"甲`a`**`b`**乙\n", L"` a\r\nb `\n"}) {
                EditorSession session;
                session.SetText(source);
                auto original = session.Text();
                auto frame = session.RenderFrame();
                if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                auto sheet = CreateEditorStyleSheet(folia::default_theme_profile(folia::Theme::Light));
                EditorRenderResources resources;
                resources.InitializeOffscreen(sheet, 600, 240);
                resources.EnsureFrameResources(sheet);
                EditorTextLayoutEngine engine(resources, sheet);
                MathJaxRenderer math; SvgNormalizer normalizer;
                auto display = BuildDisplayInlineText(frame.renderModel.blocks.front().inline_items, {}, {},
                    math, normalizer, sheet.textColor, sheet.body.size, 560, false, false, false);
                auto layout = engine.CreateFlow(display, resources.textFormat.Get(), 560, {});
                ::Microsoft::WRL::ComPtr<IDWriteTextLayout1> spacing;
                winrt::check_hresult(layout.As(&spacing));
                DWRITE_TEXT_METRICS padded{}, plain{};
                layout->GetMetrics(&padded);
                std::unordered_set<std::uint64_t> owners;
                float totalSpacing = 0;
                auto text = ToWide(display.text);
                for (UINT32 position = 0; position < text.size(); ++position) {
                    float leading = 0, trailing = 0, minimum = 0;
                    winrt::check_hresult(spacing->GetCharacterSpacing(position, &leading, &trailing, &minimum));
                    if (position > 0 && text[position] >= 0xdc00 && text[position] <= 0xdfff) {
                        float previousLeading = 0, previousTrailing = 0;
                        spacing->GetCharacterSpacing(position - 1, &previousLeading, &previousTrailing, &minimum);
                        expect(leading == previousLeading && trailing == previousTrailing);
                    }
                }
                UINT32 count = 0;
                layout->GetClusterMetrics(nullptr, 0, &count);
                std::vector<DWRITE_CLUSTER_METRICS> clusters(count);
                layout->GetClusterMetrics(clusters.data(), count, &count);
                UINT32 start = 0;
                for (auto const& cluster : clusters) {
                    float leading = 0, trailing = 0, minimum = 0;
                    spacing->GetCharacterSpacing(start, &leading, &trailing, &minimum);
                    totalSpacing += leading + trailing;
                    start += cluster.length;
                }
                for (auto const& range : display.ranges)
                    if (!range.marker && range.style.code == folia::CodeStyle::Inline) {
                        expect(range.style.code_span_id.v != 0_u);
                        owners.insert(range.style.code_span_id.v);
                    }
                expect(std::abs(totalSpacing - owners.size() * 6.f) < .01f);
                winrt::check_hresult(spacing->SetCharacterSpacing(0, 0, 0, {0, static_cast<UINT32>(text.size())}));
                layout->GetMetrics(&plain);
                expect(std::abs(padded.widthIncludingTrailingWhitespace - plain.widthIncludingTrailingWhitespace - totalSpacing) < .01f);
                expect(session.Text() == original);
                expect(display.displayToSource.size() == text.size() + 1);
            }
        };

        "inline code uses one themed background across flows and table cells"_test = [] {
            for (auto theme : {folia::Theme::Light, folia::Theme::Dark, folia::Theme::HighContrast}) {
                auto sheet = CreateEditorStyleSheet(folia::default_theme_profile(theme));
                EditorRenderResources resources;
                resources.InitializeOffscreen(sheet, 400, 260);
                resources.EnsureFrameResources(sheet);
                EditorTextLayoutEngine engine(resources, sheet);
                MathJaxRenderer math;
                SvgNormalizer normalizer;
                auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
                ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> target;
                winrt::check_hresult(resources.d2dContext->CreateBitmap({400, 260}, nullptr, 0, properties, target.GetAddressOf()));
                for (auto source : {L"`alpha beta`\n", L"# `alpha beta`\n", L"- `alpha beta`\n",
                    L"- [ ] `alpha beta`\n", L"> `alpha beta`\n", L"> [!NOTE]\n> `alpha beta`\n",
                    L"[^n]: `alpha beta`\n\nref[^n]\n", L"| head |\n| --- |\n| `alpha beta` |\n"}) {
                    EditorSession session;
                    session.SetText(source);
                    auto original = session.Text();
                    auto frame = session.RenderFrame();
                    if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
                    std::size_t examined = 0;
                    auto check = [&](auto const& items) {
                        if (std::none_of(items.begin(), items.end(), [](auto const& item) {
                            return item.style.code == folia::CodeStyle::Inline;
                        })) return;
                        ++examined;
                        for (auto width : {64.f, 320.f}) {
                            auto display = BuildDisplayInlineText(items, {}, {}, math, normalizer,
                                sheet.textColor, sheet.body.size, width, false, false, false);
                            auto layout = engine.CreateFlow(display, resources.textFormat.Get(), width, {});
                            for (auto const& range : display.ranges) {
                                if (range.marker || range.style.code != folia::CodeStyle::Inline) continue;
                                ::Microsoft::WRL::ComPtr<IUnknown> effect;
                                winrt::check_hresult(layout->GetDrawingEffect(range.start, effect.GetAddressOf()));
                                expect(effect.Get() == static_cast<IUnknown*>(resources.inlineCodeBrush.Get()));
                            }
                            resources.d2dContext->SetTarget(target.Get());
                            resources.d2dContext->BeginDraw();
                            resources.d2dContext->Clear(sheet.canvasColor);
                            DrawInlinePresentationBackgrounds(resources, sheet, layout.Get(), {20, 20}, display.ranges);
                            winrt::check_hresult(resources.d2dContext->EndDraw());
                            resources.d2dContext->SetTarget(nullptr);
                            expect(BackgroundPixels(resources, target.Get(), sheet.inlineCodeBackgroundColor) > 10_u)
                                << winrt::to_string(source) << width;
                        }
                    };
                    auto walk = [&](auto&& self, folia::RenderBlock const& block) -> void {
                        check(block.inline_items);
                        for (auto const& cell : block.special().table_cells) check(cell);
                        for (auto const& child : block.child_blocks) self(self, child);
                    };
                    for (auto const& block : frame.renderModel.blocks) walk(walk, block);
                    expect(examined > 0_u);
                    expect(session.Text() == original);
                }
                // An identical font run in block code (or a revealed marker)
                // must not be mistaken for a semantic inline code span.
                for (auto role : {folia::CodeStyle::None, folia::CodeStyle::Block, folia::CodeStyle::Inline}) {
                    auto style = folia::InlineStyle::plain(); style.code = role;
                    std::vector<InlineStyleRange> ranges{{0, 4, style, role == folia::CodeStyle::Inline}};
                    auto layout = engine.Create(L"code", resources.codeFormat.Get(), 300);
                    engine.ApplyStyles(layout.Get(), ranges);
                    resources.d2dContext->SetTarget(target.Get());
                    resources.d2dContext->BeginDraw();
                    resources.d2dContext->Clear(sheet.canvasColor);
                    DrawInlinePresentationBackgrounds(resources, sheet, layout.Get(), {20, 20}, ranges);
                    winrt::check_hresult(resources.d2dContext->EndDraw());
                    resources.d2dContext->SetTarget(nullptr);
                    expect(BackgroundPixels(resources, target.Get(), sheet.inlineCodeBackgroundColor) == 0_u);
                }
                resources.ResetBrushes();
                resources.EnsureFrameResources(sheet);
                expect(resources.inlineCodeBackgroundBrush != nullptr && resources.inlineCodeBrush != nullptr);
                expect(math.Snapshot().entries == 0_u);
            }
        };
    };
}
