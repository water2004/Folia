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
    unsigned weight = 400;
    bool italic = false;
};
struct MermaidTextMetrics {
    double advance{};
    double ink_width{};
    // Union of the advance interval with native glyph ink bounds, not
    // Chromium's raster-hinted SVG bounding box. See compare-text-probe.mjs
    // for measured corpus-specific error ceilings; no per-string correction.
    double bounds_width{};
    double line_height{};
    double baseline{};
};
struct MermaidTextStyle {
    std::size_t offset=0,length=0;
    unsigned weight=400;
    bool italic=false;
};

// Measures UTF-16 after SVG's default whitespace processing (collapse ASCII
// whitespace, including LF/CR/TAB). The browser probe checks the
// chosen width against real SVG bounds on the same machine. No approximation
// is returned on failure. Sizes and results are 96-DPI SVG user units.
class MermaidMeasurer {
    static bool apply_styles(IDWriteTextLayout* layout,std::span<const MermaidTextStyle> styles,
        std::size_t length) {
        for(const auto& style:styles) {
            if(style.offset>length||style.length>length-style.offset) return false;
            const DWRITE_TEXT_RANGE range{static_cast<UINT32>(style.offset),static_cast<UINT32>(style.length)};
            if(FAILED(layout->SetFontWeight(static_cast<DWRITE_FONT_WEIGHT>(style.weight),range))||
                FAILED(layout->SetFontStyle(style.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,range)))
                return false;
        }
        return true;
    }
public:
    // Resolve the configured CSS family list once, then share that exact family
    // between DirectWrite measurement and SVG emission. Generic families have
    // explicit Windows mappings; missing named candidates advance in list order.
    std::expected<std::u16string, std::string> resolve_family(std::u16string_view list) const {
        using Microsoft::WRL::ComPtr;
        ComPtr<IDWriteFactory> factory;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))
            return std::unexpected("DirectWrite factory creation failed");
        ComPtr<IDWriteFontCollection> fonts;
        if (FAILED(factory->GetSystemFontCollection(&fonts)))
            return std::unexpected("DirectWrite font collection failed");
        std::size_t pos=0;
        while (pos<list.size()) {
            std::u16string candidate;
            char16_t quote=0;
            while (pos<list.size()) {
                auto c=list[pos++];
                if (c==u'\0') return std::unexpected("Invalid font family list");
                if (quote) { if(c==quote) quote=0; else candidate+=c; }
                else if(c==u'\'' || c==u'"') quote=c;
                else if(c==u',') break;
                else candidate+=c;
            }
            if (quote) return std::unexpected("Unclosed font family quote");
            auto first=candidate.find_first_not_of(u" \t\r\n");
            if(first==candidate.npos) continue;
            candidate=candidate.substr(first,candidate.find_last_not_of(u" \t\r\n")-first+1);
            auto lower=candidate;
            for(auto& c:lower) if(c>=u'A' && c<=u'Z') c+=u'a'-u'A';
            if(lower==u"sans-serif") candidate=u"Arial";
            else if(lower==u"serif") candidate=u"Times New Roman";
            else if(lower==u"monospace") candidate=u"Courier New";
            else if(lower==u"system-ui") candidate=u"Segoe UI";
            std::wstring family(candidate.begin(),candidate.end());
            UINT32 index{}; BOOL exists{};
            if(FAILED(fonts->FindFamilyName(family.c_str(),&index,&exists)))
                return std::unexpected("DirectWrite family lookup failed");
            if(exists) return candidate;
        }
        return std::unexpected("No configured Mermaid font is installed");
    }
    std::expected<MermaidTextMetrics, std::string> measure(
        std::u16string_view source, MermaidFont const& font, bool preserve_spaces=false,
        std::span<const MermaidTextStyle> styles={}) const {
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
            if (!preserve_spaces && (c == u' ' || c == u'\t' || c == u'\n' || c == u'\r')) { pending_space = !text.empty(); continue; }
            if (pending_space) text += L' ';
            pending_space = false;
            text += static_cast<wchar_t>(c);
        }
        if (text.size() > std::numeric_limits<UINT32>::max())
            return std::unexpected("Mermaid display text exceeds DirectWrite limits");
        if (text.empty()) return MermaidTextMetrics{};
        ComPtr<IDWriteTextFormat> format;
        if (FAILED(factory->CreateTextFormat(family.c_str(), fonts.Get(),
            static_cast<DWRITE_FONT_WEIGHT>(font.weight), font.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            font.size, L"en-us", &format)))
            return std::unexpected("DirectWrite format creation failed");
        if (FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)))
            return std::unexpected("DirectWrite wrapping configuration failed");
        ComPtr<IDWriteTextLayout> layout;
        constexpr float extent = 1000000;
        if (FAILED(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()),
            format.Get(), extent, extent, &layout)))
            return std::unexpected("DirectWrite layout creation failed");
        if(!styles.empty() && (!preserve_spaces||!apply_styles(layout.Get(),styles,text.size())))
            return std::unexpected("Invalid DirectWrite styled text ranges");
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
        DWRITE_LINE_METRICS line{};UINT32 line_count{};
        if (FAILED(layout->GetLineMetrics(&line,1,&line_count)) || line_count!=1)
            return std::unexpected("DirectWrite single-line metrics query failed");
        return MermaidTextMetrics{advance, ink_width, bounds_width,line.height,line.baseline};
    }
    // Native Unicode line-break opportunities; preserve each UTF-16 slice so
    // measured display lines and SVG text use the same characters and spacing.
    std::expected<std::vector<std::u16string>,std::string> wrap(
        std::u16string_view source, MermaidFont const& font, double width,
        std::span<const MermaidTextStyle> styles={}) const {
        using Microsoft::WRL::ComPtr;
        auto valid=measure(source,font,true,styles);
        if(!valid) return std::unexpected(valid.error());
        if(source.empty()||valid->advance<=width) return std::vector<std::u16string>{std::u16string(source)};
        if(!std::isfinite(width)||width<=0||width>std::numeric_limits<float>::max())
            return std::unexpected("Invalid Mermaid wrapping width");
        ComPtr<IDWriteFactory> factory;
        if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))
            return std::unexpected("DirectWrite factory creation failed");
        const std::wstring family(font.family.begin(),font.family.end());
        const std::wstring text(source.begin(),source.end());
        ComPtr<IDWriteTextFormat> format;
        if(FAILED(factory->CreateTextFormat(family.c_str(),nullptr,static_cast<DWRITE_FONT_WEIGHT>(font.weight),
            font.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,font.size,L"en-us",&format)) ||
            FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WHOLE_WORD)))
            return std::unexpected("DirectWrite wrapping format failed");
        ComPtr<IDWriteTextLayout> layout;
        if(FAILED(factory->CreateTextLayout(text.data(),static_cast<UINT32>(text.size()),format.Get(),
            static_cast<float>(width),1000000,&layout)))
            return std::unexpected("DirectWrite wrapped layout failed");
        if(!apply_styles(layout.Get(),styles,text.size()))
            return std::unexpected("Invalid DirectWrite styled text ranges");
        UINT32 count{};layout->GetLineMetrics(nullptr,0,&count);
        std::vector<DWRITE_LINE_METRICS> metrics(count);
        if(FAILED(layout->GetLineMetrics(metrics.data(),count,&count)))
            return std::unexpected("DirectWrite wrapped line metrics failed");
        std::vector<std::u16string> lines;std::size_t offset=0;
        for(const auto& line:metrics) {
            lines.emplace_back(source.substr(offset,line.length));offset+=line.length;
        }
        return lines;
    }
};
}
