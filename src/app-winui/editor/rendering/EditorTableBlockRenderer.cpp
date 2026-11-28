#include "pch.h"

import folia.core.render_model;
import folia.core.types;
import folia.markdown.table_metadata;
import folia.core.table_values;
import folia.platform.editor_display_mapping;

#include "editor/rendering/layout/EditorContentPreparation.h"
#include "editor/rendering/EditorContentGeometry.h"
#include "editor/rendering/EditorCaretPresentation.h"
#include "editor/rendering/painting/EditorDocumentPainter.h"
#include "editor/rendering/EditorTableBlockRenderer.h"
#include "localization/Localization.h"

namespace winrt::Folia
{
    namespace
    {
        void PaintValueBadge(EditorRenderResources& resources, EditorStyleSheet const& style,
            const EditorVisualTableCell& cell, const DisplayInlineText& display, const folia::TableMetadata& metadata)
        {
            if (cell.column >= metadata.column_types.size()) return;
            const auto kind = metadata.column_types[cell.column].kind;
            auto round = [&](D2D1_RECT_F bounds, D2D1_COLOR_F color) {
                resources.tableBrush->SetColor(color);
                resources.d2dContext->FillRoundedRectangle(D2D1::RoundedRect(bounds, 4, 4), resources.tableBrush.Get());
            };
            if (kind == folia::TableColumnKind::Progress) {
                if (auto fraction = folia::table_progress(display.text)) {
                    const auto center = std::midpoint(cell.rect.top, cell.rect.bottom);
                    const auto left = cell.textOrigin.x;
                    auto bar = D2D1::RectF(left, center - 8, left + cell.textWidth, center + 8);
                    auto color = style.accentColor;
                    color.a *= 0.10f;
                    round(bar, color);
                    bar.right = left + cell.textWidth * static_cast<float>(*fraction);
                    color.a *= 2.6f;
                    round(bar, color);
                }
                return;
            }
            if ((kind != folia::TableColumnKind::Status && kind != folia::TableColumnKind::Multi)
                || cell.column >= metadata.options.size()) return;
            auto tags = kind == folia::TableColumnKind::Multi ? folia::table_tags(display.text)
                : std::vector<std::u32string_view>{folia::trim_table_value(display.text)};
            const auto countUtf16 = [](std::u32string_view view) {
                return static_cast<UINT32>(view.size() + std::ranges::count_if(view, [](char32_t cp) { return cp > 0xffff; }));
            };
            auto const& options = metadata.options[cell.column];
            for (auto tag : tags) {
                auto value = options.find(tag);
                if (tag.empty() || value == options.end()) continue;
                const auto prefix = static_cast<std::size_t>(tag.data() - display.text.data());
                const auto start = countUtf16(std::u32string_view(display.text).substr(0, prefix));
                const auto end = start + countUtf16(tag) - 1;
                DWRITE_HIT_TEST_METRICS first{}, last{};
                float x1{}, x2{}, y1{}, y2{};
                if (FAILED(cell.layout->HitTestTextPosition(start, FALSE, &x1, &y1, &first))
                    || FAILED(cell.layout->HitTestTextPosition(end, TRUE, &x2, &y2, &last))
                    || std::fabs(y1 - y2) > 0.5f) continue;
                auto color = style.tablePalette[static_cast<std::size_t>(value->second)];
                color.a *= 0.19f;
                round(D2D1::RectF(cell.textOrigin.x + x1 - 3, cell.textOrigin.y + y1 + 1,
                    cell.textOrigin.x + x2 + 3, cell.textOrigin.y + y1 + first.height - 1), color);
            }
        }

        struct TableSelectionGeometry
        {
            const EditorVisualTable& table;
            D2D1_RECT_F local;

            D2D1_RECT_F Bounds(bool frozen, bool sticky) const
            {
                const auto x = table.rect.left - (frozen ? 0.0f : table.horizontalOffset);
                const auto y = table.rect.top + (sticky ? table.stickyOffset : 0.0f);
                return {local.left + x, local.top + y, local.right + x, local.bottom + y};
            }

            D2D1_RECT_F Clip(bool frozen, bool sticky) const
            {
                auto clip = table.rect;
                if (table.frozenWidth > 0.0f)
                    (frozen ? clip.right : clip.left) = table.rect.left + table.frozenWidth;
                if (table.stickyOffset > 0.0f)
                    (sticky ? clip.bottom : clip.top) = table.rowBoundaries[table.headerRows] + table.stickyOffset;
                if (sticky) clip.top += table.stickyOffset;
                return clip;
            }

            std::optional<D2D1_POINT_2F> Handle() const
            {
                const auto frozen = table.frozenWidth > 0.0f && local.right <= table.columnBoundaries.Local()[1];
                const auto sticky = table.stickyOffset > 0.0f && local.bottom <= table.rowBoundaries.Local()[table.headerRows];
                const auto bounds = Bounds(frozen, sticky);
                const auto clip = Clip(frozen, sticky);
                if (bounds.right < clip.left || bounds.right > clip.right
                    || bounds.bottom < clip.top || bounds.bottom > clip.bottom) return std::nullopt;
                return D2D1::Point2F(bounds.right, bounds.bottom);
            }

            void PaintOutline(ID2D1DeviceContext& context, ID2D1Brush* brush) const
            {
                for (bool sticky : {false, true})
                for (bool frozen : {false, true})
                {
                    if ((sticky && table.stickyOffset == 0.0f) || (frozen && table.frozenWidth == 0.0f)) continue;
                    const auto clip = Clip(frozen, sticky);
                    if (clip.right <= clip.left || clip.bottom <= clip.top) continue;
                    context.PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_ALIASED);
                    context.DrawRectangle(Bounds(frozen, sticky), brush, 1.5f);
                    context.PopAxisAlignedClip();
                }
            }
        };
    }

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
        folia::platform::MathJaxRenderer& mathJax,
        folia::platform::SvgNormalizer& svgNormalizer,
        TreeSitterHighlighter& treeSitter,
        std::optional<detail::EditorCompletionPreview> completion,
        PreparedTable* previous,
        std::uint64_t dependencyKey,
        folia::NodeId editingOwner)
    {
        auto const& special = block.special();
        auto modeledTable = special.row_count > 0 && special.column_count > 0 && !special.table_cells.empty();
        if (!modeledTable) return std::nullopt;
        PreparedTable prepared;
        prepared.metadata = special.table_metadata;
        prepared.alignments = special.table_aligns;
        prepared.dependencyKey = dependencyKey;
        prepared.editingOwner = editingOwner;
        prepared.completionOwner = completion ? completion->position.container_id : folia::NodeId{};
        prepared.cellKeys = special.table_cell_keys;
        auto& table = prepared.visual;
        table.id = block.id;
        table.sourceSpans = std::make_shared<const std::vector<folia::TextSpan>>(special.table_cell_spans);
        table.editable = special.table_grid_editable;
        table.columnCount = special.column_count;
        table.rowCount = special.row_count;
        tableWidth = (std::max)(1.0f, tableWidth);
        table.viewportWidth = tableWidth;
        auto const* metadata = special.table_metadata.get();
        prepared.headerRows = special.table_header_row
            ? (std::min)(table.rowCount, metadata ? metadata->header_rows : std::size_t{1}) : 0;
        if (metadata && metadata->folded && prepared.headerRows > 0 && table.rowCount > prepared.headerRows)
        {
            const auto hiddenStart = prepared.headerRows * table.columnCount;
            const auto hidden = std::span<const folia::TextSpan>{special.table_cell_spans}.subspan(
                (std::min)(hiddenStart, special.table_cell_spans.size()));
            if (std::ranges::find(hidden, caret.container_id, &folia::TextSpan::container_id) == hidden.end())
            {
                table.hiddenRows = table.rowCount - prepared.headerRows;
                table.rowCount = prepared.headerRows;
            }
        }
        const auto horizontalPadding = EditorTableCellPadding(metadata);
        const auto verticalPadding = metadata && metadata->compact ? 3.0f : 7.0f;
        table.columnBoundaries = EditorTableColumnBoundaries(special, tableWidth);
        const bool reuseCellLayouts = previous && previous->visual.id == block.id
            && previous->visual.viewportWidth == tableWidth
            && previous->visual.columnCount == table.columnCount
            && previous->visual.rowCount == table.rowCount
            && previous->headerRows == prepared.headerRows
            && previous->metadata == prepared.metadata
            && previous->alignments == prepared.alignments
            && previous->dependencyKey == dependencyKey;
        auto merges = ResolveEditorTableMerges(metadata, table.rowCount, table.columnCount);
        std::vector<float> rowHeights(table.rowCount, styleSheet.body.lineHeight + 2 * verticalPadding);
        prepared.displays.reserve(table.rowCount * table.columnCount);
        prepared.imageDraws.reserve(table.rowCount * table.columnCount);
        prepared.mathPreviews.reserve(table.rowCount * table.columnCount);
        prepared.textHeights.reserve(table.rowCount * table.columnCount);
        table.cells.reserve(table.rowCount * table.columnCount);
        for (std::size_t row = 0; row < table.rowCount; ++row)
        {
            for (std::size_t column = 0; column < table.columnCount; ++column)
            {
                auto rangeIndex = row * table.columnCount + column;
                auto sourceSpan = rangeIndex < special.table_cell_spans.size()
                    ? special.table_cell_spans[rangeIndex] : folia::TextSpan{block.id, {0, 0}};
                auto span = merges.empty() ? EditorTableMerge{} : merges[rangeIndex];
                const auto owner = sourceSpan.container_id;
                if (reuseCellLayouts && span.rows == 1 && span.columns == 1
                    && rangeIndex < prepared.cellKeys.size() && rangeIndex < previous->cellKeys.size()
                    && prepared.cellKeys[rangeIndex] == previous->cellKeys[rangeIndex]
                    && owner != prepared.editingOwner && owner != previous->editingOwner
                    && owner != prepared.completionOwner && owner != previous->completionOwner
                    && previous->imageDraws[rangeIndex].empty()
                    && previous->displays[rangeIndex].imageOverlays.empty()
                    && !previous->displays[rangeIndex].pendingMath
                    && previous->displays[rangeIndex].mathOverlays.empty()
                    && previous->displays[rangeIndex].mathSources.empty())
                {
                    const auto height = previous->textHeights[rangeIndex];
                    auto& cached = previous->visual.cells[rangeIndex];
                    rowHeights[row] = (std::max)(rowHeights[row], cached.textHeight + 2 * verticalPadding);
                    table.cells.push_back(std::move(cached));
                    prepared.displays.push_back(std::move(previous->displays[rangeIndex]));
                    prepared.imageDraws.emplace_back();
                    prepared.mathPreviews.emplace_back();
                    prepared.textHeights.push_back(height);
                    continue;
                }
                EditorVisualTableCell cell;
                cell.sourceSpan = sourceSpan;
                cell.row = row;
                cell.column = column;
                cell.rowSpan = span.rows;
                cell.columnSpan = span.columns;
                cell.covered = span.rows == 0;
                if (cell.covered)
                {
                    table.cells.push_back(std::move(cell));
                    prepared.displays.emplace_back();
                    prepared.imageDraws.emplace_back();
                    prepared.mathPreviews.emplace_back();
                    prepared.textHeights.push_back(0);
                    continue;
                }
                auto columnWidth = table.columnBoundaries[column + span.columns] - table.columnBoundaries[column];
                auto contentWidth = EditorTableCellContentWidth(columnWidth, metadata);
                auto appearance = EditorTableCellAppearance(metadata, row, column);
                auto fontSize = EditorTableCellFontSize(styleSheet.body.size, metadata, appearance);
                auto foreground = appearance.foreground
                    ? styleSheet.tablePalette[static_cast<std::size_t>(*appearance.foreground)] : styleSheet.textColor;
                const auto header = row < prepared.headerRows || (metadata && metadata->header_column && column == 0);
                bool editing = caret.container_id == owner;
                DisplayInlineText display;
                if (rangeIndex < special.table_cells.size())
                {
                    if (span.rows > 1 || span.columns > 1)
                    {
                        for (auto mergedRow = row; mergedRow < row + span.rows; ++mergedRow)
                            for (auto mergedColumn = column; mergedColumn < column + span.columns; ++mergedColumn)
                            {
                                const auto index = mergedRow * table.columnCount + mergedColumn;
                                const auto& part = special.table_cell_spans[index];
                                editing |= caret.container_id == part.container_id;
                                if (part.source_range.empty()) continue;
                                if (!display.displayToSource.empty())
                                {
                                    const auto boundary = display.displayToSource.back();
                                    display.displayToSource.pop_back();
                                    AppendGeneratedText(display, U"\n", boundary, folia::InlineStyle::plain());
                                }
                                const folia::TextPosition end{part.container_id, part.source_range.end,
                                    folia::TextAffinity::Upstream};
                                auto content = BuildDisplayInlineText(special.table_cells[index], caret, end,
                                    mathJax, svgNormalizer, {styleSheet, foreground, fontSize}, contentWidth,
                                    svgSupported, requestEmbedded, highPriority);
                                MergeDisplayText(display, std::move(content));
                                display.displayToSource.push_back(end);
                            }
                    }
                    if (display.displayToSource.empty())
                        display = BuildDisplayInlineText(special.table_cells[rangeIndex], caret,
                            {sourceSpan.container_id, sourceSpan.source_range.end, folia::TextAffinity::Downstream},
                            mathJax, svgNormalizer, {styleSheet, foreground, fontSize}, contentWidth,
                            svgSupported, requestEmbedded, highPriority);
                }
                else
                {
                    AppendGeneratedText(display, U"\u200B", {sourceSpan.container_id, sourceSpan.source_range.start, folia::TextAffinity::Downstream}, folia::InlineStyle::plain());
                    display.displayToSource.push_back({sourceSpan.container_id, sourceSpan.source_range.end, folia::TextAffinity::Downstream});
                }
                HighlightMathSources(display, caret, treeSitter);
                if (completion) InsertCompletionPreview(display, completion->position, completion->text, completion->before);
                auto wide = ToWide(display.text);
                ++resources.tableTextLayoutsBuilt;
                auto layout = textLayoutEngine.Create(wide, resources.textFormat.Get(), contentWidth);
                auto cellTextHeight = styleSheet.body.lineHeight;
                if (layout)
                {
                    layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                    auto textRange = DWRITE_TEXT_RANGE{0, static_cast<UINT32>(wide.size())};
                    layout->SetFontSize(fontSize, textRange);
                    if (header) layout->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, textRange);
                    textLayoutEngine.ApplyStyles(layout.Get(), display.ranges);
                    ApplyMathInlineObjects(layout.Get(), display.mathOverlays);
                    auto alignment = column < special.table_aligns.size() ? special.table_aligns[column] : folia::TableAlignment::None;
                    if (alignment == folia::TableAlignment::Center) layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    else if (alignment == folia::TableAlignment::Right) layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                    else layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                    if (!editing)
                        if (auto index = EditorTableCheckboxIndex(special, rangeIndex))
                        {
                            const auto& part = special.table_cell_spans[*index];
                            const auto end = folia::platform::editor::DisplayPositionForSource(display.displayToSource,
                                {part.container_id, 3, folia::TextAffinity::Downstream});
                            if (end != 0 && end <= wide.size())
                            {
                                constexpr float iconSize = 20.0f;
                                ApplySyntaxIconObject(layout.Get(), 0, static_cast<UINT32>(end), iconSize, 1.0f);
                                FLOAT x = 0, y = 0;
                                DWRITE_HIT_TEST_METRICS metrics{};
                                if (SUCCEEDED(layout->HitTestTextPosition(0, FALSE, &x, &y, &metrics))) {
                                    const auto top = y + (metrics.height - iconSize) * .5f;
                                    cell.checkbox = special.table_checkboxes.at(*index);
                                    cell.checkboxRect = D2D1::RectF(x + 1, top, x + 1 + iconSize, top + iconSize);
                                }
                            }
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
                auto images = inlineImageRenderer.Resolve(
                    layout.Get(),
                    display.imageOverlays,
                    contentWidth,
                    requestEmbedded);
                auto trailingGap = textLayoutEngine.ApplyLineGap(layout.Get(), styleSheet.body.lineHeight);
                cellTextHeight = textLayoutEngine.MeasureHeight(layout.Get(), cellTextHeight, false, trailingGap);
                if (span.rows == 1)
                    rowHeights[row] = (std::max)(rowHeights[row], cellTextHeight + previewHeight + 2 * verticalPadding);
                prepared.pendingImage = prepared.pendingImage || std::ranges::any_of(
                    images,
                    [](auto const& image) { return image.pending; });
                cell.displayToSource = display.displayToSource;
                cell.textHeight = cellTextHeight + previewHeight;
                cell.layout = std::move(layout);
                cell.layoutLines = folia::platform::editor::BuildEditorTextLines(
                    cell.layout.Get(), cell.displayToSource, sourceSpan, contentWidth);
                cell.paintPlan = folia::platform::editor::EditorTextPaintPlan::Prepare(cell.layout.Get());
                prepared.pendingMath = prepared.pendingMath || display.pendingMath;
                prepared.displays.push_back(std::move(display));
                prepared.imageDraws.push_back(std::move(images));
                prepared.mathPreviews.push_back(std::move(previews));
                prepared.textHeights.push_back(cellTextHeight);
                table.cells.push_back(std::move(cell));
            }
        }
        for (auto const& cell : table.cells)
        {
            if (cell.rowSpan <= 1) continue;
            const auto first = rowHeights.begin() + static_cast<std::ptrdiff_t>(cell.row);
            const auto last = first + static_cast<std::ptrdiff_t>(cell.rowSpan);
            const auto available = std::accumulate(first, last, 0.0f);
            *(last - 1) += (std::max)(0.0f, cell.textHeight + 2 * verticalPadding - available);
        }
        std::vector<float> rowBoundaries{0.0f};
        rowBoundaries.reserve(table.rowCount + 1);
        for (auto rowHeight : rowHeights) rowBoundaries.push_back(rowBoundaries.back() + rowHeight);
        table.rowBoundaries = std::move(rowBoundaries);
        table.rect = D2D1::RectF(0.0f, 0.0f, table.columnBoundaries.back(), table.rowBoundaries.back());
        for (auto& cell : table.cells)
        {
            if (cell.covered) continue;
            cell.rect = D2D1::RectF(table.columnBoundaries[cell.column], table.rowBoundaries[cell.row],
                table.columnBoundaries[cell.column + cell.columnSpan], table.rowBoundaries[cell.row + cell.rowSpan]);
            const auto alignment = metadata && cell.column < metadata->vertical_alignments.size()
                ? metadata->vertical_alignments[cell.column] : folia::TableVerticalAlignment::Top;
            const auto extra = (std::max)(0.0f, cell.rect.bottom - cell.rect.top - cell.textHeight - 2 * verticalPadding);
            auto inset = verticalPadding;
            if (cell.rowSpan > 1 || cell.columnSpan > 1 || alignment == folia::TableVerticalAlignment::Middle) inset += extra * 0.5f;
            else if (alignment == folia::TableVerticalAlignment::Bottom) inset += extra;
            cell.textOrigin = D2D1::Point2F(cell.rect.left + horizontalPadding, cell.rect.top + inset);
            cell.textWidth = EditorTableCellContentWidth(cell.rect.right - cell.rect.left, metadata);
        }
        prepared.height = table.rect.bottom;
        if (table.hiddenRows)
        {
            prepared.height += styleSheet.body.lineHeight + 10.0f;
            table.foldBar = D2D1::RectF(0, table.rect.bottom, table.rect.right, prepared.height);
        }
        return prepared;
    }

    void EditorTableBlockRenderer::Paint(
        folia::RenderBlock const& block,
        PreparedTable const& prepared,
        float documentLeft,
        float top,
        float scrollOffset,
        float horizontalOffset,
        bool printMode,
        folia::NodeId editingCell,
        const folia::TableRegion* selection,
        EditorTableSelectionAnimation& selectionAnimation,
        const folia::platform::editor::EditorTableAction* gesture,
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
        auto const& local = prepared.visual;
        auto const* metadata = block.special().table_metadata.get();
        using Action = folia::platform::editor::EditorTableActionKind;
        auto positions = local.columnBoundaries;
        if (gesture && gesture->kind == Action::ResizeColumn && gesture->index < local.columnCount)
        {
            const auto old = positions.Local();
            std::vector<float> preview(old.begin(), old.end());
            const auto delta = gesture->width - (old[gesture->index + 1] - old[gesture->index]);
            for (auto column = gesture->index + 1; column < preview.size(); ++column) preview[column] += delta;
            positions = std::move(preview);
        }
        EditorVisualTable translatedTable;
        translatedTable.id = local.id;
        translatedTable.rect = local.rect;
        translatedTable.sourceSpans = local.sourceSpans;
        translatedTable.rowCount = local.rowCount;
        translatedTable.columnCount = local.columnCount;
        translatedTable.headerRows = prepared.headerRows;
        translatedTable.dataEnd = !local.hiddenRows && folia::table_has_totals(metadata)
            ? local.rowCount - 1 : local.rowCount;
        translatedTable.editable = local.editable;
        translatedTable.rowBoundaries = local.rowBoundaries;
        translatedTable.columnBoundaries = positions;
        translatedTable.viewportWidth = local.viewportWidth;
        translatedTable.maximumHorizontalOffset = (std::max)(0.0f, positions.back() - local.viewportWidth);
        translatedTable.horizontalOffset = (std::clamp)(horizontalOffset, 0.0f, translatedTable.maximumHorizontalOffset);
        if (selection && selection->first.row < local.rowCount && selection->last.column < local.columnCount)
        {
            translatedTable.selection = *selection;
            translatedTable.selection->last.row = (std::min)(selection->last.row, local.rowCount - 1);
        }
        translatedTable.hiddenRows = local.hiddenRows;
        if (local.hiddenRows)
        {
            translatedTable.foldBar = local.foldBar;
            translatedTable.foldBar.left += documentLeft;
            translatedTable.foldBar.right += documentLeft;
            translatedTable.foldBar.top += top;
            translatedTable.foldBar.bottom += top;
        }
        std::size_t firstRow = 0;
        std::size_t endRow = local.rowCount;
        if (!printMode)
        {
            auto rows = local.rowBoundaries.Local();
            // Retain one offscreen row in each direction for keyboard
            // navigation. The block cache owns the full layout.
            auto first = std::ranges::lower_bound(rows, -top);
            auto end = std::ranges::upper_bound(rows, resources.surfaceHeightDip - top);
            const auto beginIndex = static_cast<std::size_t>(first - rows.begin());
            firstRow = beginIndex > 2 ? beginIndex - 2 : 0;
            endRow = (std::min)(local.rowCount, static_cast<std::size_t>(end - rows.begin()) + 1);
        }
        std::vector<std::size_t> visibleCells;
        if (metadata && firstRow > 0) {
            for (const auto& merge : metadata->merges) {
                if (merge.first.row >= firstRow || merge.last.row < firstRow
                    || merge.first.column >= local.columnCount) continue;
                auto index = merge.first.row * local.columnCount + merge.first.column;
                auto const& cell = local.cells[index];
                if (cell.row + cell.rowSpan > firstRow) visibleCells.push_back(index);
            }
            std::ranges::sort(visibleCells);
            auto duplicates = std::ranges::unique(visibleCells);
            visibleCells.erase(duplicates.begin(), duplicates.end());
        }
        const auto headerHeight = local.rowBoundaries[prepared.headerRows];
        const auto stickyOffset = !printMode && !local.hiddenRows && top < 0.0f
            ? (std::min)(-top, (std::max)(0.0f, local.rect.bottom - headerHeight)) : 0.0f;
        translatedTable.stickyOffset = stickyOffset;
        auto const frozen = metadata && metadata->frozen && translatedTable.horizontalOffset > 0.0f;
        const auto firstColumn = printMode ? 0 : positions.Interval(translatedTable.horizontalOffset);
        const auto endColumn = printMode ? local.columnCount : 1 + positions.Interval(
            translatedTable.horizontalOffset + local.viewportWidth);
        auto appendRow = [&](std::size_t row) {
            if (frozen && firstColumn > 0 && !local.cells[row * local.columnCount].covered)
                visibleCells.push_back(row * local.columnCount);
            for (auto column = firstColumn; column < endColumn; ++column) {
                const auto index = row * local.columnCount + column;
                if (!local.cells[index].covered) visibleCells.push_back(index);
            }
        };
        if (stickyOffset > 0.0f)
            for (auto row = std::size_t{0}; row < (std::min)(firstRow, prepared.headerRows); ++row)
                appendRow(row);
        for (auto row = firstRow; row < endRow; ++row)
            appendRow(row);
        if (metadata && firstColumn > 0) {
            for (auto const& merge : metadata->merges)
                if (merge.first.row < endRow && merge.last.row >= firstRow
                    && merge.first.column < firstColumn && merge.last.column >= firstColumn) {
                    const auto index = merge.first.row * local.columnCount + merge.first.column;
                    if (index < local.cells.size() && local.cells[index].columnSpan > 1) visibleCells.push_back(index);
                }
            std::ranges::sort(visibleCells);
            auto duplicates = std::ranges::unique(visibleCells);
            visibleCells.erase(duplicates.begin(), duplicates.end());
        }
        translatedTable.frozenWidth = frozen ? (std::min)(positions[1], local.viewportWidth) : 0.0f;
        auto frozenCell = [&](const EditorVisualTableCell& cell) { return frozen && cell.column == 0 && cell.columnSpan == 1; };
        auto stickyCell = [&](const EditorVisualTableCell& cell) {
            return stickyOffset > 0.0f && cell.row + cell.rowSpan <= prepared.headerRows;
        };
        if (frozen || stickyOffset > 0.0f)
            std::ranges::stable_partition(visibleCells, [&](auto index) {
                const auto& cell = local.cells[index];
                return !frozenCell(cell) && !stickyCell(cell);
            });
        translatedTable.cells.reserve(visibleCells.size());
        for (auto index : visibleCells) translatedTable.cells.push_back(local.cells[index]);
        translatedTable.rect.left += documentLeft;
        translatedTable.rect.right = documentLeft + (std::min)(positions.back(), local.viewportWidth);
        translatedTable.rect.top += top;
        translatedTable.rect.bottom += top;
        translatedTable.columnBoundaries.Translate(documentLeft - translatedTable.horizontalOffset);
        translatedTable.rowBoundaries.Translate(top);
        for (auto& cell : translatedTable.cells)
        {
            auto x = documentLeft - (frozenCell(cell) ? 0.0f : translatedTable.horizontalOffset);
            auto y = top + (stickyCell(cell) ? stickyOffset : 0.0f);
            const auto shift = x + positions[cell.column] - cell.rect.left;
            cell.rect.left = x + positions[cell.column];
            cell.rect.right = x + positions[cell.column + cell.columnSpan];
            cell.rect.top += y;
            cell.rect.bottom += y;
            cell.textOrigin.x += shift;
            cell.textOrigin.y += y;
            if (cell.checkbox)
            {
                cell.checkboxRect.left += cell.textOrigin.x;
                cell.checkboxRect.right += cell.textOrigin.x;
                cell.checkboxRect.top += cell.textOrigin.y;
                cell.checkboxRect.bottom += cell.textOrigin.y;
            }
        }
        auto tableIndex = interactionMap.tables.size();
        interactionMap.tables.push_back(std::move(translatedTable));
        auto& visualTable = interactionMap.tables.back();
        std::optional<TableSelectionGeometry> selectionGeometry;
        if (const auto& area = visualTable.selection)
        {
            const auto target = D2D1::RectF(positions[area->first.column], local.rowBoundaries[area->first.row],
                positions[area->last.column + 1], local.rowBoundaries[area->last.row + 1]);
            selectionGeometry.emplace(visualTable, selectionAnimation.Sample(block.id, target));
            visualTable.selectionHandle = selectionGeometry->Handle();
        }
        EditorVisualBlock visualBlock;
        visualBlock.rect = visualTable.rect;
        visualBlock.sourceSpan = visualTable.SourceSpans().empty() ? block.source_span : visualTable.SourceSpans().front();
        visualBlock.documentY = top + scrollOffset;
        auto visualBlockIndex = interactionMap.blocks.size();
        interactionMap.blocks.push_back(std::move(visualBlock));
        const auto preset = metadata ? metadata->preset : folia::TablePreset::Default;
        const bool grid = preset == folia::TablePreset::Default || preset == folia::TablePreset::Zebra;
        auto* brush = resources.tableBrush.Get();
        auto fill = [&](D2D1_RECT_F rect, D2D1_COLOR_F color) {
            brush->SetColor(color);
            resources.d2dContext->FillRectangle(rect, brush);
        };
        for (std::size_t cellIndex = 0; cellIndex < visualTable.cells.size(); ++cellIndex)
        {
            auto& cell = visualTable.cells[cellIndex];
            const auto layoutIndex = visibleCells[cellIndex];
            const auto pinned = frozenCell(cell);
            const auto sticky = stickyCell(cell);
            const auto leftClip = documentLeft + (frozen && !pinned ? visualTable.frozenWidth : 0.0f);
            auto clipRect = cell.rect;
            clipRect.left = (std::max)(clipRect.left, leftClip);
            clipRect.right = (std::min)(clipRect.right, visualTable.rect.right);
            if (!sticky && stickyOffset > 0.0f)
                clipRect.top = (std::max)(clipRect.top, top + stickyOffset + headerHeight);
            if (clipRect.right <= clipRect.left || clipRect.bottom <= clipRect.top) continue;
            interactionMap.AddTableCellLines(visualBlockIndex, tableIndex, cellIndex);
            if (!printMode && (cell.rect.bottom < 0 || cell.rect.top > resources.surfaceHeightDip)) continue;
            resources.d2dContext->PushAxisAlignedClip(D2D1::RectF(clipRect.left, clipRect.top - 1,
                clipRect.right, cell.rect.bottom + 1), D2D1_ANTIALIAS_MODE_ALIASED);
            struct Clip
            {
                ID2D1DeviceContext* context;
                ~Clip() { context->PopAxisAlignedClip(); }
            } clip{resources.d2dContext.Get()};
            const auto appearance = EditorTableCellAppearance(metadata, cell.row, cell.column);
            const bool headerRow = cell.row < prepared.headerRows;
            const bool totalRow = folia::table_has_totals(metadata) && cell.row + 1 == block.special().row_count;
            if (pinned || sticky)
            {
                auto surface = styleSheet.canvasColor;
                surface.a = 1;
                fill(cell.rect, surface);
            }
            if (appearance.background)
            {
                auto color = styleSheet.tablePalette[static_cast<std::size_t>(*appearance.background)];
                color.a *= *appearance.background == folia::TableColor::Gray ? .07f : .16f;
                fill(cell.rect, color);
            }
            else if (totalRow || (grid && (headerRow || (metadata && metadata->header_column && cell.column == 0))))
            {
                fill(cell.rect, styleSheet.tableHeaderColor);
            }
            else if (preset == folia::TablePreset::Zebra && (cell.row - prepared.headerRows) % 2 == 1)
            {
                auto color = styleSheet.tableHeaderColor;
                color.a *= 0.4f;
                fill(cell.rect, color);
            }
            if (selectionGeometry)
            {
                const auto bounds = selectionGeometry->Bounds(pinned, sticky);
                const auto highlight = D2D1::RectF((std::max)(clipRect.left, bounds.left),
                    (std::max)(clipRect.top, bounds.top), (std::min)(clipRect.right, bounds.right),
                    (std::min)(clipRect.bottom, bounds.bottom));
                if (highlight.right > highlight.left && highlight.bottom > highlight.top)
                    resources.d2dContext->FillRectangle(highlight, resources.selectionBrush.Get());
            }

            const auto right = cell.rect.right;
            const auto bottom = cell.rect.bottom;
            const auto atTopEdge = cell.row == 0;
            const auto lastRow = cell.row + cell.rowSpan == visualTable.rowCount;
            const auto headerEnd = cell.row + cell.rowSpan == prepared.headerRows;
            auto line = [&](float x1, float y1, float x2, float y2, float width = 1.0f) {
                resources.d2dContext->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush, width);
            };
            brush->SetColor(styleSheet.tableBorderColor);
            if (grid)
            {
                if (cell.column == 0) line(cell.rect.left, cell.rect.top, cell.rect.left, bottom);
                line(right, cell.rect.top, right, bottom);
                if (atTopEdge) line(cell.rect.left, cell.rect.top, right, cell.rect.top);
                line(cell.rect.left, bottom, right, bottom);
            }
            else
            {
                if (preset == folia::TablePreset::Academic && atTopEdge)
                    line(cell.rect.left, cell.rect.top, right, cell.rect.top, 1.5f);
                if (headerEnd || lastRow || preset == folia::TablePreset::Minimal)
                    line(cell.rect.left, bottom, right, bottom, headerEnd ? 1.5f : 1.0f);
            }
            const bool editing = editingCell == cell.sourceSpan.container_id;
            if (!printMode && editing)
            {
                auto outline = styleSheet.accentColor;
                outline.a *= .65f;
                brush->SetColor(outline);
                resources.d2dContext->DrawRectangle(cell.rect, brush, 1.25f);
            }

            resources.d2dContext->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_ALIASED);
            Clip textClip{resources.d2dContext.Get()};

            auto const& display = prepared.displays[layoutIndex];
            if (cell.layout)
            {
                if (cell.checkbox)
                    resources.chromeIcons.Draw(resources.d2dContext.Get(), *cell.checkbox ? L"checkbox-checked" : L"checkbox",
                        cell.checkboxRect, *cell.checkbox ? styleSheet.accentColor : resources.mutedBrush->GetColor());
                if (!cell.checkbox && !editing && !headerRow && !totalRow && metadata)
                    PaintValueBadge(resources, styleSheet, cell, display, *metadata);
                auto* textBrush = resources.textBrush.Get();
                if (appearance.foreground)
                {
                    brush->SetColor(styleSheet.tablePalette[static_cast<std::size_t>(*appearance.foreground)]);
                    textBrush = brush;
                }
                documentPainter.DrawInlineText(cell.sourceSpan.container_id, cell.layout.Get(), cell.textOrigin,
                    display, cell.displayToSource, textBrush, cell.paintPlan, cell.layoutLines);
                for (auto const& overlay : display.footnoteOverlays)
                {
                    auto metrics = folia::platform::editor::TextRangeContentMetrics(
                        cell.layout.Get(), overlay.displayStart, overlay.displayLength, cell.textOrigin);
                    for (auto const& metric : metrics)
                    {
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
                inlineImageRenderer.Draw(cell.layout.Get(), cell.textOrigin, prepared.imageDraws[layoutIndex]);
                for (auto const& positioned : EditorDocumentPainter::PaintMath(cell.layout.Get(), cell.textOrigin,
                        display.mathOverlays, cell.layout->GetMaxWidth(), resources, drawMath, drawMathFallback))
                {
                    auto const& overlay = *positioned.overlay;
                    auto mathY = cell.textOrigin.y + positioned.localTop;
                    auto mathX = cell.textOrigin.x + positioned.localX;
                    interactionMap.mathHits.push_back(EditorVisualMathHit{D2D1::RectF(mathX, mathY, mathX + overlay.fragment.width, mathY + overlay.fragment.height), overlay.sourceSpan, overlay.progressStart, overlay.progressEnd});
                }
            }
            if (layoutIndex < prepared.mathPreviews.size()
                && layoutIndex < prepared.textHeights.size())
            {
                auto previewTop = cell.textOrigin.y + prepared.textHeights[layoutIndex];
                for (auto const& preview : prepared.mathPreviews[layoutIndex])
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
        }
        if (selectionGeometry)
            selectionGeometry->PaintOutline(*resources.d2dContext.Get(), resources.accentBrush.Get());
        if (visualTable.hiddenRows)
        {
            auto bounds = visualTable.foldBar;
            fill(bounds, styleSheet.tableHeaderColor);
            bounds.left += 10.0f;
            bounds.top += 4.0f;
            auto label = LocalizeFormat(L"TableShowRows", {winrt::to_hstring(visualTable.hiddenRows)});
            resources.d2dContext->DrawText(label.data(), static_cast<UINT32>(label.size()),
                resources.textFormat.Get(), bounds, resources.mutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    }
}
