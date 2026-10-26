#include "pch.h"
import std;
#include "editor/rendering/EditorMathPreview.h"
#include "editor/rendering/EditorDocumentPainter.h"

namespace winrt::Folia {
    std::optional<EditorMathPreviewPlacement> PlaceInlineMathPreview(
        D2D1_RECT_F caret, float anchorX, D2D1_SIZE_F content, D2D1_SIZE_F viewport) {
        constexpr float margin = 12, gap = 8, horizontalPadding = 12, verticalPadding = 10;
        if (!std::isfinite(caret.left) || !std::isfinite(caret.top) || !std::isfinite(caret.right) || !std::isfinite(caret.bottom)
            || !std::isfinite(anchorX) || !std::isfinite(content.width) || !std::isfinite(content.height) || content.width <= 0 || content.height <= 0
            || !std::isfinite(viewport.width) || !std::isfinite(viewport.height)
            || caret.bottom <= margin || caret.top >= viewport.height - margin
            || caret.right < 0 || caret.left > viewport.width) return std::nullopt;
        const float above = caret.top - margin - gap, below = viewport.height - margin - caret.bottom - gap;
        const bool useAbove = content.height + 2 * verticalPadding <= above || above >= below;
        const auto availableHeight = useAbove ? above : below;
        const auto availableWidth = (std::min)(640.f, viewport.width - 2 * margin);
        if (availableWidth <= 2 * horizontalPadding || availableHeight <= 2 * verticalPadding) return std::nullopt;
        auto scale = (std::min)({1.f, (availableWidth - 2 * horizontalPadding) / content.width,
            (availableHeight - 2 * verticalPadding) / content.height});
        auto width = content.width * scale + 2 * horizontalPadding;
        auto height = content.height * scale + 2 * verticalPadding;
        auto left = std::clamp(anchorX - horizontalPadding, margin, (std::max)(margin, viewport.width - margin - width));
        auto top = useAbove ? caret.top - gap - height : caret.bottom + gap;
        return EditorMathPreviewPlacement{D2D1::RectF(left, top, left + width, top + height),
            D2D1::Point2F(left + horizontalPadding, top + verticalPadding), scale};
    }

    std::optional<D2D1_RECT_F> PaintInlineMathPreview(
        EditorPreparedMathPreview const& preview, D2D1_RECT_F caret, float anchorX,
        EditorRenderResources& resources, EditorStyleSheet const& styleSheet,
        EditorDrawMath const& drawMath, EditorDrawMathFallback const& drawFallback) {
        DWRITE_TEXT_METRICS metrics{};
        if (!preview.layout || FAILED(preview.layout->GetMetrics(&metrics))) return std::nullopt;
        auto placement = PlaceInlineMathPreview(caret, anchorX,
            D2D1::SizeF((std::max)(1.f, metrics.widthIncludingTrailingWhitespace), preview.height),
            D2D1::SizeF(resources.surfaceWidthDip, resources.surfaceHeightDip));
        if (!placement) return std::nullopt;
        auto const& box = *placement;
        // A few translucent contours make a soft, small shadow without an
        // intermediate bitmap or a new GPU effect for each edited character.
        resources.previewShadowBrush->SetOpacity(.018f);
        for (int ring = 6; ring >= 1; --ring) {
            const float spread = static_cast<float>(ring);
            resources.d2dContext->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(
                box.bounds.left - spread, box.bounds.top - spread + 2,
                box.bounds.right + spread, box.bounds.bottom + spread + 2), 7 + spread, 7 + spread),
                resources.previewShadowBrush.Get());
        }
        resources.d2dContext->FillRoundedRectangle(D2D1::RoundedRect(box.bounds, 7, 7), resources.previewBackgroundBrush.Get());
        resources.d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(box.bounds, 7, 7), resources.previewBorderBrush.Get(), 1);
        D2D1_MATRIX_3X2_F transform;
        resources.d2dContext->GetTransform(&transform);
        resources.d2dContext->SetTransform(D2D1::Matrix3x2F::Scale(box.scale, box.scale)
            * D2D1::Matrix3x2F::Translation(box.origin.x, box.origin.y) * transform);
        resources.d2dContext->DrawTextLayout(D2D1::Point2F(0, 0), preview.layout.Get(), resources.textBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        resources.d2dContext->SetTransform(transform);
        for (auto const& positioned : EditorDocumentPainter::PositionMath(preview.layout.Get(), preview.display.mathOverlays, metrics.layoutWidth)) {
            auto fragment = positioned.overlay->fragment;
            fragment.width *= box.scale;
            fragment.height *= box.scale;
            auto origin = D2D1::Point2F(box.origin.x + positioned.localX * box.scale, box.origin.y + positioned.localTop * box.scale);
            if (!drawMath(fragment, origin, styleSheet.textColor)) drawFallback(positioned.overlay->sourceSpan, origin);
        }
        return box.bounds;
    }
}
