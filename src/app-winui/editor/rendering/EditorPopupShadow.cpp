#include "pch.h"
import std;
#include "editor/rendering/EditorPopupShadow.h"
#include <d2d1effects.h>

namespace winrt::Folia {
    void EditorPopupShadow::Reset() {
        shadow.Reset(); maskBrush.Reset(); recorder.Reset();
        size = {}; radius = -1;
    }

    void EditorPopupShadow::Draw(ID2D1DeviceContext* context, D2D1_RECT_F bounds,
                                 folia::ThemeMathPreview const& style) {
        if (!context || style.shadow_color.a == 0) return;
        if (!recorder) {
            ::Microsoft::WRL::ComPtr<ID2D1Device> device;
            context->GetDevice(&device);
            winrt::check_hresult(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &recorder));
            winrt::check_hresult(recorder->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &maskBrush));
            winrt::check_hresult(context->CreateEffect(CLSID_D2D1Shadow, &shadow));
        }
        D2D1_SIZE_F next{bounds.right - bounds.left, bounds.bottom - bounds.top};
        if (size.width != next.width || size.height != next.height || radius != style.corner_radius) {
            ::Microsoft::WRL::ComPtr<ID2D1CommandList> mask;
            winrt::check_hresult(recorder->CreateCommandList(&mask));
            recorder->SetTarget(mask.Get());
            recorder->BeginDraw();
            recorder->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0, 0, next.width, next.height),
                style.corner_radius, style.corner_radius), maskBrush.Get());
            auto recorded = recorder->EndDraw();
            recorder->SetTarget(nullptr);
            winrt::check_hresult(recorded);
            winrt::check_hresult(mask->Close());
            shadow->SetInput(0, mask.Get());
            size = next; radius = style.corner_radius;
        }
        auto color = style.shadow_color;
        winrt::check_hresult(shadow->SetValue(D2D1_SHADOW_PROP_COLOR,
            D2D1::Vector4F(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f)));
        winrt::check_hresult(shadow->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, style.shadow_blur));
        context->DrawImage(shadow.Get(), D2D1::Point2F(bounds.left, bounds.top + style.shadow_offset_y));
    }
}
