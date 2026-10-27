#pragma once
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorEmbeddedDraw.h"
#include "editor/rendering/EditorRenderResources.h"

namespace winrt::Folia {
    struct EditorTextLayoutEngine;
    // Shared prepared visual for flows and table cells. Its source span comes
    // from the CST render projection; it is never another editing coordinate.
    struct EditorPreparedMathPreview {
        DisplayInlineText display;
        ::Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        D2D1_RECT_F contentBounds{};
        folia::TextSpan contentSpan;
        bool floating = false;
        float Width() const { return contentBounds.right - contentBounds.left; }
        float Height() const { return contentBounds.bottom - contentBounds.top; }
    };
    struct EditorMathPreviewPlacement {
        D2D1_RECT_F bounds{};
        D2D1_POINT_2F origin{};
    };
    std::optional<EditorMathPreviewPlacement> PlaceInlineMathPreview(
        D2D1_RECT_F caret, float anchorX, D2D1_SIZE_F content, D2D1_SIZE_F viewport,
        folia::ThemeMathPreview const& style = {});
    EditorPreparedMathPreview PrepareMathPreview(DisplayInlineText::MathPreview const& input,
        EditorTextLayoutEngine& engine, IDWriteTextFormat* format, float width, float viewportWidth,
        folia::ThemeMathPreview const& style);
    void PaintMathPreviewContent(EditorPreparedMathPreview const& preview, D2D1_POINT_2F origin,
        EditorRenderResources& resources, EditorStyleSheet const& styleSheet,
        EditorDrawMath const& drawMath, EditorDrawMathFallback const& drawFallback);
    std::optional<D2D1_RECT_F> PaintInlineMathPreview(
        EditorPreparedMathPreview const& preview, D2D1_RECT_F caret, float anchorX,
        EditorRenderResources& resources, EditorStyleSheet const& styleSheet,
        EditorDrawMath const& drawMath, EditorDrawMathFallback const& drawFallback);
}
