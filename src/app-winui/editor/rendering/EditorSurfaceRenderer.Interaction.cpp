#include "pch.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/rendering/layout/EditorPreparedDocument.h"

namespace winrt::Folia
{
    void EditorSurfaceRenderer::ScrollBy(float delta) { SetScrollOffset(scrollState.Offset() + delta); }
    void EditorSurfaceRenderer::QueueScrollBy(float delta)
    {
        scrollState.Queue(delta, MaximumScrollOffset());
    }

    bool EditorSurfaceRenderer::AdvanceScrollAnimation(float elapsedSeconds)
    {
        return scrollState.Advance(elapsedSeconds);
    }

    void EditorSurfaceRenderer::SetScrollOffset(float value)
    {
        scrollState.Set(value, MaximumScrollOffset());
    }
    float EditorSurfaceRenderer::ScrollOffset() const { return scrollState.Offset(); }
    float EditorSurfaceRenderer::MaximumScrollOffset() const { return (std::max)(0.0f, totalDocumentHeight - resources.surfaceHeightDip); }
    float EditorSurfaceRenderer::ViewportHeight() const { return resources.surfaceHeightDip; }
    float EditorSurfaceRenderer::ViewportWidth() const { return resources.surfaceWidthDip; }
    void EditorSurfaceRenderer::UpdatePointer(float x, float y) { pointerPosition = D2D1::Point2F(x, y); }
    void EditorSurfaceRenderer::ClearPointer() { pointerPosition.reset(); }
    void EditorSurfaceRenderer::SetTableDrag(std::optional<EditorTableAction> action, std::optional<std::size_t> dropIndex) { draggedTableAction = std::move(action); tableDropIndex = dropIndex; }
    folia::platform::editor::EditorTableControls EditorSurfaceRenderer::TableControlsAt(float x, float y) const
    {
        return interactionMap.TableControlsAt(x, y);
    }
    std::optional<folia::TextSelection> EditorSurfaceRenderer::TableAxisSelection(EditorTableAction const& action) const
    {
        const auto table = std::ranges::find(interactionMap.tables, action.tableId, &EditorVisualTable::id);
        if (table == interactionMap.tables.end() || table->SourceSpans().empty()) return std::nullopt;
        const auto spans = table->SourceSpans();
        const auto row = action.kind == folia::platform::editor::EditorTableActionKind::DragRow;
        const auto first = row ? action.index * table->columnCount : action.index;
        const auto last = first + (row ? table->columnCount - 1 : (table->rowCount - 1) * table->columnCount);
        if (last >= spans.size()) return std::nullopt;
        return folia::TextSelection::cells({spans[last].container_id, 0}, {spans[first].container_id, 0});
    }
    std::optional<folia::TableFormatEdit> EditorSurfaceRenderer::TableResizeFormat(
        EditorTableAction const& action, float width) const
    {
        const auto table = std::ranges::find(interactionMap.tables, action.tableId, &EditorVisualTable::id);
        if (table == interactionMap.tables.end() || action.index >= table->columnCount) return std::nullopt;
        const auto positions = table->columnBoundaries.Local();
        folia::TableFormatEdit edit{.kind = folia::TableFormatKind::ColumnWidths};
        edit.widths.reserve(table->columnCount);
        float total = 0.0f;
        for (std::size_t column = 0; column < table->columnCount; ++column)
        {
            const auto measured = column == action.index ? width : positions[column + 1] - positions[column];
            edit.widths.push_back(measured);
            total += measured;
        }
        edit.enabled = total > table->viewportWidth + 1.0f;
        return edit;
    }
    std::optional<folia::TextSpan> EditorSurfaceRenderer::TableCellAt(float x, float y) const
    {
        auto hit = interactionMap.TableCellAt(x, y);
        if (!hit || !interactionMap.tables[hit->first].editable) return std::nullopt;
        return interactionMap.tables[hit->first].cells[hit->second].sourceSpan;
    }
    std::optional<folia::TableCoordinate> EditorSurfaceRenderer::TableFillTargetAt(folia::NodeId table, float x, float y) const
    {
        return interactionMap.TableFillTargetAt(table, x, y);
    }
    std::optional<std::pair<folia::NodeId, float>> EditorSurfaceRenderer::ScrollTableAt(float x, float y, float delta) const
    {
        for (const auto& table : interactionMap.tables)
        {
            if (table.maximumHorizontalOffset <= 0.0f || x < table.rect.left || x > table.rect.right
                || y < table.rect.top || y > table.rect.bottom) continue;
            return std::pair{table.id,
                (std::clamp)(table.horizontalOffset + delta, 0.0f, table.maximumHorizontalOffset)};
        }
        return std::nullopt;
    }
    std::optional<float> EditorSurfaceRenderer::RevealTableColumn(const folia::TableCellAddress& address) const
    {
        const auto table = std::ranges::find(interactionMap.tables, address.table_id, &EditorVisualTable::id);
        if (table == interactionMap.tables.end() || table->maximumHorizontalOffset <= 0.0f
            || address.column + 1 >= table->columnBoundaries.size()
            || (address.column == 0 && table->frozenWidth > 0.0f)) return std::nullopt;
        const auto left = table->columnBoundaries[address.column];
        const auto right = table->columnBoundaries[address.column + 1];
        const auto leftEdge = table->rect.left + table->frozenWidth;
        const auto amount = left < leftEdge ? left - leftEdge : (std::max)(0.0f, right - table->rect.right);
        const auto target = (std::clamp)(table->horizontalOffset + amount, 0.0f, table->maximumHorizontalOffset);
        return target != table->horizontalOffset ? std::optional{target} : std::nullopt;
    }
    std::optional<std::size_t> EditorSurfaceRenderer::TableDropIndexAt(float x, float y, bool rows) const { return interactionMap.TableDropIndexAt(draggedTableAction, x, y, rows); }

    bool EditorSurfaceRenderer::ScrollToPosition(folia::TextPosition position, EditorScrollAlignment alignment)
    {
        auto previous = scrollState.Offset();
        if (auto bounds = CaretBounds(position))
        {
            auto margin = styleSheet.verticalPadding;
            auto next = previous;
            if (alignment == EditorScrollAlignment::Start) next = previous + bounds->top - margin;
            else if (bounds->top < margin) next = (std::max)(0.0f, previous - (margin - bounds->top));
            else if (bounds->bottom > resources.surfaceHeightDip - margin) next = (std::min)(MaximumScrollOffset(), previous + bounds->bottom - (resources.surfaceHeightDip - margin));
            scrollState.Set(next, MaximumScrollOffset());
            return scrollState.Offset() != previous;
        }
        if (preparedDocument)
        {
            auto owner = preparedDocument->FindOwnerBlock(position.container_id);
            if (owner && *owner < preparedDocument->geometry.Size())
            {
                auto placement = preparedDocument->geometry.At(*owner);
                auto y = placement.top;
                auto const& visuals = preparedDocument->blocks[*owner].Visuals();
                if (visuals.table)
                {
                    auto const& table = visuals.table->visual;
                    auto cell = EditorTableCellIndex(table.SourceSpans(), position.container_id, preparedDocument->editableIndex);
                    if (table.columnCount > 0 && cell)
                    {
                        auto row = *cell / table.columnCount;
                        if (row < table.rowBoundaries.size()) y += table.rowBoundaries[row];
                    }
                }
                scrollState.Set(
                    y - styleSheet.verticalPadding,
                    MaximumScrollOffset());
                return scrollState.Offset() != previous;
            }
        }
        for (auto const& block : interactionMap.blocks)
        {
            auto mapped = std::any_of(block.displayToSource.begin(), block.displayToSource.end(), [&](auto value) { return value.container_id == position.container_id; });
            if (block.sourceSpan.container_id != position.container_id && !mapped) continue;
            scrollState.Set(block.documentY - styleSheet.verticalPadding, MaximumScrollOffset());
            return scrollState.Offset() != previous;
        }
        return false;
    }

    std::optional<folia::TextPosition> EditorSurfaceRenderer::HitTest(float x, float y) const
    {
        for (auto const& rect : nonInteractiveRegions)
            if (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom)
                return std::nullopt;
        return interactionMap.HitTest(x, y);
    }
    std::optional<folia::TextPosition> EditorSurfaceRenderer::HitTestTableText(float x, float y, folia::NodeId owner) const
    {
        return interactionMap.HitTest(x, y, false, owner);
    }

    std::optional<folia::TextPosition> EditorSurfaceRenderer::HitTestTextUnit(float x, float y) const
    {
        for (auto const& rect : nonInteractiveRegions)
            if (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom)
                return std::nullopt;
        return interactionMap.HitTest(x, y, true);
    }
    std::optional<folia::TextPosition> EditorSurfaceRenderer::ViewportPosition() const
    {
        // Use the existing lossless visual-to-source mapping, including nested
        // block flows and source mode. This does not move the editing selection.
        EditorVisualLine const* first = nullptr;
        for (auto const& line : interactionMap.lines)
        {
            // A line ending exactly at the reading edge has already scrolled
            // away. Pointer hit testing deliberately has different edge rules.
            if (line.SourceSpans().empty() || line.rect.bottom <= styleSheet.verticalPadding + 0.1f
                || line.rect.top >= resources.surfaceHeightDip) continue;
            if (!first || line.rect.top < first->rect.top
                || (line.rect.top == first->rect.top && line.rect.left < first->rect.left)) first = &line;
        }
        if (first)
        {
            auto const& span = first->SourceSpans().front();
            return folia::TextPosition{span.container_id, span.source_range.start, folia::TextAffinity::Downstream};
        }
        return interactionMap.HitTest(0.0f, styleSheet.verticalPadding);
    }
    std::optional<folia::TextPosition> EditorSurfaceRenderer::TaskCheckboxAt(float x, float y) const { return interactionMap.TaskCheckboxAt(x, y); }
    std::optional<EditorVisualFootnoteHit> EditorSurfaceRenderer::FootnoteAt(float x, float y) const { return interactionMap.FootnoteAt(x, y); }
    std::optional<EditorVisualRawBlock> EditorSurfaceRenderer::RawBlockFor(folia::NodeId owner) const { return interactionMap.RawBlockFor(owner); }
    std::optional<EditorVisualRawBlock> EditorSurfaceRenderer::CodeLanguageAt(float x, float y) const
    {
        if (x < 0 || y < 0 || x >= resources.surfaceWidthDip || y >= resources.surfaceHeightDip) return {};
        for (auto const& block : interactionMap.rawBlocks) {
            auto const& r = block.headerRect;
            if (block.kind == folia::platform::editor::EditorRawBlockKind::Code && block.languageEditable
                && x >= r.left && x < r.right && y >= r.top && y < r.bottom) return block;
        }
        return {};
    }
    std::optional<D2D1_RECT_F> EditorSurfaceRenderer::CaretBounds(folia::TextPosition position) const { return interactionMap.CaretBounds(position, styleSheet.body.lineHeight); }
    std::optional<folia::TextPosition> EditorSurfaceRenderer::MoveCaretVertically(folia::TextPosition position, bool down, float& goalX) const { return interactionMap.MoveCaretVertically(position, down, goalX, styleSheet.body.lineHeight); }
    std::optional<folia::TextPosition> EditorSurfaceRenderer::MoveCaretByPage(
        folia::TextPosition position, bool down, float& goalX) const
    {
        auto caret = CaretBounds(position);
        if (!caret) return std::nullopt;
        if (goalX < 0.0f) goalX = caret->left;
        auto lineHeight = (std::max)(1.0f, caret->bottom - caret->top);
        auto distance = (std::max)(lineHeight, ViewportHeight() - lineHeight);
        auto targetY = (caret->top + caret->bottom) * 0.5f + (down ? distance : -distance);
        return interactionMap.HitTest(goalX, targetY);
    }
    std::optional<folia::TextPosition> EditorSurfaceRenderer::VisualLineStart(folia::TextPosition position) const { return interactionMap.VisualLineStart(position); }
    std::optional<folia::TextPosition> EditorSurfaceRenderer::VisualLineEnd(folia::TextPosition position) const { return interactionMap.VisualLineEnd(position); }

}
