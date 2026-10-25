#pragma once

import folia.core.syntax_marker;

namespace winrt::Folia
{
    using SyntaxIcon = folia::SyntaxMarker;
    enum class SyntaxIconPlacement { Inline, Block };
    void ApplySyntaxIconObject(IDWriteTextLayout* layout, UINT32 start, UINT32 length,
        float size, float gap);

    // A finite, device-owned set. SVGs come from the selected theme; drawing performs
    // no filesystem access and creates no per-occurrence XAML control.
    struct EditorSyntaxIcons
    {
        void Reset();
        void Draw(ID2D1DeviceContext* target, SyntaxIcon icon, D2D1_RECT_F bounds, D2D1_COLOR_F color,
            std::shared_ptr<const folia::SyntaxMarkerTheme> const& assets);
    private:
        ::Microsoft::WRL::ComPtr<ID2D1DeviceContext5> context;
        std::shared_ptr<const folia::SyntaxMarkerTheme> theme;
        std::array<::Microsoft::WRL::ComPtr<ID2D1SvgDocument>, static_cast<std::size_t>(SyntaxIcon::Count)> documents;
    };
}
