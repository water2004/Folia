#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/rendering/EditorMathPreview.h"
#include "editor/rendering/EditorTextLayoutEngine.h"
#include "editor/rendering/EditorSvgPainter.h"

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;
    void SaveBitmap(EditorRenderResources& resources, ID2D1Bitmap1* target, std::filesystem::path const& path) {
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
        auto result = frame->WritePixels(size.height, mapped.pitch, mapped.pitch * size.height, mapped.bits);
        readback->Unmap();
        winrt::check_hresult(result);
        winrt::check_hresult(frame->Commit());
        winrt::check_hresult(encoder->Commit());
    }

    suite math_preview_appearance = [] {
        "math floating preview paints real native SVG in light and dark themes"_test = [] {
            for (auto theme : {folia::Theme::Light, folia::Theme::Dark}) {
                auto sheet = CreateEditorStyleSheet(folia::default_theme_profile(theme));
                EditorRenderResources resources;
                resources.InitializeOffscreen(sheet, 600, 280);
                resources.EnsureFrameResources(sheet);
                MathJaxRenderer math;
                constexpr auto tex = R"(x=\frac{-b\pm\sqrt{b^2-4ac}}{2a})";
                auto color = SvgColor(sheet.textColor);
                math.Render(tex, false, sheet.body.size, 560, color, {}, MathRenderPurpose::Preview);
                math.WaitForIdle();
                auto statistics = math.Snapshot();
                expect(statistics.fileReads == 0_u && statistics.fileWrites == 0_u);
                expect(statistics.previewSvgBytes > 0_u);
                auto svg = math.Lookup(tex, false, sheet.body.size, 560, color, MathRenderPurpose::Preview);
                expect(static_cast<bool>(svg));
                if (!svg) return;
                expect(svg->errorKind == MathJaxErrorKind::None);
                DisplayInlineText::MathPreview input{*svg, {}, false};
                auto display = BuildMathPreviewText(input);
                EditorTextLayoutEngine engine(resources, sheet);
                auto layout = engine.CreateFlow(display, resources.textFormat.Get(), 560, {});
                EditorPreparedMathPreview preview{std::move(display), layout, engine.MeasureHeight(layout.Get(), sheet.body.lineHeight), {}, true};
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
                auto line = engine.Create(L"Before $x=\\frac{-b\\pm\\sqrt{b^2-4ac}}{2a}$ after.", resources.codeFormat.Get(), 560);
                context->DrawTextLayout({20, 150}, line.Get(), resources.textBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
                auto following = engine.Create(L"Following paragraph stays in place.", resources.textFormat.Get(), 560);
                context->DrawTextLayout({20, 220}, following.Get(), resources.textBrush.Get());
                int fragments = 0;
                auto bounds = PaintInlineMathPreview(preview, D2D1::RectF(180, 150, 181, 178), 100, resources, sheet,
                    [&](auto const& fragment, auto origin, auto) {
                        auto payload = fragment.svg ? fragment.svg : math.GetSvg(fragment.renderId);
                        ++fragments;
                        return payload && painter.Draw(fragment.renderId, *payload, fragment.width, fragment.height, origin);
                    }, [&](auto, auto) { expect(false) << "native preview unexpectedly fell back"; });
                expect(bounds.has_value());
                expect(fragments > 0_i);
                expect(bounds && bounds->bottom < 150);
                expect(resources.previewBackgroundBrush->GetColor().a == 1.f);
                winrt::check_hresult(context->EndDraw());
                context->SetTransform(D2D1::Matrix3x2F::Identity());
                context->SetTarget(nullptr);
                auto directory = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / L"build/tests/inline-math-preview";
                std::filesystem::create_directories(directory);
                SaveBitmap(resources, target.Get(), directory / (theme == folia::Theme::Light ? L"light.png" : L"dark.png"));
            }
        };
    };
}
