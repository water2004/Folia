#include "pch.h"
#include "support/folia_test.hpp"
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorTextLayoutEngine.h"
#include "theme/ThemeConfig.h"

import folia.markdown.parser;
import folia.core.render_builder;

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    void SavePreview(EditorRenderResources& resources, ID2D1Bitmap1* bitmap, wchar_t const* path)
    {
        ::Microsoft::WRL::ComPtr<IWICStream> stream;
        winrt::check_hresult(resources.wicFactory->CreateStream(stream.GetAddressOf()));
        winrt::check_hresult(stream->InitializeFromFilename(path, GENERIC_WRITE));
        ::Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
        winrt::check_hresult(resources.wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf()));
        winrt::check_hresult(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
        ::Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        winrt::check_hresult(encoder->CreateNewFrame(frame.GetAddressOf(), nullptr));
        winrt::check_hresult(frame->Initialize(nullptr));
        auto size = bitmap->GetPixelSize();
        winrt::check_hresult(frame->SetSize(size.width, size.height));
        auto format = GUID_WICPixelFormat32bppBGRA;
        winrt::check_hresult(frame->SetPixelFormat(&format));
        D2D1_MAPPED_RECT mapped{};
        winrt::check_hresult(bitmap->Map(D2D1_MAP_OPTIONS_READ, &mapped));
        auto result = frame->WritePixels(size.height, mapped.pitch, mapped.pitch * size.height, mapped.bits);
        bitmap->Unmap();
        winrt::check_hresult(result);
        winrt::check_hresult(frame->Commit());
        winrt::check_hresult(encoder->Commit());
    }

    void SaveMathBlockPreview(EditorRenderResources& resources,
        std::shared_ptr<const folia::SyntaxMarkerTheme> const& assets, wchar_t const* path)
    {
        auto theme = folia::default_theme_profile(folia::Theme::Light);
        theme.syntax_markers = assets;
        auto sheet = CreateEditorStyleSheet(theme);
        resources.RebuildTextFormats(sheet);
        auto parsed = folia::parse_text(1, "$$\nr_0=\\frac{\\mu}{\\lambda}\n$$\n");
        auto model = folia::build_render_model(parsed.document, parsed.outline, parsed.symbols, theme);
        auto const& block = model.blocks.front();
        MathJaxRenderer math;
        SvgNormalizer normalizer;
        auto display = BuildMathBlockText(block, {block.id, 3, folia::TextAffinity::Downstream},
            math, normalizer, sheet.textColor, sheet.body.size, 320, false, false, false);
        EditorTextLayoutEngine engine(resources, sheet);
        auto layout = engine.CreateFlow(display, resources.textFormat.Get(), 320, {});
        auto context = resources.d2dContext.Get();
        auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> target, readback;
        winrt::check_hresult(context->CreateBitmap({720, 260}, nullptr, 0, properties, target.GetAddressOf()));
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
        winrt::check_hresult(context->CreateSolidColorBrush(sheet.textColor, brush.GetAddressOf()));
        context->SetTarget(target.Get());
        context->SetTransform(D2D1::Matrix3x2F::Scale(2, 2));
        context->BeginDraw();
        context->Clear(sheet.canvasColor);
        auto origin = D2D1::Point2F(16, 10);
        context->DrawTextLayout(origin, layout.Get(), brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        for (auto const& range : display.ranges)
            for (auto rect : SyntaxIconRectangles(layout.Get(), range, origin))
                resources.syntaxIcons.Draw(context, range.icon, rect, sheet.markerColor, assets);
        winrt::check_hresult(context->EndDraw());
        context->SetTransform(D2D1::Matrix3x2F::Identity());
        context->SetTarget(nullptr);
        properties.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        winrt::check_hresult(context->CreateBitmap({720, 260}, nullptr, 0, properties, readback.GetAddressOf()));
        winrt::check_hresult(readback->CopyFromBitmap(nullptr, target.Get(), nullptr));
        SavePreview(resources, readback.Get(), path);
    }

    suite syntax_icons_tests = [] {
        "syntax icons are indivisible font independent layout objects"_test = [] {
            EditorRenderResources resources;
            winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(resources.dwriteFactory.GetAddressOf())));
            for (auto family : {L"Segoe UI", L"Consolas", L"Times New Roman"})
            for (auto size : {12.f, 16.f, 32.f})
            for (auto const& example : {std::pair{U"*", SyntaxIcon::Star}, {U"**", SyntaxIcon::DoubleStar},
                {U"_", SyntaxIcon::Underscore}, {U"__", SyntaxIcon::DoubleUnderscore},
                {U"~~", SyntaxIcon::DoubleTilde}, {U"`", SyntaxIcon::Backtick}, {U"``", SyntaxIcon::DoubleBacktick},
                {U"```", SyntaxIcon::TripleBacktick}, {U"$", SyntaxIcon::Dollar}, {U"$$", SyntaxIcon::DoubleDollar},
                {U"\\(", SyntaxIcon::MathOpen}, {U"\\)", SyntaxIcon::MathClose},
                {U"\\[", SyntaxIcon::MathBracketOpen}, {U"\\]", SyntaxIcon::MathBracketClose}})
            for (auto width : {45.f, 120.f, 1000.f})
            {
                auto token = example.first;
                auto icon = example.second;
                EditorStyleSheet sheet;
                sheet.body = {family, size, size * 1.5f};
                sheet.code = sheet.body;
                ::Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
                winrt::check_hresult(resources.dwriteFactory->CreateTextFormat(family, nullptr,
                    DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    size, L"en-us", format.GetAddressOf()));
                DisplayInlineText display;
                auto length = std::char_traits<char32_t>::length(token);
                AppendSourceText(display, token, {folia::NodeId{1}, {0, length}}, {}, true);
                AppendSourceText(display, U"正文 text", {folia::NodeId{1}, {length, length + 7}}, {}, false);
                AppendSourceText(display, token, {folia::NodeId{1}, {length + 7, 2 * length + 7}}, {}, true);
                // Geometry fixture receives the same explicit semantic identity
                // as the render model; source text alone must not choose an icon.
                display.ranges.front().icon = display.ranges.back().icon = icon;
                EditorTextLayoutEngine engine(resources, sheet);
                auto layout = engine.CreateFlow(display, format.Get(), width, {});
                for (auto const& range : display.ranges)
                {
                    if (!range.marker) { expect(range.icon == SyntaxIcon::None); continue; }
                    expect(range.icon != SyntaxIcon::None);
                    auto rectangles = SyntaxIconRectangles(layout.Get(), range, {});
                    expect(rectangles.size() == 1_u) << "a marker must not split across lines" << size << width;
                    if (rectangles.size() != 1) continue;
                    auto rect = rectangles.front();
                    expect(std::abs(rect.right - rect.left - size * sheet.marker.sizeEm) < .01f);
                    expect(std::abs(rect.bottom - rect.top - size * sheet.marker.sizeEm) < .01f);
                    ::Microsoft::WRL::ComPtr<IDWriteInlineObject> whole;
                    winrt::check_hresult(layout->GetInlineObject(range.start, whole.GetAddressOf()));
                    for (UINT32 i = range.start; i < range.start + range.length; ++i)
                    {
                        FLOAT x = 0, y = 0;
                        DWRITE_HIT_TEST_METRICS caret{}, back{};
                        winrt::check_hresult(layout->HitTestTextPosition(i, false, &x, &y, &caret));
                        BOOL trailing = false, inside = false;
                        winrt::check_hresult(layout->HitTestPoint(x + .1f, y + caret.height * .5f, &trailing, &inside, &back));
                        expect(back.textPosition == range.start);
                        expect(back.length == range.length);
                        expect(!trailing);
                        expect(display.displayToSource[i].source_offset == i);
                        ::Microsoft::WRL::ComPtr<IDWriteInlineObject> object;
                        winrt::check_hresult(layout->GetInlineObject(i, object.GetAddressOf()));
                        expect(object == whole);
                    }
                }
            }
            DisplayInlineText raw;
            AppendSourceText(raw, U"*", {folia::NodeId{1}, {0, 1}}, {}, true);
            expect(raw.ranges.front().icon == SyntaxIcon::None);
        };

        "syntax icon theme SVG resources render in both variants and survive device resource reset"_test = [] {
          try {
            EditorStyleSheet sheet;
            sheet.body = {L"Segoe UI", 16, 24};
            sheet.heading1 = sheet.heading2 = sheet.heading3 = sheet.code = sheet.body;
            EditorRenderResources resources;
            auto assets = DefaultSyntaxMarkerTheme();
            resources.InitializeOffscreen(sheet, 1000, 560);
            auto context = resources.d2dContext.Get();
            ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> target, readback;
            auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            winrt::check_hresult(context->CreateBitmap({1000, 560}, nullptr, 0, properties, target.GetAddressOf()));
            context->SetTarget(target.Get());
            context->BeginDraw();
            auto resourceCheck = FindResourceW(nullptr, L"FOLIA_MARKER_STAR", RT_RCDATA);
            expect(resourceCheck != nullptr);
            if (!resourceCheck) { context->EndDraw(); return; }
            context->Clear(D2D1::ColorF(0xf8fafc));
            ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
            winrt::check_hresult(context->CreateSolidColorBrush(D2D1::ColorF(0x1e2430), brush.GetAddressOf()));
            context->FillRectangle({0, 280, 1000, 560}, brush.Get());
            for (int theme = 0; theme < 2; ++theme)
            {
                resources.syntaxIcons.Reset();
                auto color = D2D1::ColorF(theme ? 0xaebdd1 : 0x64748b);
                brush->SetColor(color);
                auto label = theme ? L"DARK / 16, 20, 32 DIP" : L"LIGHT / 16, 20, 32 DIP";
                context->DrawText(label, static_cast<UINT32>(std::wcslen(label)), resources.textFormat.Get(),
                    {28.f, theme * 280.f + 18, 900, theme * 280.f + 44}, brush.Get());
                for (std::size_t i = 1; i < static_cast<std::size_t>(SyntaxIcon::Count); ++i)
                {
                    float x = 28.f + (i - 1) * 60.f;
                    float y = theme * 280.f + 70;
                    for (float size : {16.f, 20.f, 32.f})
                    {
                        resources.syntaxIcons.Draw(context, static_cast<SyntaxIcon>(i), {x, y, x + size, y + size}, color, assets);
                        y += 56;
                    }
                }
            }
            winrt::check_hresult(context->EndDraw());
            context->SetTarget(nullptr);
            properties.bitmapOptions = D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            winrt::check_hresult(context->CreateBitmap({1000, 560}, nullptr, 0, properties, readback.GetAddressOf()));
            winrt::check_hresult(readback->CopyFromBitmap(nullptr, target.Get(), nullptr));
            D2D1_MAPPED_RECT mapped{};
            winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ, &mapped));
            // Every tile must contain visible SVG strokes; a successful EndDraw
            // alone does not detect missing assets or unsupported currentColor.
            for (int theme = 0; theme < 2; ++theme)
            for (std::size_t i = 1; i < static_cast<std::size_t>(SyntaxIcon::Count); ++i)
            {
                auto x = 28 + static_cast<int>(i - 1) * 60;
                auto y = theme * 280 + 70;
                int ink = 0;
                for (int row = y; row < y + 16; ++row)
                for (int col = x; col < x + 16; ++col)
                {
                    auto pixel = mapped.bits + row * mapped.pitch + col * 4;
                    if (theme ? pixel[2] > 100 : pixel[2] < 180) ++ink;
                }
                expect(ink > 2) << "missing SVG pixels" << i << theme;
            }
            readback->Unmap();
            wchar_t path[32768]{};
            if (GetEnvironmentVariableW(L"FOLIA_SYNTAX_ICON_PREVIEW", path, static_cast<DWORD>(std::size(path))))
                SavePreview(resources, readback.Get(), path);

            auto pixels = [&](std::shared_ptr<const folia::SyntaxMarkerTheme> const& themeAssets) {
                context->SetTarget(target.Get());
                context->BeginDraw();
                context->Clear(D2D1::ColorF(D2D1::ColorF::White));
                resources.syntaxIcons.Draw(context, SyntaxIcon::Star, {0, 0, 24, 24},
                    D2D1::ColorF(D2D1::ColorF::Black), themeAssets);
                winrt::check_hresult(context->EndDraw());
                context->SetTarget(nullptr);
                winrt::check_hresult(readback->CopyFromBitmap(nullptr, target.Get(), nullptr));
                winrt::check_hresult(readback->Map(D2D1_MAP_OPTIONS_READ, &mapped));
                std::vector<std::uint8_t> result;
                for (UINT32 y = 0; y < 24; ++y)
                    result.insert(result.end(), mapped.bits + y * mapped.pitch, mapped.bits + y * mapped.pitch + 24 * 4);
                readback->Unmap();
                return result;
            };
            auto standard = pixels(assets);
            auto custom = std::make_shared<folia::SyntaxMarkerTheme>(*assets);
            custom->svg[static_cast<std::size_t>(SyntaxIcon::Star)] =
                R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><circle cx="12" cy="12" r="8" fill="currentColor"/></svg>)";
            std::weak_ptr<const folia::SyntaxMarkerTheme> previous = custom;
            expect(static_cast<bool>(pixels(custom) != standard));
            custom.reset();
            expect(!previous.expired()); // current device cache owns its theme
            expect(static_cast<bool>(pixels(assets) == standard));
            expect(previous.expired()); // switching releases the previous SVG set
            resources.syntaxIcons.Reset();
            expect(static_cast<bool>(pixels(assets) == standard));
            if (GetEnvironmentVariableW(L"FOLIA_MATH_MARKER_PREVIEW", path, static_cast<DWORD>(std::size(path))))
                SaveMathBlockPreview(resources, assets, path);
          } catch (winrt::hresult_error const& error) {
              throw std::runtime_error(std::format("syntax SVG rendering: HRESULT 0x{:08X}: {}",
                  static_cast<std::uint32_t>(error.code().value), winrt::to_string(error.message())));
          }
        };
    };
}
