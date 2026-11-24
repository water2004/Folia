#pragma once

#include "editor/rendering/EditorStyleSheet.h"

namespace winrt::Folia {
    // One reusable vector mask and GPU blur per editor device. Popup content
    // never enters this cache, and replacing its size releases the old mask.
    class EditorPopupShadow {
    public:
        void Draw(ID2D1DeviceContext* context, D2D1_RECT_F bounds, folia::ThemeMathPreview const& style);
        void Reset();
    private:
        ::Microsoft::WRL::ComPtr<ID2D1DeviceContext> recorder;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> maskBrush;
        ::Microsoft::WRL::ComPtr<ID2D1Effect> shadow;
        D2D1_SIZE_F size{};
        float radius = -1;
    };
}
