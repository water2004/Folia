#include "pch.h"
#include "support/folia_test.hpp"
#include "support/MathEditorExamples.h"
#include "editor/rendering/EditorMathPreview.h"
#include "editor/rendering/EditorTextLayoutEngine.h"
#include "editor/rendering/EditorSvgPainter.h"

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;
    void SaveBitmap(EditorRenderResources& resources, ID2D1Bitmap1* target, std::filesystem::path const& path,
                    std::optional<D2D1_RECT_F> previewBounds = {}) {
        auto size = target->GetPixelSize();
        auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> readback;
        winrt::check_hresult(resources.d2dContext->CreateBitmap(size, nullptr, 0, properties, readback.GetAddressOf()));
        winrt::check_hresult(readback->CopyFromBitmap(nullptr, target, nullptr));
        ::Microsoft::WRL::ComPtr<IWICStream> stream;
        winrt::check_hresult(resources.wicFactory->CreateStream(stream.GetAddressOf()));
        winrt::check_hresult(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE));
        ::Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
        winrt::check_hresult(resources.wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf()));
        winrt::check_hresult(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
        ::Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        winrt::check_hresult(encoder->CreateNewFrame(frame.GetAddressOf(), nullptr));
        winrt::check_hresult(frame->Initialize(nullptr));
        winrt::check_hresult(frame->SetSize(size.width, size.height));
        auto format = GUID_WICPixelFormat32bppBGRA;
        winrt::check_hresult(frame->SetPixelFormat(&format));
        D2D1_MAPPED_RECT mapped{};
        winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ, &mapped));
        if (previewBounds) {
            auto pixelsPerDip = size.width / resources.surfaceWidthDip;
            std::size_t outside = 0;
            // The editable source begins at y=150. Above it, all dark ink
            // belongs to the preview and must stay inside the declared card.
            for (UINT y = 0; y < (std::min)(size.height, static_cast<UINT>(150 * pixelsPerDip)); ++y)
                for (UINT x = 0; x < size.width; ++x) {
                    auto pixel = mapped.bits + y * mapped.pitch + x * 4;
                    if (pixel[0] > 80 || pixel[1] > 80 || pixel[2] > 80 || pixel[3] < 240) continue;
                    auto px = x / pixelsPerDip, py = y / pixelsPerDip;
                    auto const& bounds = *previewBounds;
                    if (px < bounds.left || px > bounds.right || py < bounds.top || py > bounds.bottom) ++outside;
                }
            expect(outside == 0_u) << "SVG ink extends outside its preview card: " << outside;
        }
        auto result = frame->WritePixels(size.height, mapped.pitch, mapped.pitch * size.height, mapped.bits);
        readback->Unmap();
        winrt::check_hresult(result);
        winrt::check_hresult(frame->Commit());
        winrt::check_hresult(encoder->Commit());
    }

    suite math_preview_appearance = [] {
        "math floating previews use the window width and retain native inline size"_test = [] {
            auto sheet = CreateEditorStyleSheet(folia::default_theme_profile(folia::Theme::Light));
            EditorRenderResources resources;
            resources.InitializeOffscreen(sheet, 1200, 800);
            resources.EnsureFrameResources(sheet);
            MathJaxRenderer math;
            auto color = SvgColor(sheet.textColor);
            math.Render(math_editor_examples::longChain, false, sheet.body.size, 400, color, {}, MathRenderPurpose::Preview);
            math.WaitForIdle();
            auto svg = math.Lookup(math_editor_examples::longChain, false, sheet.body.size, 400, color, MathRenderPurpose::Preview);
            expect(static_cast<bool>(svg));
            if (!svg) return;
            DisplayInlineText::MathPreview input{*svg, {}, false, sheet.textColor};
            EditorTextLayoutEngine engine(resources, sheet);
            auto preview = PrepareMathPreview(input, engine, resources.textFormat.Get(), 400, 1200, sheet.mathPreview);
            expect(preview.layout->GetMaxWidth() == 1152.f);
            expect(preview.Width() > 640.f);
            expect(preview.display.mathOverlays.size() == svg->fragments.size());
            for (std::size_t i = 0; i < svg->fragments.size(); ++i) {
                expect(preview.display.mathOverlays[i].fragment.width == svg->fragments[i].width);
                expect(preview.display.mathOverlays[i].fragment.height == svg->fragments[i].height);
            }
            auto placement = PlaceInlineMathPreview({500, 600, 501, 630}, 500,
                {preview.Width(), preview.Height()}, {1200, 800}, sheet.mathPreview);
            expect(placement.has_value());
            if (placement) expect(placement->bounds.right - placement->bounds.left > 640.f);
        };

        "math previews frame native SVG evenly for inline and block formulas"_test = [] {
            for (auto theme : {folia::Theme::Light, folia::Theme::Dark}) {
              for (auto block : {false, true})
              for (auto width : {120.f, 560.f, 1200.f})
              for (auto tex : {R"(1111)", R"(g_j)", R"(\sqrt{x})", R"(x=\frac{-b\pm\sqrt{b^2-4ac}}{2a})", R"(a+b+c+d+e+f+g+h)", math_editor_examples::longChain}) {
                auto sheet = CreateEditorStyleSheet(folia::default_theme_profile(theme));
                EditorRenderResources resources;
                resources.InitializeOffscreen(sheet, 600, 280);
                resources.EnsureFrameResources(sheet);
                MathJaxRenderer math;
                auto color = SvgColor(sheet.textColor);
                math.Render(tex, block, sheet.body.size, width, color, {}, MathRenderPurpose::Preview);
                math.WaitForIdle();
                auto statistics = math.Snapshot();
                expect(statistics.fileReads == 0_u && statistics.fileWrites == 0_u);
                expect(statistics.previewSvgBytes > 0_u);
                auto svg = math.Lookup(tex, block, sheet.body.size, width, color, MathRenderPurpose::Preview);
                expect(static_cast<bool>(svg));
                if (!svg) return;
                expect(svg->errorKind == MathJaxErrorKind::None);
                DisplayInlineText::MathPreview input{*svg, {}, false, sheet.textColor};
                EditorTextLayoutEngine engine(resources, sheet);
                auto preview = PrepareMathPreview(input, engine, resources.textFormat.Get(), width,
                    resources.surfaceWidthDip, sheet.mathPreview);
                expect(preview.floating == !block);
                EditorRenderCache cache;
                EditorSvgPainter painter(resources, cache);
                expect(painter.Supported());
                auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
                ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> target;
                winrt::check_hresult(resources.d2dContext->CreateBitmap({1200, 560}, nullptr, 0, properties, target.GetAddressOf()));
                auto context = resources.d2dContext;
                context->SetTarget(target.Get());
                context->SetTransform(D2D1::Matrix3x2F::Scale(2, 2));
                context->BeginDraw();
                context->Clear(sheet.canvasColor);
                auto source = std::string(block ? "$$" : "$") + tex + (block ? "$$" : "$");
                auto sourceWide = winrt::to_hstring(source);
                auto line = engine.Create(std::wstring{sourceWide.c_str()}, resources.codeFormat.Get(), 560);
                context->DrawTextLayout({20, 150}, line.Get(), resources.textBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
                auto following = engine.Create(L"Following paragraph stays in place.", resources.textFormat.Get(), 560);
                context->DrawTextLayout({20, 220}, following.Get(), resources.textBrush.Get());
                int fragments = 0;
                auto ink = D2D1::RectF(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
                EditorDrawMath draw = [&](auto const& fragment, auto origin, auto, float scale) {
                        expect(scale == 1.f) << "editing previews must retain native formula size";
                        auto payload = fragment.svg ? fragment.svg : math.GetSvg(fragment.renderId);
                        ++fragments;
                        ink.left = (std::min)(ink.left, origin.x);
                        ink.top = (std::min)(ink.top, origin.y);
                        ink.right = (std::max)(ink.right, origin.x + fragment.width * scale);
                        ink.bottom = (std::max)(ink.bottom, origin.y + fragment.height * scale);
                        return payload && painter.Draw(fragment.renderId, *payload,
                            fragment.width * scale, fragment.height * scale, origin, fragment.width, fragment.height);
                    };
                EditorDrawMathFallback fallback = [&](auto, auto) { expect(false) << "native preview unexpectedly fell back"; };
                std::optional<D2D1_RECT_F> bounds;
                if (block) {
                    bounds = D2D1::RectF(20, 20, 580, 20 + preview.Height() + 16);
                    context->FillRectangle(*bounds, resources.nestedQuoteBrush.Get());
                    PaintMathPreviewContent(preview, {(600 - preview.Width()) * .5f, 28},
                        resources, sheet, draw, fallback);
                } else bounds = PaintInlineMathPreview(preview, D2D1::RectF(180, 150, 181, 178), 100,
                    resources, sheet, draw, fallback);
                expect(bounds.has_value());
                expect(fragments > 0_i);
                if (!block) expect(bounds && (bounds->bottom < 150 || bounds->top > 178));
                if (bounds && (block || preview.Height() + 20 <= 130)) {
                    expect(std::abs((ink.top - bounds->top) - (bounds->bottom - ink.bottom)) < .1f)
                        << tex << " SVG vertical padding: top=" << ink.top - bounds->top
                        << " bottom=" << bounds->bottom - ink.bottom;
                    expect(std::abs((ink.left - bounds->left) - (bounds->right - ink.right)) < .1f)
                        << tex << " SVG horizontal padding";
                }
                expect(resources.previewBackgroundBrush->GetColor().a == 1.f);
                winrt::check_hresult(context->EndDraw());
                context->SetTransform(D2D1::Matrix3x2F::Identity());
                context->SetTarget(nullptr);
                auto directory = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / L"build/tests/inline-math-preview";
                std::filesystem::create_directories(directory);
                auto name = std::string(tex).starts_with("0<") ? "long-chain" : std::string(tex) == "1111" ? "digits" : std::string(tex) == "g_j" ? "subscript"
                    : std::string(tex) == R"(\sqrt{x})" ? "root" : std::string(tex).starts_with("a+") ? "wrapped" : "fraction";
                SaveBitmap(resources, target.Get(), directory / (std::string(name) + (block ? "-block" : "-inline")
                    + (width < 560 ? "-narrow" : width > 560 ? "-large" : "-wide") + (theme == folia::Theme::Light ? "-light.png" : "-dark.png")),
                    !block && theme == folia::Theme::Light ? bounds : std::nullopt);
              }
            }
        };
    };
}
