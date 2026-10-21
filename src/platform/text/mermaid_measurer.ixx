module;
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwrite.h>
#include <wrl/client.h>

export module folia.platform.mermaid_measurer;
import std;

export namespace folia::platform {

struct MermaidFont {
    // One resolved installed family, not a CSS family-list expression.
    std::u16string family;
    float size = 17;
};
struct MermaidTextMetrics {
    double advance{};
    double ink_width{};
    // Union of the advance interval with native glyph ink bounds, not
    // Chromium's raster-hinted SVG bounding box. See compare-text-probe.mjs
    // for measured corpus-specific error ceilings; no per-string correction.
    double bounds_width{};
};

// Measures UTF-16 after SVG's default whitespace processing (collapse ASCII
// whitespace, including LF/CR/TAB). The browser probe checks the
// chosen width against real SVG bounds on the same machine. No approximation
// is returned on failure. Sizes and results are 96-DPI SVG user units.
class MermaidMeasurer {
public:
    std::expected<MermaidTextMetrics, std::string> measure(
        std::u16string_view source, MermaidFont const& font) const {
        using Microsoft::WRL::ComPtr;
        if (font.family.empty() || font.family.find(u'\0') != std::u16string::npos ||
            !std::isfinite(font.size) || font.size <= 0)
            return std::unexpected("Invalid resolved Mermaid font");
        std::wstring family(font.family.begin(), font.family.end());
        ComPtr<IDWriteFactory> factory;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))
            return std::unexpected("DirectWrite factory creation failed");
        ComPtr<IDWriteFontCollection> fonts;
        UINT32 index{};
        BOOL exists{};
        if (FAILED(factory->GetSystemFontCollection(&fonts)) ||
            FAILED(fonts->FindFamilyName(family.c_str(), &index, &exists)) || !exists)
            return std::unexpected("Resolved Mermaid font is not installed");
        std::wstring text;
        bool pending_space = false;
        for (std::size_t i = 0; i < source.size(); ++i) {
            auto c = source[i];
            if (c >= 0xd800 && c <= 0xdbff) {
                if (i + 1 == source.size() || source[i+1] < 0xdc00 || source[i+1] > 0xdfff)
                    return std::unexpected("Invalid UTF-16 display text");
            } else if (c >= 0xdc00 && c <= 0xdfff &&
                (i == 0 || source[i-1] < 0xd800 || source[i-1] > 0xdbff))
                return std::unexpected("Invalid UTF-16 display text");
            if (c == u' ' || c == u'\t' || c == u'\n' || c == u'\r') { pending_space = !text.empty(); continue; }
            if (pending_space) text += L' ';
            pending_space = false;
            text += static_cast<wchar_t>(c);
        }
        if (text.size() > std::numeric_limits<UINT32>::max())
            return std::unexpected("Mermaid display text exceeds DirectWrite limits");
        if (text.empty()) return MermaidTextMetrics{};
        ComPtr<IDWriteTextFormat> format;
        if (FAILED(factory->CreateTextFormat(family.c_str(), fonts.Get(),
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            font.size, L"en-us", &format)))
            return std::unexpected("DirectWrite format creation failed");
        if (FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)))
            return std::unexpected("DirectWrite wrapping configuration failed");
        ComPtr<IDWriteTextLayout> layout;
        constexpr float extent = 1000000;
        if (FAILED(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()),
            format.Get(), extent, extent, &layout)))
            return std::unexpected("DirectWrite layout creation failed");
        DWRITE_TEXT_METRICS metrics{};
        DWRITE_OVERHANG_METRICS ink{};
        if (FAILED(layout->GetMetrics(&metrics)))
            return std::unexpected("DirectWrite metrics query failed");
        // Tighten the layout box before querying overhangs: subtracting a
        // million-DIP float box would discard subpixel glyph-bound precision.
        if (FAILED(layout->SetMaxWidth(metrics.widthIncludingTrailingWhitespace)) ||
            FAILED(layout->GetOverhangMetrics(&ink)))
            return std::unexpected("DirectWrite ink bounds query failed");
        double const advance = metrics.widthIncludingTrailingWhitespace;
        double const ink_width = advance + static_cast<double>(ink.right) + ink.left;
        double const bounds_width = advance + std::max(0.0, static_cast<double>(ink.right)) +
            std::max(0.0, static_cast<double>(ink.left));
        if (!std::isfinite(advance) || !std::isfinite(ink_width) || advance < 0 || ink_width < 0)
            return std::unexpected("Invalid DirectWrite metrics");
        return MermaidTextMetrics{advance, ink_width, bounds_width};
    }
};
}
