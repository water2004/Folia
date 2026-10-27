#include "pch.h"
#include "editor/rendering/EditorDocumentRenderPass.h"
#include "editor/interaction/EditorTableInteraction.h"

namespace winrt::Folia
{
    EditorDocumentRenderPass::EditorDocumentRenderPass(
        EditorRenderResources& valueResources,
        EditorStyleSheet const& valueStyleSheet,
        EditorInteractionMap& valueInteractionMap,
        EditorCaretAnimation& valueCaretAnimation,
        std::vector<D2D1_RECT_F>& valueNonInteractiveRegions,
        EditorInlineImageRenderer& valueInlineImages,
        EditorDocumentPainter& valueDocumentPainter,
        EditorDrawMath valueDrawMath,
        EditorDrawMathFallback valueDrawMathFallback,
        EditorDrawMermaid valueDrawMermaid)
        : resources(valueResources),
          styleSheet(valueStyleSheet),
          interactionMap(valueInteractionMap),
          caretAnimation(valueCaretAnimation),
          nonInteractiveRegions(valueNonInteractiveRegions),
          inlineImages(valueInlineImages),
          documentPainter(valueDocumentPainter),
          drawMath(std::move(valueDrawMath)),
          drawMathFallback(std::move(valueDrawMathFallback)),
          drawMermaid(std::move(valueDrawMermaid))
    {
    }

    std::optional<D2D1_RECT_F> EditorDocumentRenderPass::Paint(
        detail::EditorRenderFrame const& frame,
        EditorPreparedDocument& preparedDocument,
        folia::TextSelection selection,
        folia::TextPosition caret,
        float documentLeft,
        float documentRight,
        float documentWidth,
        float sourceGutterLeft,
        float sourceGutterWidth,
        float scrollOffset,
        std::size_t viewportBegin,
        std::size_t viewportEnd,
        bool printMode,
        std::optional<D2D1_POINT_2F> pointerPosition,
        std::optional<EditorTableAction> const& draggedTableAction,
        std::optional<std::size_t> tableDropIndex)
    {
        if (sourceGutterWidth > 0.0f && resources.lineNumberBackgroundBrush)
        {
            resources.d2dContext->FillRectangle(
                D2D1::RectF(0.0f, 0.0f, sourceGutterWidth, resources.surfaceHeightDip),
                resources.lineNumberBackgroundBrush.Get());
        }

        EditorPreparedMathPreview const* floatingPreview = nullptr;
        for (auto blockIndex = viewportBegin; blockIndex < viewportEnd; ++blockIndex)
        {
            auto placement = preparedDocument.geometry.At(blockIndex);
            auto const& block = frame.renderModel.blocks[blockIndex];
            auto& prepared = preparedDocument.blocks[blockIndex];
            auto top = placement.top - scrollOffset;
            auto bottom = placement.bottom - scrollOffset;
            auto documentY = placement.top;
            if (block.kind == folia::RenderBlockKind::ThematicBreak)
            {
                auto center = (top + bottom) * 0.5f;
                resources.d2dContext->DrawLine(
                    D2D1::Point2F(documentLeft, center),
                    D2D1::Point2F(documentRight, center),
                    resources.mutedBrush.Get(),
                    1.0f);
                EditorVisualBlock visual;
                visual.rect = D2D1::RectF(documentLeft, top, documentRight, bottom);
                visual.textOrigin = D2D1::Point2F(documentLeft, top);
                visual.textWidth = documentWidth;
                visual.sourceSpan = block.source_span;
                visual.documentY = documentY;
                visual.thematicBreak = true;
                interactionMap.blocks.push_back(visual);
                EditorVisualLine line;
                line.blockIndex = interactionMap.blocks.size() - 1;
                line.sourceSpans = {block.source_span};
                line.rect = visual.rect;
                interactionMap.lines.push_back(line);
                continue;
            }
            if (prepared.table)
            {
                EditorTableBlockRenderer::Paint(
                    block,
                    *prepared.table,
                    documentLeft,
                    top,
                    scrollOffset,
                    resources,
                    styleSheet,
                    interactionMap,
                    documentPainter,
                    inlineImages,
                    drawMath,
                    drawMathFallback,
                    nonInteractiveRegions,
                    floatingPreview);
                continue;
            }
            auto flowContainer = !block.inline_items.empty()
                && (block.kind == folia::RenderBlockKind::Quote
                    || block.kind == folia::RenderBlockKind::Callout);
            auto paddingTop = flowContainer ? 0.0f : block.block_style.padding_top;
            auto paddingLeft = flowContainer ? 0.0f : block.block_style.padding_left;
            auto paddingRight = flowContainer ? 0.0f : block.block_style.padding_right;
            auto contentWidth = (std::max)(1.0f, documentWidth - paddingLeft - paddingRight);
            auto origin = D2D1::Point2F(documentLeft + paddingLeft, top + paddingTop);
            auto rect = D2D1::RectF(documentLeft, top, documentRight, bottom);

            if (sourceGutterWidth > 0.0f
                && block.source_mode
                && block.source_line_number != 0
                && resources.lineNumberFormat
                && resources.lineNumberBrush)
            {
                auto number = std::to_wstring(block.source_line_number);
                resources.d2dContext->DrawTextW(
                    number.c_str(),
                    static_cast<UINT32>(number.size()),
                    resources.lineNumberFormat.Get(),
                    D2D1::RectF(
                        sourceGutterLeft,
                        origin.y,
                        sourceGutterWidth - 5.0f,
                        origin.y + styleSheet.code.lineHeight),
                    resources.lineNumberBrush.Get(),
                    D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
            if (!prepared.code
                && block.kind != folia::RenderBlockKind::Math
                && block.block_style.background)
                resources.d2dContext->FillRectangle(rect, resources.panelBrush.Get());
            documentPainter.DrawFlowDecorations(prepared.layout.Get(), origin, block, prepared.display);
            documentPainter.DrawInlineText(prepared.layout.Get(), origin, prepared.display, prepared.display.displayToSource,
                prepared.code ? resources.codeBrush.Get() : resources.textBrush.Get());
            documentPainter.DrawTaskCheckboxes(
                prepared.layout.Get(), origin, prepared.display.taskCheckboxOverlays);
            documentPainter.RegisterFootnotes(
                prepared.layout.Get(), origin, prepared.display.footnoteOverlays);
            inlineImages.Draw(prepared.layout.Get(), origin, prepared.images);
            if (!prepared.mermaids.empty() && prepared.layout)
            {
                for (auto const& diagram : prepared.mermaids)
                {
                    float pointX = 0.0f;
                    float lineTop = 0.0f;
                    DWRITE_HIT_TEST_METRICS hit{};
                    if (FAILED(prepared.layout->HitTestTextPosition(
                            diagram.displayStart,
                            FALSE,
                            &pointX,
                            &lineTop,
                            &hit)))
                        continue;
                    auto mermaidOrigin = D2D1::Point2F(
                        origin.x
                            + pointX
                            + (diagram.advance - diagram.width) * 0.5f,
                        origin.y
                            + lineTop
                            + (diagram.lineHeight - diagram.height));
                    drawMermaid(
                        diagram.svg,
                        diagram.width,
                        diagram.height,
                        mermaidOrigin);
                }
            }
            for (auto const& positioned : EditorDocumentPainter::PaintMath(
                    prepared.layout.Get(),
                    origin,
                    prepared.display.mathOverlays,
                    contentWidth, resources, drawMath, drawMathFallback))
            {
                auto const& overlay = *positioned.overlay;
                auto mathOrigin = D2D1::Point2F(
                    origin.x + positioned.localX,
                    origin.y + positioned.localTop);
                interactionMap.mathHits.push_back({
                    D2D1::RectF(
                        mathOrigin.x,
                        mathOrigin.y,
                        mathOrigin.x + overlay.fragment.width,
                        mathOrigin.y + overlay.fragment.height),
                    overlay.sourceSpan,
                    overlay.progressStart,
                    overlay.progressEnd,
                });
            }
            auto previewTop = origin.y + prepared.textHeight;
            for (auto const& preview : prepared.mathPreviews)
            {
                if (preview.floating) { floatingPreview = &preview; continue; }
                auto nonInteractiveTop = previewTop;
                previewTop += 8.0f;
                auto previewRect = D2D1::RectF(
                    documentLeft + paddingLeft,
                    previewTop,
                    documentLeft + paddingLeft + contentWidth,
                    previewTop + preview.Height() + 16.0f);
                resources.d2dContext->FillRectangle(previewRect, resources.nestedQuoteBrush.Get());
                auto previewOrigin = D2D1::Point2F(
                    previewRect.left + (contentWidth - preview.Width()) * .5f, previewRect.top + 8.0f);
                PaintMathPreviewContent(preview, previewOrigin, resources, styleSheet, drawMath, drawMathFallback);
                nonInteractiveRegions.push_back(D2D1::RectF(
                    previewRect.left,
                    nonInteractiveTop,
                    previewRect.right,
                    previewRect.bottom));
                previewTop = previewRect.bottom;
            }
            for (auto const& preview : prepared.mermaidPreviews)
            {
                auto nonInteractiveTop = previewTop;
                previewTop += 8.0f;
                auto previewRect = D2D1::RectF(
                    documentLeft + paddingLeft,
                    previewTop,
                    documentLeft + paddingLeft + contentWidth,
                    previewTop + preview.height + 16.0f);
                resources.d2dContext->FillRectangle(
                    previewRect,
                    resources.nestedQuoteBrush.Get());
                auto previewOrigin = D2D1::Point2F(
                    previewRect.left
                        + (previewRect.right - previewRect.left
                            - preview.width) * 0.5f,
                    previewRect.top + 8.0f);
                drawMermaid(
                    preview.svg,
                    preview.width,
                    preview.height,
                    previewOrigin);
                nonInteractiveRegions.push_back(D2D1::RectF(
                    previewRect.left,
                    nonInteractiveTop,
                    previewRect.right,
                    previewRect.bottom));
                previewTop = previewRect.bottom;
            }
            EditorVisualBlock visual;
            visual.rect = rect;
            visual.textOrigin = origin;
            visual.textWidth = contentWidth;
            visual.sourceSpan = block.source_span;
            visual.documentY = documentY;
            visual.text = prepared.display.text;
            visual.displayToSource = prepared.display.displayToSource;
            visual.layout = prepared.layout;
            interactionMap.blocks.push_back(std::move(visual));
            interactionMap.AddBlockLines(interactionMap.blocks.size() - 1);
        }

        EditorTableInteraction::Paint(
            resources,
            interactionMap,
            printMode ? std::nullopt : pointerPosition,
            draggedTableAction,
            tableDropIndex);
        if (!printMode) DrawSnippetPlaceholders(frame.snippetPlaceholders);
        if (!printMode)
        {
            auto rect = selection.is_caret() ? interactionMap.CaretBounds(caret, styleSheet.body.lineHeight) : std::nullopt;
            auto visual = caretAnimation.Sample(rect, caret, scrollOffset, EditorCaretAnimation::Now(), frame.renderModel.revision);
            if (visual.opacity > 0) {
                resources.caretBrush->SetOpacity(visual.opacity);
                resources.d2dContext->DrawLine(
                    D2D1::Point2F(visual.bounds.left, visual.bounds.top),
                    D2D1::Point2F(visual.bounds.left, visual.bounds.bottom),
                    resources.caretBrush.Get(),
                    styleSheet.editor.caret_width);
                resources.caretBrush->SetOpacity(1);
            }
        }
        // Paint after every document block: the preview never enters geometry,
        // line-breaking, selection mapping or table-row height calculation.
        if (!printMode && floatingPreview) if (auto anchor = interactionMap.CaretBounds(caret, styleSheet.body.lineHeight)) {
            auto anchorX = anchor->left;
            for (auto const& range : interactionMap.TextRangeBounds(floatingPreview->contentSpan))
                if (range.top <= anchor->top + 1 && range.bottom >= anchor->bottom - 1) { anchorX = range.left; break; }
            if (auto bounds = PaintInlineMathPreview(*floatingPreview, *anchor, anchorX, resources, styleSheet, drawMath, drawMathFallback)) {
                nonInteractiveRegions.push_back(*bounds);
                return bounds;
            }
        }
        return std::nullopt;
    }

    void EditorDocumentRenderPass::DrawSnippetPlaceholders(
        std::span<const folia::platform::editor::EditorSnippetPlaceholder> placeholders)
    {
        for (auto const& placeholder : placeholders)
        {
            auto brush = placeholder.current
                ? resources.accentBrush.Get()
                : resources.mutedBrush.Get();
            auto strokeWidth = placeholder.current ? 1.5f : 1.0f;
            if (placeholder.range.empty())
            {
                auto position = folia::TextPosition{
                    placeholder.container,
                    placeholder.range.start,
                    folia::TextAffinity::Downstream};
                auto caret = interactionMap.CaretBounds(position, styleSheet.body.lineHeight);
                if (!caret) continue;
                auto top = caret->top + 2.0f;
                auto bottom = caret->bottom - 2.0f;
                auto x = caret->left;
                resources.d2dContext->DrawLine(
                    D2D1::Point2F(x, top),
                    D2D1::Point2F(x, bottom),
                    brush,
                    strokeWidth);
                resources.d2dContext->DrawLine(
                    D2D1::Point2F(x - 2.5f, top),
                    D2D1::Point2F(x + 2.5f, top),
                    brush,
                    strokeWidth);
                resources.d2dContext->DrawLine(
                    D2D1::Point2F(x - 2.5f, bottom),
                    D2D1::Point2F(x + 2.5f, bottom),
                    brush,
                    strokeWidth);
                continue;
            }

            auto span = folia::TextSpan{placeholder.container, placeholder.range};
            for (auto rect : interactionMap.TextRangeBounds(span))
            {
                rect.left -= 1.5f;
                rect.top += 1.0f;
                rect.right += 1.5f;
                rect.bottom -= 1.0f;
                resources.d2dContext->DrawRoundedRectangle(
                    D2D1::RoundedRect(rect, 2.5f, 2.5f),
                    brush,
                    strokeWidth);
            }
        }
    }
}
