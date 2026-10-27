#include "pch.h"

import folia.core.render_model;
import folia.core.types;

#include "editor/rendering/EditorContentPreparation.h"
#include "editor/rendering/EditorContentGeometry.h"
#include "editor/rendering/EditorDocumentPainter.h"
#include "editor/rendering/EditorTableBlockRenderer.h"

namespace winrt::Folia
{
    std::optional<EditorTableBlockRenderer::PreparedTable> EditorTableBlockRenderer::Prepare(
        folia::RenderBlock const& block,
        folia::TextPosition caret,
        float tableWidth,
        bool svgSupported,
        bool requestEmbedded,
        bool highPriority,
        EditorRenderResources& resources,
        EditorStyleSheet const& styleSheet,
        EditorTextLayoutEngine& textLayoutEngine,
        EditorInlineImageRenderer& inlineImageRenderer,
        MathJaxRenderer& mathJax,
        SvgNormalizer& svgNormalizer,
        TreeSitterHighlighter& treeSitter)
    {
        auto const& special = block.special();
        auto modeledTable = special.row_count > 0 && special.column_count > 0 && !special.table_cells.empty();
        if (!modeledTable) return std::nullopt;
        PreparedTable prepared;
        auto& table = prepared.visual;
        table.sourceSpans = special.table_cell_spans;
        table.editable = true;
        table.columnCount = special.column_count;
        table.rowCount = special.row_count;
        tableWidth = (std::max)(1.0f, tableWidth);
        auto columnWidth = tableWidth / static_cast<float>(table.columnCount);
        std::vector<float> rowHeights(table.rowCount, styleSheet.body.lineHeight + 16.0f);
        prepared.displays.reserve(table.rowCount * table.columnCount);
        prepared.imageDraws.reserve(table.rowCount * table.columnCount);
        prepared.mathPreviews.reserve(table.rowCount * table.columnCount);
        prepared.textHeights.reserve(table.rowCount * table.columnCount);
        for (std::size_t row = 0; row < table.rowCount; ++row)
        {
            for (std::size_t column = 0; column < table.columnCount; ++column)
            {
                auto sourceSpan = folia::TextSpan{block.id, {0, 0}};
                auto rangeIndex = row * table.columnCount + column;
                if (rangeIndex < special.table_cell_spans.size())
                {
                    sourceSpan = special.table_cell_spans[rangeIndex];
                }
                DisplayInlineText display;
                auto renderCellIndex = row * table.columnCount + column;
                if (renderCellIndex < special.table_cells.size())
                {
                    display = BuildDisplayInlineText(special.table_cells[renderCellIndex], caret, {sourceSpan.container_id, sourceSpan.source_range.end, folia::TextAffinity::Downstream}, mathJax, svgNormalizer, styleSheet.textColor, styleSheet.body.size, EditorTableCellContentWidth(tableWidth, table.columnCount), svgSupported, requestEmbedded, highPriority);
                }
                else
                {
                    AppendGeneratedText(display, U"\u200B", {sourceSpan.container_id, sourceSpan.source_range.start, folia::TextAffinity::Downstream}, folia::InlineStyle::plain());
                    display.displayToSource.push_back({sourceSpan.container_id, sourceSpan.source_range.end, folia::TextAffinity::Downstream});
                }
                HighlightMathSources(display, caret, treeSitter);
                auto wide = ToWide(display.text);
                auto layout = textLayoutEngine.Create(wide, resources.textFormat.Get(), EditorTableCellContentWidth(tableWidth, table.columnCount));
                auto cellTextHeight = styleSheet.body.lineHeight;
                if (layout)
                {
                    layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                    textLayoutEngine.ApplyStyles(layout.Get(), display.ranges);
                    ApplyMathInlineObjects(layout.Get(), display.mathOverlays);
                    if (row == 0 && special.table_header_row) layout->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_RANGE{0, static_cast<UINT32>(wide.size())});
                    auto alignment = column < special.table_aligns.size() ? special.table_aligns[column] : folia::TableAlignment::None;
                    if (alignment == folia::TableAlignment::Center) layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    else if (alignment == folia::TableAlignment::Right) layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                    else layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                    DWRITE_TEXT_METRICS metrics{};
                    if (SUCCEEDED(layout->GetMetrics(&metrics)))
                    {
                        cellTextHeight = metrics.height;
                    }
                }
                std::vector<PreparedTable::MathPreview> previews;
                auto previewHeight = 0.0f;
                previews.reserve(display.mathPreviews.size());
                for (auto const& preview : display.mathPreviews)
                {
                    auto preparedPreview = PrepareMathPreview(preview, textLayoutEngine,
                        resources.textFormat.Get(), (std::max)(1.0f, columnWidth - 36.0f),
                        resources.surfaceWidthDip, styleSheet.mathPreview);
                    if (!preparedPreview.floating) previewHeight += preparedPreview.Height() + 24.0f;
                    previews.push_back(std::move(preparedPreview));
                }
                rowHeights[row] = (std::max)(
                    rowHeights[row],
                    cellTextHeight + previewHeight + 16.0f);
                auto images = inlineImageRenderer.Resolve(
                    layout.Get(),
                    display.imageOverlays,
                    EditorTableCellContentWidth(tableWidth, table.columnCount),
                    requestEmbedded);
                prepared.pendingImage = prepared.pendingImage || std::ranges::any_of(
                    images,
                    [](auto const& image) { return image.pending; });
                EditorVisualTableCell cell;
                cell.sourceSpan = sourceSpan;
                cell.row = row;
                cell.column = column;
                cell.text = std::move(display.text);
                cell.displayToSource = std::move(display.displayToSource);
                cell.textHeight = cellTextHeight + previewHeight;
                cell.layout = std::move(layout);
                prepared.pendingMath = prepared.pendingMath || display.pendingMath;
                prepared.displays.push_back(std::move(display));
                prepared.imageDraws.push_back(std::move(images));
                prepared.mathPreviews.push_back(std::move(previews));
                prepared.textHeights.push_back(cellTextHeight);
                table.cells.push_back(std::move(cell));
            }
        }
        table.columnBoundaries.reserve(table.columnCount + 1);
        for (std::size_t column = 0; column <= table.columnCount; ++column) table.columnBoundaries.push_back(columnWidth * static_cast<float>(column));
        table.rowBoundaries.reserve(table.rowCount + 1);
        table.rowBoundaries.push_back(0.0f);
        for (auto rowHeight : rowHeights) table.rowBoundaries.push_back(table.rowBoundaries.back() + rowHeight);
        table.rect = D2D1::RectF(0.0f, 0.0f, tableWidth, table.rowBoundaries.back());
        for (auto& cell : table.cells)
        {
            cell.rect = D2D1::RectF(table.columnBoundaries[cell.column], table.rowBoundaries[cell.row], table.columnBoundaries[cell.column + 1], table.rowBoundaries[cell.row + 1]);
            auto verticalInset = (std::max)(0.0f, (cell.rect.bottom - cell.rect.top - cell.textHeight) * 0.5f);
            cell.textOrigin = D2D1::Point2F(cell.rect.left + 10.0f, cell.rect.top + verticalInset);
            cell.textWidth = (std::max)(1.0f, cell.rect.right - cell.rect.left - 20.0f);
        }
        prepared.height = table.rect.bottom;
        return prepared;
    }

    void EditorTableBlockRenderer::Paint(
        folia::RenderBlock const& block,
        PreparedTable const& prepared,
        float documentLeft,
        float top,
        float scrollOffset,
        EditorRenderResources& resources,
        EditorStyleSheet const& styleSheet,
        EditorInteractionMap& interactionMap,
        EditorDocumentPainter& documentPainter,
        EditorInlineImageRenderer& inlineImageRenderer,
        EditorDrawMath const& drawMath,
        EditorDrawMathFallback const& drawMathFallback,
        std::vector<D2D1_RECT_F>& nonInteractiveRegions,
        EditorPreparedMathPreview const*& floatingPreview)
    {
        auto translatedTable = prepared.visual;
        translatedTable.rect.left += documentLeft;
        translatedTable.rect.right += documentLeft;
        translatedTable.rect.top += top;
        translatedTable.rect.bottom += top;
        for (auto& boundary : translatedTable.columnBoundaries) boundary += documentLeft;
        for (auto& boundary : translatedTable.rowBoundaries) boundary += top;
        for (auto& cell : translatedTable.cells)
        {
            cell.rect.left += documentLeft;
            cell.rect.right += documentLeft;
            cell.rect.top += top;
            cell.rect.bottom += top;
            cell.textOrigin.x += documentLeft;
            cell.textOrigin.y += top;
        }
        auto tableIndex = interactionMap.tables.size();
        interactionMap.tables.push_back(std::move(translatedTable));
        auto& visualTable = interactionMap.tables.back();
        EditorVisualBlock visualBlock;
        visualBlock.rect = visualTable.rect;
        visualBlock.sourceSpan = visualTable.sourceSpans.empty() ? block.source_span : visualTable.sourceSpans.front();
        visualBlock.documentY = top + scrollOffset;
        auto visualBlockIndex = interactionMap.blocks.size();
        interactionMap.blocks.push_back(std::move(visualBlock));
        resources.d2dContext->FillRectangle(D2D1::RectF(visualTable.rect.left, visualTable.rowBoundaries[0], visualTable.rect.right, visualTable.rowBoundaries[1]), resources.panelBrush.Get());
        for (std::size_t cellIndex = 0; cellIndex < visualTable.cells.size(); ++cellIndex)
        {
            auto& cell = visualTable.cells[cellIndex];
            auto const& display = prepared.displays[cellIndex];
            if (cell.layout)
            {
                documentPainter.DrawInlineText(cell.layout.Get(), cell.textOrigin, display, cell.displayToSource, resources.textBrush.Get());
                for (auto const& overlay : display.footnoteOverlays)
                {
                    UINT32 count = 0;
                    auto result = cell.layout->HitTestTextRange(
                        overlay.displayStart,
                        overlay.displayLength,
                        cell.textOrigin.x,
                        cell.textOrigin.y,
                        nullptr,
                        0,
                        &count);
                    if (result != E_NOT_SUFFICIENT_BUFFER || count == 0) continue;
                    std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
                    if (FAILED(cell.layout->HitTestTextRange(
                            overlay.displayStart,
                            overlay.displayLength,
                            cell.textOrigin.x,
                            cell.textOrigin.y,
                            metrics.data(),
                            count,
                            &count))) continue;
                    for (UINT32 index = 0; index < count; ++index)
                    {
                        auto const& metric = metrics[index];
                        interactionMap.footnoteHits.push_back({
                            D2D1::RectF(
                                metric.left,
                                metric.top,
                                metric.left + metric.width,
                                metric.top + metric.height),
                            overlay.sourceSpan,
                            overlay.label,
                            overlay.kind});
                    }
                }
                inlineImageRenderer.Draw(cell.layout.Get(), cell.textOrigin, prepared.imageDraws[cellIndex]);
                for (auto const& positioned : EditorDocumentPainter::PaintMath(cell.layout.Get(), cell.textOrigin,
                        display.mathOverlays, cell.layout->GetMaxWidth(), resources, drawMath, drawMathFallback))
                {
                    auto const& overlay = *positioned.overlay;
                    auto mathY = cell.textOrigin.y + positioned.localTop;
                    auto mathX = cell.textOrigin.x + positioned.localX;
                    interactionMap.mathHits.push_back(EditorVisualMathHit{D2D1::RectF(mathX, mathY, mathX + overlay.fragment.width, mathY + overlay.fragment.height), overlay.sourceSpan, overlay.progressStart, overlay.progressEnd});
                }
            }
            if (cellIndex < prepared.mathPreviews.size()
                && cellIndex < prepared.textHeights.size())
            {
                auto previewTop = cell.textOrigin.y + prepared.textHeights[cellIndex];
                for (auto const& preview : prepared.mathPreviews[cellIndex])
                {
                    if (preview.floating) { floatingPreview = &preview; continue; }
                    auto nonInteractiveTop = previewTop;
                    previewTop += 8.0f;
                    auto previewRect = D2D1::RectF(
                        cell.rect.left + 6.0f,
                        previewTop,
                        cell.rect.right - 6.0f,
                        previewTop + preview.Height() + 16.0f);
                    resources.d2dContext->FillRectangle(previewRect, resources.nestedQuoteBrush.Get());
                    auto previewOrigin = D2D1::Point2F(
                        (previewRect.left + previewRect.right - preview.Width()) * .5f, previewRect.top + 8.0f);
                    PaintMathPreviewContent(preview, previewOrigin, resources, styleSheet, drawMath, drawMathFallback);
                    nonInteractiveRegions.push_back(D2D1::RectF(
                        previewRect.left,
                        nonInteractiveTop,
                        previewRect.right,
                        previewRect.bottom));
                    previewTop = previewRect.bottom;
                }
            }
            interactionMap.AddTableCellLines(visualBlockIndex, tableIndex, cellIndex);
        }
        for (auto boundary : visualTable.columnBoundaries) resources.d2dContext->DrawLine(D2D1::Point2F(boundary, visualTable.rect.top), D2D1::Point2F(boundary, visualTable.rect.bottom), resources.mutedBrush.Get(), 1.0f);
        for (auto boundary : visualTable.rowBoundaries) resources.d2dContext->DrawLine(D2D1::Point2F(visualTable.rect.left, boundary), D2D1::Point2F(visualTable.rect.right, boundary), resources.mutedBrush.Get(), 1.0f);
    }
}
