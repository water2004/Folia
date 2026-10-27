#include "pch.h"
import std;
#include "editor/rendering/EditorMathPreview.h"
#include "editor/rendering/EditorDocumentPainter.h"
#include "editor/rendering/EditorTextLayoutEngine.h"

namespace winrt::Folia {
    std::optional<EditorMathPreviewPlacement> PlaceInlineMathPreview(
        D2D1_RECT_F caret, float anchorX, D2D1_SIZE_F content, D2D1_SIZE_F viewport,
        folia::ThemeMathPreview const& style) {
        const auto margin = style.margin, gap = style.gap;
        const auto horizontalPadding = style.padding_horizontal, verticalPadding = style.padding_vertical;
        if (!std::isfinite(caret.left) || !std::isfinite(caret.top) || !std::isfinite(caret.right) || !std::isfinite(caret.bottom)
            || !std::isfinite(anchorX) || !std::isfinite(content.width) || !std::isfinite(content.height) || content.width <= 0 || content.height <= 0
            || !std::isfinite(viewport.width) || !std::isfinite(viewport.height)
            || caret.bottom <= margin || caret.top >= viewport.height - margin
            || caret.right < 0 || caret.left > viewport.width) return std::nullopt;
        const float above = caret.top - margin - gap, below = viewport.height - margin - caret.bottom - gap;
        const bool useAbove = content.height + 2 * verticalPadding <= above || above >= below;
        const auto availableHeight = useAbove ? above : below;
        const auto availableWidth = viewport.width - 2 * margin;
        if (availableWidth <= 2 * horizontalPadding || availableHeight <= 2 * verticalPadding) return std::nullopt;
        auto width = (std::min)(content.width + 2 * horizontalPadding, availableWidth);
        auto height = (std::min)(content.height + 2 * verticalPadding, availableHeight);
        auto left = std::clamp(anchorX - horizontalPadding, margin, (std::max)(margin, viewport.width - margin - width));
        auto top = useAbove ? caret.top - gap - height : caret.bottom + gap;
        return EditorMathPreviewPlacement{D2D1::RectF(left, top, left + width, top + height),
            D2D1::Point2F(left + horizontalPadding, top + verticalPadding)};
    }

    EditorPreparedMathPreview PrepareMathPreview(DisplayInlineText::MathPreview const& input,
        EditorTextLayoutEngine& engine, IDWriteTextFormat* format, float width, float viewportWidth,
        folia::ThemeMathPreview const& style) {
        EditorPreparedMathPreview preview;
        preview.display = BuildMathPreviewText(input);
        auto available = input.svg.display ? width
            : (std::max)(1.f, viewportWidth - 2 * (style.margin + style.padding_horizontal));
        preview.layout = engine.CreateFlow(preview.display, format, available, {});
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

    void PaintMathPreviewContent(EditorPreparedMathPreview const& preview, D2D1_POINT_2F origin,
        EditorRenderResources& resources, EditorStyleSheet const& styleSheet,
        EditorDrawMath const& drawMath, EditorDrawMathFallback const& drawFallback) {
        if (!preview.layout) return;
        auto const& content = preview.contentBounds;
        auto at = D2D1::Point2F(origin.x - content.left, origin.y - content.top);
        resources.d2dContext->DrawTextLayout(at, preview.layout.Get(), resources.textBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        EditorDocumentPainter::PaintMath(preview.layout.Get(), at, preview.display.mathOverlays,
            preview.layout->GetMaxWidth(), resources, drawMath, drawFallback);
    }

    std::optional<D2D1_RECT_F> PaintInlineMathPreview(
        EditorPreparedMathPreview const& preview, D2D1_RECT_F caret, float anchorX,
        EditorRenderResources& resources, EditorStyleSheet const& styleSheet,
        EditorDrawMath const& drawMath, EditorDrawMathFallback const& drawFallback) {
        auto placement = PlaceInlineMathPreview(caret, anchorX, D2D1::SizeF(preview.Width(), preview.Height()),
            D2D1::SizeF(resources.surfaceWidthDip, resources.surfaceHeightDip), styleSheet.mathPreview);
        if (!placement) return std::nullopt;
        auto const& box = *placement;
        auto const& style = styleSheet.mathPreview;
        resources.popupShadow.Draw(resources.d2dContext.Get(), box.bounds, style);
        auto card = D2D1::RoundedRect(box.bounds, style.corner_radius, style.corner_radius);
        resources.d2dContext->FillRoundedRectangle(card, resources.previewBackgroundBrush.Get());
        resources.d2dContext->DrawRoundedRectangle(card, resources.previewBorderBrush.Get(), 1);
        resources.d2dContext->PushAxisAlignedClip(D2D1::RectF(
            box.bounds.left + style.padding_horizontal, box.bounds.top + style.padding_vertical,
            box.bounds.right - style.padding_horizontal, box.bounds.bottom - style.padding_vertical), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        PaintMathPreviewContent(preview, box.origin, resources, styleSheet, drawMath, drawFallback);
        resources.d2dContext->PopAxisAlignedClip();
        return box.bounds;
    }
}
