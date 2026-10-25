#pragma once

namespace winrt::Folia
{
    enum class SyntaxIcon : std::uint8_t
    {
        None, Star, DoubleStar, Underscore, DoubleUnderscore, Tilde, DoubleTilde, Backtick, DoubleBacktick, TripleBacktick, Dollar, DoubleDollar, MathOpen, MathClose, Count
    };

    // The input is an already classified CST marker, never arbitrary body text.
    SyntaxIcon SyntaxIconFor(std::u32string_view marker) noexcept;
    void ApplySyntaxIconObject(IDWriteTextLayout* layout, UINT32 start, UINT32 length,
        float size, float gap);

    // A finite, device-owned set. SVGs are embedded resources; drawing performs
    // no filesystem access and creates no per-occurrence XAML control.
    struct EditorSyntaxIcons
    {
        void Reset();
        void Draw(ID2D1DeviceContext* target, SyntaxIcon icon, D2D1_RECT_F bounds, D2D1_COLOR_F color);
    private:
        ::Microsoft::WRL::ComPtr<ID2D1DeviceContext5> context;
        std::array<::Microsoft::WRL::ComPtr<ID2D1SvgDocument>, static_cast<std::size_t>(SyntaxIcon::Count)> documents;
    };
}
