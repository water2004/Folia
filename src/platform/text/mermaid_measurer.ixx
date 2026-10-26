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
struct MermaidSvgFontMetrics { double height{},baseline{}; };

// Measures UTF-16 after SVG's default whitespace processing (collapse ASCII
// whitespace, including LF/CR/TAB). The browser probe checks the
// chosen width against real SVG bounds on the same machine. No approximation
// is returned on failure. Sizes and results are 96-DPI SVG user units.
class MermaidMeasurer {
    // SVG text boxes use raster-hinted glyph bounds. Sequence's integer width
    // comparisons can change a wrap at a one-pixel difference, so retain shaped
    // advances but measure each glyph's coverage at its origin.
    class SvgBounds final : public IDWriteTextRenderer {
        IDWriteFactory* factory;
    public:
        double left=0,right=0;
        explicit SvgBounds(IDWriteFactory* value):factory(value) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {
            if(iid==__uuidof(IUnknown)||iid==__uuidof(IDWritePixelSnapping)||iid==__uuidof(IDWriteTextRenderer)) {
                *out=static_cast<IDWriteTextRenderer*>(this);AddRef();return S_OK;
            }
            *out=nullptr;return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
        ULONG STDMETHODCALLTYPE Release() override {return 1;}
        HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*,BOOL* value) override {*value=TRUE;return S_OK;}
        HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*,DWRITE_MATRIX* value) override {*value={1,0,0,1,0,0};return S_OK;}
        HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*,FLOAT* value) override {*value=1;return S_OK;}
        HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,FLOAT x,FLOAT,DWRITE_MEASURING_MODE,
            const DWRITE_GLYPH_RUN* run,const DWRITE_GLYPH_RUN_DESCRIPTION*,IUnknown*) override {
            double pen=x;
            const bool rtl=run->bidiLevel%2;
            for(UINT32 i=0;i<run->glyphCount;++i) {
                const auto advance=run->glyphAdvances[i];
                if(rtl) pen-=advance;
                const float zero=0;
                const DWRITE_GLYPH_RUN glyph{run->fontFace,run->fontEmSize,1,run->glyphIndices+i,&zero,nullptr,run->isSideways,0};
                Microsoft::WRL::ComPtr<IDWriteGlyphRunAnalysis> analysis;
                // Honor the font's hinted/embedded-bitmap mode. Small CJK fonts
                // can have wider hinted coverage than their scalable outlines.
                Microsoft::WRL::ComPtr<IDWriteRenderingParams> params;
                auto hr=factory->CreateRenderingParams(&params);
                if(FAILED(hr)) return hr;
                DWRITE_RENDERING_MODE mode;
                hr=run->fontFace->GetRecommendedRenderingMode(run->fontEmSize,1,
                    DWRITE_MEASURING_MODE_NATURAL,params.Get(),&mode);
                if(FAILED(hr)) return hr;
                hr=factory->CreateGlyphRunAnalysis(&glyph,1,nullptr,mode,
                    DWRITE_MEASURING_MODE_NATURAL,0,0,&analysis);
                if(FAILED(hr)) return hr;
                RECT bounds{};
                if(FAILED(hr=analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1,&bounds))) return hr;
                const auto offset=run->glyphOffsets?run->glyphOffsets[i].advanceOffset:0;
                if(bounds.right>bounds.left) {
                    const auto origin=pen+(rtl?-offset:offset);
                    left=std::min(left,origin+bounds.left);right=std::max(right,origin+bounds.right);
                }
                if(!rtl) pen+=advance;
                left=std::min(left,pen);right=std::max(right,pen);
            }
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE DrawUnderline(void*,FLOAT,FLOAT,const DWRITE_UNDERLINE*,IUnknown*) override {return E_NOTIMPL;}
        HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*,FLOAT,FLOAT,const DWRITE_STRIKETHROUGH*,IUnknown*) override {return E_NOTIMPL;}
        HRESULT STDMETHODCALLTYPE DrawInlineObject(void*,FLOAT,FLOAT,IDWriteInlineObject*,BOOL,BOOL,IUnknown*) override {return E_NOTIMPL;}
    };
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
    // SVG needs the font's ascent/descent box, excluding the layout line gap.
    std::expected<MermaidSvgFontMetrics,std::string> svg_font_metrics(const MermaidFont& font) const {
        using Microsoft::WRL::ComPtr;
        ComPtr<IDWriteFactory> factory;
        if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))
            return std::unexpected("DirectWrite factory creation failed");
        ComPtr<IDWriteFontCollection> collection;
        ComPtr<IDWriteFontFamily> family;
        ComPtr<IDWriteFont> selected;
        const std::wstring name(font.family.begin(),font.family.end());
        UINT32 index{}; BOOL exists{};
        if(FAILED(factory->GetSystemFontCollection(&collection)) ||
            FAILED(collection->FindFamilyName(name.c_str(),&index,&exists)) || !exists ||
            FAILED(collection->GetFontFamily(index,&family)) ||
            FAILED(family->GetFirstMatchingFont(static_cast<DWRITE_FONT_WEIGHT>(font.weight),
                DWRITE_FONT_STRETCH_NORMAL,font.italic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,&selected)))
            return std::unexpected("DirectWrite SVG font metric lookup failed");
        DWRITE_FONT_METRICS metrics{}; selected->GetMetrics(&metrics);
        const auto scale=font.size/static_cast<double>(metrics.designUnitsPerEm);
        const auto baseline=std::round(metrics.ascent*scale);
        return MermaidSvgFontMetrics{baseline+std::round(metrics.descent*scale),baseline};
    }
    std::expected<double,std::string> svg_font_height(const MermaidFont& font) const {
        auto metrics=svg_font_metrics(font);if(!metrics) return std::unexpected(metrics.error());
        return metrics->height;
    }
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
        std::span<const MermaidTextStyle> styles={},bool svg_bounds=false) const {
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
        double bounds_width = advance + std::max(0.0, static_cast<double>(ink.right)) +
            std::max(0.0, static_cast<double>(ink.left));
        if(svg_bounds) {
            SvgBounds bounds{factory.Get()};
            if(FAILED(layout->Draw(nullptr,&bounds,0,0)))
                return std::unexpected("DirectWrite SVG glyph bounds query failed");
            bounds_width=std::max(advance,bounds.right)-bounds.left;
        }
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
        std::span<const MermaidTextStyle> styles={},bool break_spaces=true) const {
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
            FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)))
            return std::unexpected("DirectWrite wrapping format failed");
        ComPtr<IDWriteTextLayout> layout;
        if(FAILED(factory->CreateTextLayout(text.data(),static_cast<UINT32>(text.size()),format.Get(),
            static_cast<float>(width),1000000,&layout)))
            return std::unexpected("DirectWrite wrapped layout failed");
        if(!apply_styles(layout.Get(),styles,text.size()))
            return std::unexpected("Invalid DirectWrite styled text ranges");
        UINT32 count{};layout->GetClusterMetrics(nullptr,0,&count);
        std::vector<DWRITE_CLUSTER_METRICS> metrics(count);
        if(FAILED(layout->GetClusterMetrics(metrics.data(),count,&count)))
            return std::unexpected("DirectWrite cluster metrics failed");
        // createText uses CSS break-spaces after constraining the container.
        // Its trailing spaces take width; DirectWrite's built-in wrapping
        // excludes them when deciding whether another word fits. Retain native
        // cluster widths and Unicode break opportunities, including those spaces.
        std::vector<std::u16string> lines;
        std::size_t start=0,segment=0,end=0;double used=0,segment_width=0,trailing_width=0;
        for(std::size_t i=0;i<metrics.size();++i) {
            const auto& cluster=metrics[i];end+=cluster.length;segment_width+=cluster.width;
            trailing_width=cluster.isWhitespace?trailing_width+cluster.width:0;
            if(!cluster.canWrapLineAfter && i+1<metrics.size()) continue;
            if(segment>start && used+segment_width-(break_spaces?0:trailing_width)>width) {
                lines.emplace_back(source.substr(start,segment-start));start=segment;used=0;
            }
            used+=segment_width;segment_width=0;trailing_width=0;segment=end;
        }
        lines.emplace_back(source.substr(start));
        return lines;
    }
};
}
