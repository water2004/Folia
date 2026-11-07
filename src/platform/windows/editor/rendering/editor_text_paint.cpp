module;
#include <d2d1_1.h>
#include <dwrite_1.h>
#include <wrl.h>

module folia.platform.editor_text_paint;
import std;

namespace folia::platform::editor {
namespace {
using Microsoft::WRL::ComPtr;

// Both collection and painting traverse the same native layout callbacks.
// Inline objects are delegated too, so nested drawing retains callback order.
class TextRenderer final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextRenderer> {
public:
    std::function<HRESULT(float, float, DWRITE_MEASURING_MODE,
        DWRITE_GLYPH_RUN const*, DWRITE_GLYPH_RUN_DESCRIPTION const*, IUnknown*)> glyph;
    ID2D1DeviceContext* context = nullptr;
    ID2D1Brush* foreground = nullptr;
    D2D1_RECT_F viewport{};

    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) override {
        *disabled = context == nullptr; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override {
        D2D1_MATRIX_3X2_F matrix{1, 0, 0, 1, 0, 0};
        if (context) context->GetTransform(&matrix);
        *transform = {matrix._11, matrix._12, matrix._21, matrix._22, matrix._31, matrix._32};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixels) override {
        float x = 96, y = 96;
        if (context) context->GetDpi(&x, &y);
        *pixels = x / 96; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE mode,
        DWRITE_GLYPH_RUN const* run, DWRITE_GLYPH_RUN_DESCRIPTION const* description,
        IUnknown* effect) override {
        return glyph(x, y, mode, run, description, effect);
    }
    ComPtr<ID2D1Brush> Brush(IUnknown* effect) const {
        ComPtr<ID2D1Brush> brush;
        if (effect) effect->QueryInterface(IID_PPV_ARGS(&brush));
        if (!brush) brush = foreground;
        return brush;
    }
    HRESULT Decoration(float x, float y, float width, float offset, float thickness,
        DWRITE_READING_DIRECTION, DWRITE_FLOW_DIRECTION flow, IUnknown* effect) {
        if (!context) return S_OK;
        D2D1_RECT_F rect;
        if (flow == DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM || flow == DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP) {
            // DirectWrite supplies the decoration origin at its visual start,
            // already adjusted for bidi runs. Reading direction must not shift it again.
            auto left = x;
            auto top = flow == DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM ? y + offset : y - offset - thickness;
            rect = {left, top, left + width, top + thickness};
        } else {
            auto top = y;
            auto left = flow == DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT ? x + offset : x - offset - thickness;
            rect = {left, top, left + thickness, top + width};
        }
        if (rect.bottom >= viewport.top && rect.top <= viewport.bottom) {
            auto brush = Brush(effect);
            context->FillRectangle(rect, brush.Get());
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT x, FLOAT y,
        DWRITE_UNDERLINE const* line, IUnknown* effect) override {
        return Decoration(x, y, line->width, line->offset, line->thickness,
            line->readingDirection, line->flowDirection, effect);
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT x, FLOAT y,
        DWRITE_STRIKETHROUGH const* line, IUnknown* effect) override {
        return Decoration(x, y, line->width, line->offset, line->thickness,
            line->readingDirection, line->flowDirection, effect);
    }
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void* client, FLOAT x, FLOAT y,
        IDWriteInlineObject* object, BOOL sideways, BOOL rtl, IUnknown* effect) override {
        return object->Draw(client, this, x, y, sideways, rtl, effect);
    }
};
}

std::shared_ptr<const EditorTextPaintPlan> EditorTextPaintPlan::Prepare(IDWriteTextLayout* layout) {
    if (!layout) return {};
    auto plan = std::make_shared<EditorTextPaintPlan>();
    plan->layout = layout;
    auto collector = Microsoft::WRL::Make<TextRenderer>();
    std::unordered_map<IDWriteFontFace*, DWRITE_FONT_METRICS1> fonts;
    collector->glyph = [&](float, float, auto, DWRITE_GLYPH_RUN const* run, auto, auto) {
        auto infinity = std::numeric_limits<float>::infinity();
        InkBounds bounds{-infinity, infinity};
        if (!run->isSideways) {
            auto found = fonts.find(run->fontFace);
            if (found == fonts.end()) {
                ComPtr<IDWriteFontFace1> face;
                if (SUCCEEDED(run->fontFace->QueryInterface(IID_PPV_ARGS(&face)))) {
                    DWRITE_FONT_METRICS1 metrics{};
                    face->GetMetrics(&metrics);
                    found = fonts.emplace(run->fontFace, metrics).first;
                }
            }
            if (found != fonts.end() && found->second.designUnitsPerEm) {
                auto scale = run->fontEmSize / found->second.designUnitsPerEm;
                float minimumOffset = 0, maximumOffset = 0;
                if (run->glyphOffsets) {
                    for (UINT32 i = 0; i < run->glyphCount; ++i) {
                        minimumOffset = (std::min)(minimumOffset, run->glyphOffsets[i].ascenderOffset);
                        maximumOffset = (std::max)(maximumOffset, run->glyphOffsets[i].ascenderOffset);
                    }
                }
                bounds = {-found->second.glyphBoxTop * scale - maximumOffset,
                    -found->second.glyphBoxBottom * scale - minimumOffset};
            }
        }
        plan->ink.push_back(bounds);
        return S_OK;
    };
    auto result = layout->Draw(nullptr, collector.Get(), 0, 0);
    if (FAILED(result)) throw std::runtime_error("DirectWrite text paint preparation failed");
    return plan;
}

HRESULT EditorTextPaintPlan::Draw(ID2D1DeviceContext* context, D2D1_POINT_2F origin,
    ID2D1Brush* foreground, D2D1_RECT_F viewport, EditorTextPaintStatistics* statistics) const {
    if (!context || !foreground) return E_INVALIDARG;
    auto renderer = Microsoft::WRL::Make<TextRenderer>();
    renderer->context = context;
    renderer->foreground = foreground;
    renderer->viewport = viewport;
    std::size_t index = 0;
    if (statistics) *statistics = {};
    renderer->glyph = [&](float x, float y, auto mode, DWRITE_GLYPH_RUN const* run,
        DWRITE_GLYPH_RUN_DESCRIPTION const* description, IUnknown* effect) {
        if (index >= ink.size()) return E_UNEXPECTED;
        auto bounds = ink[index++];
        if (statistics) ++statistics->visitedRuns;
        if (y + bounds.bottom < viewport.top || y + bounds.top > viewport.bottom) return S_OK;
        auto brush = renderer->Brush(effect);
        context->DrawGlyphRun({x, y}, run, description, brush.Get(), mode);
        if (statistics) {
            ++statistics->paintedRuns;
            statistics->paintedGlyphs += run->glyphCount;
        }
        return S_OK;
    };
    return layout->Draw(nullptr, renderer.Get(), origin.x, origin.y);
}

}
