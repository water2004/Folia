#include "pch.h"
import std;
#include "editor/rendering/EditorMathPreview.h"
#include "editor/rendering/EditorDocumentPainter.h"
#include "editor/rendering/EditorTextLayoutEngine.h"

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

    EditorPreparedMathPreview PrepareMathPreview(DisplayInlineText::MathPreview const& input,
        EditorTextLayoutEngine& engine, IDWriteTextFormat* format, float width) {
        EditorPreparedMathPreview preview;
        preview.display = BuildMathPreviewText(input);
        preview.layout = engine.CreateFlow(preview.display, format, width, {});
        preview.contentSpan = input.contentSpan;
        preview.floating = !input.svg.display;
        DWRITE_TEXT_METRICS metrics{};
        if (!preview.layout || FAILED(preview.layout->GetMetrics(&metrics))) return preview;
        auto positionedMath = EditorDocumentPainter::PositionMath(
            preview.layout.Get(), preview.display.mathOverlays, metrics.layoutWidth);
        if (positionedMath.empty()) return preview;
        auto content = D2D1::RectF(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (auto const& positioned : positionedMath) {
            auto const& fragment = positioned.overlay->fragment;
            content.left = (std::min)(content.left, positioned.localX);
            content.top = (std::min)(content.top, positioned.localTop);
            content.right = (std::max)(content.right, positioned.localX + fragment.width);
            content.bottom = (std::max)(content.bottom, positioned.localTop + fragment.height);
        }
        // Both floating and block previews surround the SVGs, not DirectWrite's
        // font leading. Keep the fragment positions and line breaks intact.
        preview.contentBounds = content;
        return preview;
    }

    void PaintMathPreviewContent(EditorPreparedMathPreview const& preview, D2D1_POINT_2F origin, float scale,
        EditorRenderResources& resources, EditorStyleSheet const& styleSheet,
        EditorDrawMath const& drawMath, EditorDrawMathFallback const& drawFallback) {
        if (!preview.layout) return;
        auto const& content = preview.contentBounds;
        D2D1_MATRIX_3X2_F transform;
        resources.d2dContext->GetTransform(&transform);
        resources.d2dContext->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale)
            * D2D1::Matrix3x2F::Translation(origin.x - content.left * scale,
                origin.y - content.top * scale) * transform);
        resources.d2dContext->DrawTextLayout(D2D1::Point2F(0, 0), preview.layout.Get(), resources.textBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        resources.d2dContext->SetTransform(transform);
        for (auto const& positioned : EditorDocumentPainter::PositionMath(
                preview.layout.Get(), preview.display.mathOverlays, preview.layout->GetMaxWidth())) {
            auto fragment = positioned.overlay->fragment;
            fragment.width *= scale;
            fragment.height *= scale;
            auto fragmentOrigin = D2D1::Point2F(origin.x + (positioned.localX - content.left) * scale,
                origin.y + (positioned.localTop - content.top) * scale);
            if (!drawMath(fragment, fragmentOrigin, styleSheet.textColor)) drawFallback(positioned.overlay->sourceSpan, fragmentOrigin);
        }
    }

    std::optional<D2D1_RECT_F> PaintInlineMathPreview(
        EditorPreparedMathPreview const& preview, D2D1_RECT_F caret, float anchorX,
        EditorRenderResources& resources, EditorStyleSheet const& styleSheet,
        EditorDrawMath const& drawMath, EditorDrawMathFallback const& drawFallback) {
        auto placement = PlaceInlineMathPreview(caret, anchorX, D2D1::SizeF(preview.Width(), preview.Height()),
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
        PaintMathPreviewContent(preview, box.origin, box.scale, resources, styleSheet, drawMath, drawFallback);
        return box.bounds;
    }
}
