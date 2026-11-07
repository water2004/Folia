#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi.h>
#include <objidl.h>
#include <dwrite.h>
#include <cstdio>
#include <wrl/client.h>
#include "support/folia_test.hpp"
import folia.platform.editor_text_paint;

using namespace boost::ut;
using namespace folia::platform::editor;
using Microsoft::WRL::ComPtr;

namespace {
struct TextTestSurface {
    ComPtr<ID3D11Device> d3d;
    ComPtr<ID2D1DeviceContext5> context;
    ComPtr<ID2D1Bitmap1> target;

    TextTestSurface() {
        expect(fatal(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP,
            nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
            D3D11_SDK_VERSION, &d3d, nullptr, nullptr))));
        ComPtr<IDXGIDevice> dxgi;
        expect(fatal(SUCCEEDED(d3d.As(&dxgi))));
        ComPtr<ID2D1Factory1> factory;
        expect(fatal(SUCCEEDED(D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&factory)))));
        ComPtr<ID2D1Device> device;
        expect(fatal(SUCCEEDED(factory->CreateDevice(dxgi.Get(), &device))));
        ComPtr<ID2D1DeviceContext> base;
        expect(fatal(SUCCEEDED(device->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &base))));
        expect(fatal(SUCCEEDED(base.As(&context))));
        auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        expect(fatal(SUCCEEDED(context->CreateBitmap(
            D2D1::SizeU(160, 120), nullptr, 0, &properties, &target))));
        context->SetTarget(target.Get());
    }

    std::vector<unsigned char> Pixels() {
        ComPtr<ID2D1Bitmap1> readable;
        auto properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            target->GetPixelFormat());
        expect(fatal(SUCCEEDED(context->CreateBitmap(
            target->GetPixelSize(), nullptr, 0, &properties, &readable))));
        expect(fatal(SUCCEEDED(readable->CopyFromBitmap(nullptr, target.Get(), nullptr))));
        D2D1_MAPPED_RECT mapped{};
        expect(fatal(SUCCEEDED(readable->Map(D2D1_MAP_OPTIONS_READ, &mapped))));
        std::vector<unsigned char> pixels(160 * 120 * 4);
        for (std::size_t row = 0; row < 120; ++row)
            std::copy_n(mapped.bits + row * mapped.pitch, 160 * 4, pixels.data() + row * 160 * 4);
        expect(fatal(SUCCEEDED(readable->Unmap())));
        return pixels;
    }
};


}

suite editor_text_paint_tests = [] {
"viewport glyph painting matches DirectWrite shaping effects and decorations"_test = [] {
    TextTestSurface surface;
    ComPtr<IDWriteFactory> factory;
    expect(fatal(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf())))));
    ComPtr<IDWriteTextFormat> format;
    expect(fatal(SUCCEEDED(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 17, L"en-US", &format))));
    ComPtr<ID2D1SolidColorBrush> ink, red;
    expect(fatal(SUCCEEDED(surface.context->CreateSolidColorBrush(D2D1::ColorF(0x21242a), &ink))));
    expect(fatal(SUCCEEDED(surface.context->CreateSolidColorBrush(D2D1::ColorF(0xb21b38), &red))));
    for (auto text : {std::wstring{L"office fi x\u0301 \u4e2d\u6587 \xd83d\xde00\nregular bold italic\nnext line 0123456789"},
        std::wstring{L"\u0645\u0631\u062d\u0628\u0627 abc \u05e9\u05dc\u05d5\u05dd\nRTL text\nmore words"}}) {
        for (bool rtl : {false, true}) {
            ComPtr<IDWriteTextLayout> layout;
            expect(fatal(SUCCEEDED(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()),
                format.Get(), 140, 10000, &layout))));
            layout->SetReadingDirection(rtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
            layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, {2, 4});
            layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, {8, 8});
            layout->SetDrawingEffect(red.Get(), {5, 9});
            layout->SetUnderline(TRUE, {1, 12});
            layout->SetStrikethrough(TRUE, {15, 9});
            auto plan = EditorTextPaintPlan::Prepare(layout.Get());
            for (auto dpi : {96.f, 144.f, 192.f}) {
                surface.context->SetDpi(dpi, dpi);
                for (float y : {7.25f, -32.5f, -100.f}) {
                    std::vector<unsigned char> reference;
                    for (bool custom : {false, true}) {
                        surface.context->BeginDraw();
                        surface.context->Clear(D2D1::ColorF(D2D1::ColorF::White));
                        auto origin = D2D1::Point2F(3.25f, y);
                        if (custom) expect(SUCCEEDED(plan->Draw(surface.context.Get(), origin, ink.Get(),
                            {0, 0, 160 * 96 / dpi, 120 * 96 / dpi})));
                        else surface.context->DrawTextLayout(origin, layout.Get(), ink.Get());
                        expect(fatal(SUCCEEDED(surface.context->EndDraw())));
                        auto pixels = surface.Pixels();
                        if (!custom) reference = std::move(pixels);
                        else {
                            bool equal = std::ranges::equal(pixels, reference);
                            if (!equal) {
                                std::size_t changed = 0, left = 160, right = 0, top = 120, bottom = 0;
                                for (std::size_t at = 0; at < pixels.size(); at += 4) {
                                    if (std::equal(pixels.begin()+at, pixels.begin()+at+4, reference.begin()+at)) continue;
                                    ++changed;
                                    auto x = at / 4 % 160, row = at / 4 / 160;
                                    left = (std::min)(left,x); right = (std::max)(right,x);
                                    top = (std::min)(top,row); bottom = (std::max)(bottom,row);
                                }
                                std::fprintf(stderr, "text paint difference %zu pixels rect %zu,%zu-%zu,%zu dpi %.0f y %.2f RTL %d sample %x\n",
                                    changed,left,top,right,bottom,dpi,y,rtl,text.front());
                            }
                            expect(equal) << "DPI " << dpi << " y " << y << " RTL " << rtl;
                        }
                    }
                }
            }
        }
    }
};

"viewport glyph submission is bounded by visible content in a long native layout"_test = [] {
    TextTestSurface surface;
    ComPtr<IDWriteFactory> factory;
    expect(fatal(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.GetAddressOf())))));
    ComPtr<IDWriteTextFormat> format;
    expect(fatal(SUCCEEDED(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 17, L"en-US", &format))));
    ComPtr<ID2D1SolidColorBrush> ink;
    expect(fatal(SUCCEEDED(surface.context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &ink))));
    std::wstring text;
    for (int i = 0; i < 5000; ++i) text += L"Sample paragraph rendering and scrolling.\n";
    ComPtr<IDWriteTextLayout> layout;
    expect(fatal(SUCCEEDED(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()),
        format.Get(), 140, 1000000, &layout))));
    auto plan = EditorTextPaintPlan::Prepare(layout.Get());
    for (float offset : {0.f, -5000.f, -100000.f}) {
        EditorTextPaintStatistics stats;
        surface.context->BeginDraw();
        expect(SUCCEEDED(plan->Draw(surface.context.Get(), {0, offset}, ink.Get(), {0, 0, 160, 120}, &stats)));
        expect(fatal(SUCCEEDED(surface.context->EndDraw())));
        expect(stats.visitedRuns > 1000_u);
        expect(stats.paintedRuns > 0_u);
        expect(stats.paintedGlyphs < 1000_u);
    }
};
};
