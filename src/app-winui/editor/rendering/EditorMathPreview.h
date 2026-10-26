#pragma once
#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorEmbeddedDraw.h"
#include "editor/rendering/EditorRenderResources.h"

namespace winrt::Folia {
    // Shared prepared visual for flows and table cells. Its source span comes
    // from the CST render projection; it is never another editing coordinate.
    struct EditorPreparedMathPreview {
        DisplayInlineText display;
        ::Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        float height = 0;
        folia::TextSpan contentSpan;
        bool floating = false;
    };
    struct EditorMathPreviewPlacement {
        D2D1_RECT_F bounds{};
        D2D1_POINT_2F origin{};
        float scale = 1;
    };
    std::optional<EditorMathPreviewPlacement> PlaceInlineMathPreview(
        D2D1_RECT_F caret, float anchorX, D2D1_SIZE_F content, D2D1_SIZE_F viewport);
    std::optional<D2D1_RECT_F> PaintInlineMathPreview(
        EditorPreparedMathPreview const& preview, D2D1_RECT_F caret, float anchorX,
        EditorRenderResources& resources, EditorStyleSheet const& styleSheet,
        EditorDrawMath const& drawMath, EditorDrawMathFallback const& drawFallback);
}
