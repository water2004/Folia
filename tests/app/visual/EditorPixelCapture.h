#pragma once

#include "editor/rendering/EditorRenderResources.h"
#include <dwmapi.h>
#include <wil/resource.h>

namespace app_tests {
    struct GlyphPixelError {
        unsigned samples;
        double mean;
        unsigned outsideTolerance;
    };

    // Compare DirectWrite's premultiplied texture with its actual desktop
    // pixels. The column before the glyphs provides the local paper color.
    inline GlyphPixelError MeasurePresentedGlyphs(winrt::Folia::EditorRenderResources& r, HWND hwnd,
        winrt::Windows::Graphics::PointInt32 origin, unsigned left, unsigned top, unsigned width, unsigned height) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width; desc.Height = height;
        desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ::Microsoft::WRL::ComPtr<ID3D11Texture2D> snapshot;
        winrt::check_hresult(r.d3dDevice->CreateTexture2D(&desc, nullptr, &snapshot));
        D3D11_BOX region{left, top, 0, left + width, top + height, 1};
        r.d3dContext->CopySubresourceRegion(snapshot.Get(), 0, 0, 0, 0, r.renderTexture.Get(), 0, &region);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        winrt::check_hresult(r.d3dContext->Map(snapshot.Get(), 0, D3D11_MAP_READ, 0, &mapped));
        auto unmap = wil::scope_exit([&] { r.d3dContext->Unmap(snapshot.Get(), 0); });
        auto dc = ::GetDC(nullptr);
        auto release = wil::scope_exit([&] { ::ReleaseDC(nullptr, dc); });
        auto mem = ::CreateCompatibleDC(dc);
        auto releaseMem = wil::scope_exit([&] { ::DeleteDC(mem); });
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), static_cast<LONG>(width + 1), -static_cast<LONG>(height), 1, 32, BI_RGB};
        void* bits{};
        auto bitmap = ::CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        winrt::check_bool(bitmap != nullptr);
        auto releaseBitmap = wil::scope_exit([&] { ::DeleteObject(bitmap); });
        auto old = ::SelectObject(mem, bitmap);
        auto restore = wil::scope_exit([&] { ::SelectObject(mem, old); });
        ::DwmFlush();
        POINT at{origin.X + static_cast<int>(left), origin.Y + static_cast<int>(top)};
        winrt::check_bool(::GetAncestor(::WindowFromPoint(at), GA_ROOT) == hwnd);
        winrt::check_bool(::BitBlt(mem, 0, 0, width + 1, height, dc, at.x - 1, at.y, SRCCOPY));
        ::GdiFlush();
        double error = 0;
        unsigned edgeCount = 0, off = 0;
        for (unsigned y = 0; y < height; ++y) {
            auto row = static_cast<std::uint8_t const*>(bits) + y * (width + 1) * 4;
            auto source = static_cast<std::uint8_t const*>(mapped.pData) + y * mapped.RowPitch;
            for (unsigned x = 0; x + 2 < width; ++x) {
                auto alpha = source[x * 4 + 3];
                if (!alpha) continue;
                auto expected = source[x * 4 + 1] + row[1] * (255 - alpha) / 255.;
                auto diff = std::abs(row[(x + 1) * 4 + 1] - expected);
                error += diff;
                if (diff > 15) ++off;
                ++edgeCount;
            }
        }
        return {edgeCount, error / (std::max)(1u, edgeCount), off};
    }
}
