module;
#include <d2d1.h>
#include <dwrite.h>

module folia.platform.editor_interaction;
import std;

namespace folia::platform::editor
{
    namespace
    {
        folia::TextSpan const* SourceSpanFor(EditorVisualLine const& line, folia::NodeId containerId)
        {
            auto found = std::find_if(line.SourceSpans().begin(), line.SourceSpans().end(), [&](auto const& span)
            {
                return span.container_id == containerId;
            });
            return found == line.SourceSpans().end() ? nullptr : &*found;
        }

        bool Contains(EditorVisualLine const& line, folia::TextPosition position)
        {
            auto span = SourceSpanFor(line, position.container_id);
            return span && span->source_range.covers(position.source_offset);
        }

        std::vector<folia::TextSpan> SpansFromMapping(
            EditorDisplayMapping const& mapping,
            std::size_t start,
            std::size_t end,
            folia::TextSpan fallback)
        {
            if (mapping.empty()) return {fallback};
            start = (std::min)(start, mapping.size() - 1);
            end = (std::min)(end, mapping.size() - 1);
            if (end < start) std::swap(start, end);

            std::vector<folia::TextSpan> spans;
            auto add = [&](EditorDisplayPosition position, bool allowNewContainer)
            {
                auto found = std::find_if(spans.begin(), spans.end(), [&](auto const& span)
                {
                    return span.container_id == position.container_id;
                });
                if (position.kind == EditorDisplayPositionKind::BoundaryDecoration) {
                    // A suffix can carry the preceding text's final source
                    // boundary, but cannot create a source row or pull its
                    // start backwards into decoration owned by another line.
                    if (found != spans.end() && position.affinity == folia::TextAffinity::Upstream)
                        found->source_range.end = (std::max)(found->source_range.end, position.source_offset);
                    return;
                }
                if (found == spans.end())
                {
                    if (allowNewContainer)
                        spans.push_back({position.container_id, {position.source_offset, position.source_offset}});
                    return;
                }
                found->source_range.start = (std::min)(found->source_range.start, position.source_offset);
                found->source_range.end = (std::max)(found->source_range.end, position.source_offset);
            };

            if (start == end)
            {
                add(mapping[start], true);
            }
            else
            {
                // [start, end) contains the glyphs on this visual line. The
                // mapping at end is a caret boundary: it may extend an existing
                // container, but must not pull the next block onto this line.
                mapping.for_each_run(start, end, [&](auto, auto, auto first, auto last) {
                    add(first, true);
                    add(last, true);
                    return true;
                });
                // A terminal source boundary can own an empty editable row
                // after a generated prefix, but cannot extend a text line
                // to the next source container.
                add(mapping[end], spans.empty());
            }
            if (spans.empty()) spans.push_back(fallback);
            return spans;
        }

        std::optional<folia::TextPosition> ResolveMappedPosition(
            EditorVisualLine const& line,
            EditorDisplayMapping const& mapping,
            std::size_t displayPosition)
        {
            if (line.SourceSpans().empty()) return std::nullopt;
            if (mapping.empty())
            {
                auto const& span = line.SourceSpans().front();
                return folia::TextPosition{span.container_id, span.source_range.start, folia::TextAffinity::Downstream};
            }

            auto first = (std::min)(static_cast<std::size_t>(line.displayStart), mapping.size() - 1);
            auto last = (std::min)(static_cast<std::size_t>(line.displayEnd), mapping.size() - 1);
            displayPosition = (std::clamp)(displayPosition, first, last);
            auto resolve = [&](std::size_t index) -> std::optional<folia::TextPosition>
            {
                folia::TextPosition position = mapping[index];
                auto span = SourceSpanFor(line, position.container_id);
                if (!span) return std::nullopt;
                position.source_offset = (std::clamp)(position.source_offset, span->source_range.start, span->source_range.end);
                return position;
            };

            if (auto direct = resolve(displayPosition)) return direct;
            if (displayPosition >= last)
            {
                auto const& span = line.SourceSpans().back();
                return folia::TextPosition{span.container_id, span.source_range.end, folia::TextAffinity::Upstream};
            }
            if (displayPosition <= first)
            {
                auto const& span = line.SourceSpans().front();
                return folia::TextPosition{span.container_id, span.source_range.start, folia::TextAffinity::Downstream};
            }

            for (auto index = displayPosition; index > first; --index)
                if (auto before = resolve(index - 1)) return before;
            for (auto index = displayPosition + 1; index <= last; ++index)
                if (auto after = resolve(index)) return after;

            auto const& span = line.SourceSpans().front();
            return folia::TextPosition{span.container_id, span.source_range.start, folia::TextAffinity::Downstream};
        }
    }

    void EditorInteractionMap::Clear()
    {
        blocks.clear();
        lines.clear();
        tables.clear();
        mathHits.clear();
        taskCheckboxHits.clear();
        footnoteHits.clear();
        rawBlocks.clear();
    }

    std::optional<EditorVisualRawBlock> EditorInteractionMap::RawBlockFor(
        folia::NodeId owner) const
    {
        auto found = std::find_if(rawBlocks.rbegin(), rawBlocks.rend(), [&](auto const& block)
        {
            return block.contentStart.container_id == owner;
        });
        return found == rawBlocks.rend()
            ? std::nullopt
            : std::optional<EditorVisualRawBlock>{*found};
    }

    EditorLayoutLines BuildEditorTextLines(IDWriteTextLayout* layout, EditorDisplayMapping const& mapping,
        folia::TextSpan sourceSpan, float width)
    {
        auto result = std::make_shared<std::vector<EditorVisualLine>>();
        if (!layout || mapping.empty()) return result;
        UINT32 lineCount = 0;
        if (layout->GetLineMetrics(nullptr, 0, &lineCount) != E_NOT_SUFFICIENT_BUFFER || lineCount == 0) return result;
        std::vector<DWRITE_LINE_METRICS> metrics(lineCount);
        if (FAILED(layout->GetLineMetrics(metrics.data(), lineCount, &lineCount))) return result;
        result->reserve(lineCount);
        UINT32 textPosition = 0;
        UINT32 previousNewlineLength = 0;
        float lineTop = 0;
        for (UINT32 lineIndex = 0; lineIndex < lineCount; ++lineIndex)
        {
            auto const& metric = metrics[lineIndex];
            auto lineEnd = textPosition + metric.length;
            auto visibleEnd = lineEnd >= metric.newlineLength ? lineEnd - metric.newlineLength : lineEnd;
            EditorVisualLine line;
            line.sourceSpans = std::make_shared<const std::vector<folia::TextSpan>>(
                SpansFromMapping(mapping, textPosition, visibleEnd, sourceSpan));
            line.displayStart = textPosition;
            line.displayEnd = visibleEnd;
            line.wrapContinuation = lineIndex > 0 && previousNewlineLength == 0;
            line.rect = D2D1::RectF(0, lineTop, width, lineTop + metric.height);
            auto first = (std::min)(static_cast<std::size_t>(textPosition), mapping.size());
            auto end = (std::min)(static_cast<std::size_t>(visibleEnd) + 1, mapping.size());
            // Generated prefixes, padding and control rows do not own source
            // lines. Include the terminal boundary so empty source rows remain
            // editable even when they have no visible glyphs.
            auto decorationOnly = first < end && std::all_of(mapping.begin() + first,
                mapping.begin() + end, [](auto const& point) {
                    return point.kind == EditorDisplayPositionKind::BoundaryDecoration;
                });
            if (!decorationOnly) result->push_back(std::move(line));
            textPosition = lineEnd;
            lineTop += metric.height;
            previousNewlineLength = metric.newlineLength;
        }
        return result;
    }

    void EditorInteractionMap::AddBlockLines(std::size_t blockIndex)
    {
        if (blockIndex >= blocks.size()) return;
        auto& block = blocks[blockIndex];
        if (!block.layoutLines)
            block.layoutLines = BuildEditorTextLines(block.layout.Get(), block.displayToSource, block.sourceSpan, block.textWidth);
        for (auto line : *block.layoutLines) {
            line.blockIndex = blockIndex;
            line.rect.left += block.textOrigin.x; line.rect.right += block.textOrigin.x;
            line.rect.top += block.textOrigin.y; line.rect.bottom += block.textOrigin.y;
            lines.push_back(std::move(line));
        }
    }

    void EditorInteractionMap::AddTableCellLines(std::size_t blockIndex, std::size_t tableIndex, std::size_t cellIndex)
    {
        if (blockIndex >= blocks.size() || tableIndex >= tables.size() || cellIndex >= tables[tableIndex].cells.size()) return;
        auto& cell = tables[tableIndex].cells[cellIndex];
        auto before = lines.size();
        if (!cell.layoutLines)
            cell.layoutLines = BuildEditorTextLines(cell.layout.Get(), cell.displayToSource, cell.sourceSpan, cell.textWidth);
        for (auto line : *cell.layoutLines) {
            line.blockIndex = blockIndex; line.tableIndex = tableIndex; line.cellIndex = cellIndex;
            line.rect.left = cell.rect.left; line.rect.right = cell.rect.right;
            line.rect.top += cell.textOrigin.y; line.rect.bottom += cell.textOrigin.y;
            lines.push_back(std::move(line));
        }
        if (lines.size() != before) return;
        EditorVisualLine line;
        line.blockIndex = blockIndex;
        line.tableIndex = tableIndex;
        line.cellIndex = cellIndex;
        line.sourceSpans = std::make_shared<const std::vector<folia::TextSpan>>(
            std::initializer_list<folia::TextSpan>{cell.sourceSpan});
        line.displayEnd = static_cast<std::uint32_t>(cell.displayToSource.empty() ? 0 : cell.displayToSource.size() - 1);
        line.rect = cell.rect;
        lines.push_back(line);
    }

    std::optional<std::size_t> EditorInteractionMap::LineIndexFor(folia::TextPosition position) const
    {
        std::optional<std::size_t> first;
        std::optional<std::size_t> last;
        EditorDisplayMapping const* currentMapping = nullptr;
        std::size_t displayPosition = 0;
        bool previewAnchor = false;
        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            auto const& line = lines[index];
            if (!Contains(line, position) || line.blockIndex >= blocks.size()) continue;
            auto mapping = &blocks[line.blockIndex].displayToSource;
            if (line.tableIndex < tables.size() && line.cellIndex < tables[line.tableIndex].cells.size())
                mapping = &tables[line.tableIndex].cells[line.cellIndex].displayToSource;
            if (mapping != currentMapping) {
                currentMapping = mapping;
                displayPosition = DisplayPositionForSource(*mapping, position);
                previewAnchor = displayPosition < mapping->size()
                    && (*mapping)[displayPosition].kind == EditorDisplayPositionKind::GeneratedPreview;
            }
            // Generated text can span many lines at one source offset. Its
            // canonical display boundary, rather than those repeated source
            // ranges, determines the line that owns the caret.
            if (previewAnchor
                && (displayPosition < line.displayStart || displayPosition > line.displayEnd)) continue;
            if (!first) first = index;
            last = index;
        }
        if (!first) return std::nullopt;
        return previewAnchor || position.affinity == folia::TextAffinity::Upstream ? first : last;
    }

    std::optional<D2D1_RECT_F> EditorInteractionMap::CaretRectOnLine(
        EditorVisualLine const& line,
        folia::TextPosition position,
        float bodyLineHeight) const
    {
        auto sourceSpan = SourceSpanFor(line, position.container_id);
        if (line.blockIndex >= blocks.size() || !sourceSpan) return std::nullopt;
        auto const& block = blocks[line.blockIndex];
        position.source_offset = (std::clamp)(position.source_offset, sourceSpan->source_range.start, sourceSpan->source_range.end);
        if (block.thematicBreak)
        {
            auto lineHeight = (std::min)(bodyLineHeight, (line.rect.bottom - line.rect.top) * 0.46f);
            auto top = position.source_offset <= sourceSpan->source_range.start ? line.rect.top : line.rect.bottom - lineHeight;
            return D2D1::RectF(line.rect.left, top, line.rect.left + 2.0f, top + lineHeight);
        }
        IDWriteTextLayout* layout = block.layout.Get();
        auto textOrigin = block.textOrigin;
        auto const* mapping = &block.displayToSource;
        if (line.tableIndex < tables.size() && line.cellIndex < tables[line.tableIndex].cells.size())
        {
            auto const& cell = tables[line.tableIndex].cells[line.cellIndex];
            layout = cell.layout.Get();
            textOrigin = cell.textOrigin;
            mapping = &cell.displayToSource;
        }
        if (!layout) return D2D1::RectF(line.rect.left, line.rect.top, line.rect.left + 2.0f, line.rect.bottom);
        auto displayPos = DisplayPositionForSource(*mapping, position);
        const auto previewAnchor = displayPos < mapping->size()
            && (*mapping)[displayPos].kind == EditorDisplayPositionKind::GeneratedPreview;
        displayPos = (std::clamp)(displayPos, static_cast<std::size_t>(line.displayStart), static_cast<std::size_t>(line.displayEnd));
        UINT32 hitPos = static_cast<UINT32>(displayPos);
        BOOL trailing = FALSE;
        if (displayPos == line.displayEnd
            && (previewAnchor || position.affinity == folia::TextAffinity::Upstream)
            && displayPos > line.displayStart) { --hitPos; trailing = TRUE; }
        FLOAT x = 0.0f, y = 0.0f;
        DWRITE_HIT_TEST_METRICS metrics{};
        if (FAILED(layout->HitTestTextPosition(hitPos, trailing, &x, &y, &metrics))) return std::nullopt;
        auto left = textOrigin.x + x;
        return D2D1::RectF(left, line.rect.top, left + 2.0f, line.rect.bottom);
    }

    std::optional<D2D1_RECT_F> EditorInteractionMap::CaretBounds(folia::TextPosition position, float bodyLineHeight) const
    {
        auto line = LineIndexFor(position);
        return line ? CaretRectOnLine(lines[*line], position, bodyLineHeight) : std::nullopt;
    }

    std::vector<D2D1_RECT_F> EditorInteractionMap::TextRangeBounds(
        folia::TextSpan span) const
    {
        std::vector<D2D1_RECT_F> result;
        if (span.source_range.empty()) return result;
        for (auto const& line : lines)
        {
            auto lineSpan = SourceSpanFor(line, span.container_id);
            if (!lineSpan) continue;
            auto start = (std::max)(span.source_range.start, lineSpan->source_range.start);
            auto end = (std::min)(span.source_range.end, lineSpan->source_range.end);
            if (end <= start || line.blockIndex >= blocks.size()) continue;

            auto const& block = blocks[line.blockIndex];
            IDWriteTextLayout* layout = block.layout.Get();
            auto textOrigin = block.textOrigin;
            auto const* mapping = &block.displayToSource;
            if (line.tableIndex < tables.size()
                && line.cellIndex < tables[line.tableIndex].cells.size())
            {
                auto const& cell = tables[line.tableIndex].cells[line.cellIndex];
                layout = cell.layout.Get();
                textOrigin = cell.textOrigin;
                mapping = &cell.displayToSource;
            }
            if (!layout || mapping->empty()) continue;

            auto displayStart = DisplayPositionForSource(*mapping, {
                span.container_id, start, folia::TextAffinity::Downstream});
            auto displayEnd = DisplayPositionForSource(*mapping, {
                span.container_id, end, folia::TextAffinity::Upstream});
            displayStart = (std::clamp)(
                displayStart,
                static_cast<std::size_t>(line.displayStart),
                static_cast<std::size_t>(line.displayEnd));
            displayEnd = (std::clamp)(
                displayEnd,
                static_cast<std::size_t>(line.displayStart),
                static_cast<std::size_t>(line.displayEnd));
            if (displayEnd <= displayStart) continue;

            UINT32 count = 0;
            auto hit = layout->HitTestTextRange(
                static_cast<UINT32>(displayStart),
                static_cast<UINT32>(displayEnd - displayStart),
                textOrigin.x,
                textOrigin.y,
                nullptr,
                0,
                &count);
            if (hit != E_NOT_SUFFICIENT_BUFFER || count == 0) continue;
            std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
            if (FAILED(layout->HitTestTextRange(
                    static_cast<UINT32>(displayStart),
                    static_cast<UINT32>(displayEnd - displayStart),
                    textOrigin.x,
                    textOrigin.y,
                    metrics.data(),
                    count,
                    &count))) continue;
            for (UINT32 index = 0; index < count; ++index)
            {
                auto const& metric = metrics[index];
                result.push_back(D2D1::RectF(
                    metric.left,
                    metric.top,
                    metric.left + metric.width,
                    metric.top + metric.height));
            }
        }
        return result;
    }

    std::optional<folia::TextPosition> EditorInteractionMap::VisualLineStart(folia::TextPosition position) const
    {
        auto line = LineIndexFor(position);
        if (!line) return std::nullopt;
        auto span = SourceSpanFor(lines[*line], position.container_id);
        if (!span) return std::nullopt;
        return folia::TextPosition{span->container_id, span->source_range.start, folia::TextAffinity::Downstream};
    }

    std::optional<folia::TextPosition> EditorInteractionMap::VisualLineEnd(folia::TextPosition position) const
    {
        auto line = LineIndexFor(position);
        if (!line) return std::nullopt;
        auto span = SourceSpanFor(lines[*line], position.container_id);
        if (!span) return std::nullopt;
        return folia::TextPosition{span->container_id, span->source_range.end, folia::TextAffinity::Upstream};
    }

    std::optional<folia::TextPosition> EditorInteractionMap::HitTest(float x, float y, TextHitMode mode,
        folia::NodeId tableTextOwner) const
    {
        for (auto hit = mathHits.rbegin(); tableTextOwner.v == 0 && hit != mathHits.rend(); ++hit)
        {
            if (x < hit->rect.left || x > hit->rect.right || y < hit->rect.top || y > hit->rect.bottom) continue;
            auto width = (std::max)(1.0f, hit->rect.right - hit->rect.left);
            auto local = (std::clamp)((x - hit->rect.left) / width, 0.0f, 1.0f);
            auto progress = hit->progressStart + local * (hit->progressEnd - hit->progressStart);
            auto length = hit->sourceSpan.source_range.length();
            auto offset = hit->sourceSpan.source_range.start + static_cast<std::size_t>(std::llround(progress * static_cast<float>(length)));
            return folia::TextPosition{hit->sourceSpan.container_id, (std::min)(offset, hit->sourceSpan.source_range.end), folia::TextAffinity::Downstream};
        }
        if (lines.empty()) return std::nullopt;
        std::optional<std::pair<std::size_t, std::size_t>> tableCell;
        if (tableTextOwner.v != 0)
        {
            for (std::size_t index = 0; index < tables.size() && !tableCell; ++index)
            {
                const auto& cells = tables[index].cells;
                auto cell = std::ranges::find_if(cells, [&](const auto& item) {
                    return item.sourceSpan.container_id == tableTextOwner;
                });
                if (cell == cells.end()) continue;
                tableCell = std::pair{index, static_cast<std::size_t>(cell - cells.begin())};
                x = (std::clamp)(x, cell->rect.left, cell->rect.right);
                y = (std::clamp)(y, cell->rect.top, cell->rect.bottom);
            }
            if (!tableCell) return std::nullopt;
        }
        else tableCell = TableCellAt(x, y);
        std::size_t best = 0;
        float bestDistance = (std::numeric_limits<float>::max)();
        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            if (tableCell && (lines[index].tableIndex != tableCell->first
                || lines[index].cellIndex != tableCell->second)) continue;
            auto const& rect = lines[index].rect;
            auto distance = y < rect.top ? rect.top - y : y > rect.bottom ? y - rect.bottom : 0.0f;
            if (lines[index].tableIndex < tables.size())
            {
                if (x < rect.left) distance += rect.left - x;
                else if (x > rect.right) distance += x - rect.right;
            }
            if (distance < bestDistance) { bestDistance = distance; best = index; }
            if (distance == 0.0f) break;
        }
        if (bestDistance == (std::numeric_limits<float>::max)()) return std::nullopt;
        auto const& line = lines[best];
        if (line.blockIndex >= blocks.size() || line.SourceSpans().empty()) return std::nullopt;
        auto const& block = blocks[line.blockIndex];
        auto const& firstSpan = line.SourceSpans().front();
        auto const& lastSpan = line.SourceSpans().back();
        auto start = folia::TextPosition{firstSpan.container_id, firstSpan.source_range.start, folia::TextAffinity::Downstream};
        auto end = folia::TextPosition{lastSpan.container_id, lastSpan.source_range.end, folia::TextAffinity::Upstream};
        const bool exact = mode == TextHitMode::VisibleCharacter;
        if (exact && (bestDistance > 0 || block.thematicBreak || line.displayStart == line.displayEnd)) return std::nullopt;
        if (block.thematicBreak) return y < (block.rect.top + block.rect.bottom) * 0.5f ? start : end;
        IDWriteTextLayout* layout = block.layout.Get();
        auto textOrigin = block.textOrigin;
        auto const* mapping = &block.displayToSource;
        if (line.tableIndex < tables.size() && line.cellIndex < tables[line.tableIndex].cells.size())
        {
            auto const& cell = tables[line.tableIndex].cells[line.cellIndex];
            layout = cell.layout.Get();
            textOrigin = cell.textOrigin;
            mapping = &cell.displayToSource;
        }
        if (!layout) return exact ? std::nullopt : std::optional{start};
        BOOL trailing = FALSE, inside = FALSE;
        DWRITE_HIT_TEST_METRICS metrics{};
        auto localY = (exact ? y : (line.rect.top + line.rect.bottom) * 0.5f) - textOrigin.y;
        if (FAILED(layout->HitTestPoint(x - textOrigin.x, localY, &trailing, &inside, &metrics)))
            return exact ? std::nullopt : std::optional{start};
        if (exact && (!inside || !metrics.length || metrics.textPosition < line.displayStart || metrics.textPosition >= line.displayEnd))
            return std::nullopt;
        auto displayPos = static_cast<std::size_t>(metrics.textPosition + (trailing && mode == TextHitMode::Caret ? metrics.length : 0));
        displayPos = (std::clamp)(displayPos, static_cast<std::size_t>(line.displayStart), static_cast<std::size_t>(line.displayEnd));
        auto resolved = ResolveMappedPosition(line, *mapping, displayPos);
        auto position = resolved.value_or(end);
        auto nextWrap = best + 1 < lines.size() && lines[best + 1].blockIndex == line.blockIndex
            && lines[best + 1].cellIndex == line.cellIndex
            && lines[best + 1].wrapContinuation && lines[best + 1].displayStart == line.displayEnd;
        auto sourceSpan = SourceSpanFor(line, position.container_id);
        if (sourceSpan && position.source_offset == sourceSpan->source_range.end && nextWrap)
            position.affinity = folia::TextAffinity::Upstream;
        return position;
    }

    std::optional<folia::TextPosition> EditorInteractionMap::TaskCheckboxAt(float x, float y) const
    {
        for (auto hit = taskCheckboxHits.rbegin(); hit != taskCheckboxHits.rend(); ++hit)
        {
            if (x >= hit->rect.left && x <= hit->rect.right
                && y >= hit->rect.top && y <= hit->rect.bottom) return hit->sourcePosition;
        }
        return std::nullopt;
    }

    std::optional<EditorVisualFootnoteHit> EditorInteractionMap::FootnoteAt(float x, float y) const
    {
        for (auto hit = footnoteHits.rbegin(); hit != footnoteHits.rend(); ++hit)
        {
            if (x >= hit->rect.left && x <= hit->rect.right
                && y >= hit->rect.top && y <= hit->rect.bottom) return *hit;
        }
        return std::nullopt;
    }

    std::size_t EditorTableAxis::Nearest(float point) const
    {
        const auto edges = Local();
        const auto position = ToLocal(point);
        auto next = std::ranges::lower_bound(edges, position);
        if (next == edges.end()) return edges.size() - 1;
        if (next != edges.begin() && position - *(next - 1) < *next - position) --next;
        return static_cast<std::size_t>(next - edges.begin());
    }

    std::size_t EditorTableAxis::Interval(float point) const
    {
        const auto edges = Local();
        auto next = std::ranges::upper_bound(edges, ToLocal(point));
        if (next == edges.begin()) return 0;
        return (std::min)(edges.size() - 2, static_cast<std::size_t>(next - edges.begin() - 1));
    }

    namespace
    {
        bool HasTableGrid(EditorVisualTable const& table)
        {
            return table.editable && table.rowCount != 0 && table.columnCount != 0
                && table.rowBoundaries.size() > table.rowCount
                && table.columnBoundaries.size() > table.columnCount
                && !table.SourceSpans().empty();
        }

        bool Contains(D2D1_RECT_F bounds, float x, float y)
        {
            return x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom;
        }
    }

    const EditorTableControl* EditorTableControls::Hit() const
    {
        const auto visible = Visible();
        const auto result = std::ranges::find_if(visible, &EditorTableControl::hot);
        return result == visible.end() ? nullptr : &*result;
    }

    EditorTableControls TableControlsAt(EditorVisualTable const& table, float x, float y)
    {
        EditorTableControls controls;
        if (!HasTableGrid(table)) return controls;
        using Kind = EditorTableActionKind;
        const auto& rect = table.rect;
        const auto& rows = table.rowBoundaries;
        const auto& columns = table.columnBoundaries;
        const auto spans = table.SourceSpans();
        auto bestDistance = (std::numeric_limits<float>::max)();
        EditorTableControl* closest = nullptr;
        auto add = [&](Kind kind, std::size_t index, std::size_t cell, D2D1_RECT_F bounds,
            float width = 0.0f, bool draggable = true) {
            const auto owner = cell < spans.size() ? spans[cell] : spans.front();
            auto& control = controls.items[controls.count++];
            control = {{kind, {owner.container_id, 0}, index, width, table.id, draggable}, bounds};
            constexpr float padding = 3.0f;
            if (!Contains({bounds.left - padding, bounds.top - padding,
                    bounds.right + padding, bounds.bottom + padding}, x, y)) return;
            const auto dx = x - std::midpoint(bounds.left, bounds.right);
            const auto dy = y - std::midpoint(bounds.top, bounds.bottom);
            const auto distance = kind == Kind::ResizeColumn ? dx * dx : dx * dx + dy * dy;
            if (distance >= bestDistance) return;
            if (closest) closest->hot = false;
            closest = &control;
            closest->hot = true;
            bestDistance = distance;
        };
        if (table.hiddenRows && Contains(table.foldBar, x, y))
        {
            add(Kind::Unfold, 0, 0, table.foldBar);
            return controls;
        }

        const float header = rect.top + table.stickyOffset;
        if (!Contains({rect.left - 48, header - 46, rect.right + 30, rect.bottom + 30}, x, y))
            return controls;
        const auto visualRow = y < header ? 0 : table.headerRows && y < rows[table.headerRows] + table.stickyOffset
            ? rows.Interval(y - table.stickyOffset) : rows.Interval(y);
        const auto visualColumn = table.frozenWidth > 0 && x < rect.left + table.frozenWidth ? 0 : columns.Interval(x);
        auto button = [&](Kind kind, std::size_t index, std::size_t cell, float cx, float cy) {
            add(kind, index, cell, {cx - 12, cy - 12, cx + 12, cy + 12});
        };
        if (table.selection && table.selectionHandle)
        {
            const auto& last = table.selection->last;
            const auto [cx, cy] = *table.selectionHandle;
            add(Kind::Fill, 0, last.row * table.columnCount + last.column, {cx - 7, cy - 7, cx + 7, cy + 7});
        }
        const auto reversed = table.cells | std::views::reverse;
        const auto cell = std::ranges::find_if(reversed, [&](auto const& candidate) {
            const auto& box = candidate.checkboxRect;
            return candidate.checkbox && Contains({box.left - 3, box.top - 3, box.right + 3, box.bottom + 3}, x, y);
        });
        if (cell != reversed.end())
            add(Kind::Checkbox, 0, cell->row * table.columnCount + cell->column, cell->checkboxRect);

        const auto edge = columns.Nearest(x);
        if (edge > 0 && std::fabs(x - columns[edge]) <= 9 && columns[edge] >= rect.left + table.frozenWidth
            && y >= header - 22 && y <= header + 13)
            add(Kind::ResizeColumn, edge - 1, edge - 1,
                {columns[edge] - 6, header - 19, columns[edge] + 6, header + 10}, columns[edge] - columns[edge - 1]);

        button(Kind::Menu, 0, 0, rect.left - 13, header - 14);
        const auto middleX = std::midpoint(rect.left, rect.right);
        const auto middleY = std::midpoint(header, rect.bottom);
        if (!table.hiddenRows)
        {
            button(Kind::InsertRow, (std::min)(table.headerRows, table.rowCount), 0, middleX, header - 23);
            button(Kind::InsertRow, table.rowCount, (table.rowCount - 1) * table.columnCount, middleX, rect.bottom + 9);
        }
        button(Kind::InsertColumn, 0, 0, rect.left - 28, middleY);
        button(Kind::InsertColumn, table.columnCount, table.columnCount - 1, rect.right + 10, middleY);
        if (!table.hiddenRows && x >= rect.left - 22
            && x <= (std::min)(rect.left + 44, rect.right) && y >= header && y < rect.bottom)
        {
            const auto rowMidpoint = std::midpoint(rows[visualRow], rows[visualRow + 1])
                + (visualRow < table.headerRows ? table.stickyOffset : 0);
            add(Kind::DragRow, visualRow, visualRow * table.columnCount,
                {rect.left - 15, rowMidpoint - 12, rect.left + 9, rowMidpoint + 12},
                0.0f, visualRow >= table.headerRows && (!table.dataEnd || visualRow < table.dataEnd));
        }
        if (y >= header - 18 && y <= rows[table.headerRows] + table.stickyOffset
            && x >= rect.left && x < rect.right)
        {
            const auto columnMidpoint = visualColumn == 0 && table.frozenWidth > 0
                ? rect.left + table.frozenWidth / 2 : std::midpoint(columns[visualColumn], columns[visualColumn + 1]);
            if (columnMidpoint >= rect.left + 12 && columnMidpoint <= rect.right - 12)
                button(Kind::DragColumn, visualColumn, visualColumn, columnMidpoint, (std::max)(header, 12.0f));
        }
        return controls;
    }

    EditorTableControls EditorInteractionMap::TableControlsAt(float x, float y) const
    {
        for (auto const& table : tables)
            if (auto controls = editor::TableControlsAt(table, x, y); controls.count) return controls;
        return {};
    }

    std::optional<std::pair<std::size_t, std::size_t>> EditorInteractionMap::TableCellAt(float x, float y) const
    {
        auto containsPoint = [&](D2D1_RECT_F const& rect) {
            return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
        };
        for (std::size_t tableIndex = 0; tableIndex < tables.size(); ++tableIndex)
        {
            auto const& table = tables[tableIndex];
            if (!containsPoint(table.rect)) continue;
            auto backwards = table.cells | std::views::reverse;
            auto cell = std::ranges::find_if(backwards,
                [&](auto const& candidate) { return containsPoint(candidate.rect); });
            if (cell != backwards.end())
                return std::pair{tableIndex, static_cast<std::size_t>(cell.base() - table.cells.begin() - 1)};
        }
        return std::nullopt;
    }

    std::optional<folia::TableCoordinate> EditorInteractionMap::TableFillTargetAt(
        folia::NodeId owner, float x, float y) const
    {
        const auto table = std::ranges::find(tables, owner, &EditorVisualTable::id);
        if (table == tables.end() || !HasTableGrid(*table) || !table->selection) return std::nullopt;
        auto interval = [](EditorTableAxis const& edges, float position) {
            if (position <= edges.back()) return edges.Interval(position);
            const auto stride = (std::max)(20.0f, edges.back() - edges[edges.size() - 2]);
            return edges.size() - 1 + static_cast<std::size_t>((std::min)(4095.0f, (position - edges.back()) / stride));
        };
        const auto& area = *table->selection;
        folia::TableCoordinate target{interval(table->rowBoundaries, y), interval(table->columnBoundaries, x)};
        if (area.contains(target)) return std::nullopt;
        return target;
    }

    std::optional<std::size_t> EditorInteractionMap::TableDropIndexAt(
        std::optional<EditorTableAction> const& draggedAction, float x, float y, bool rows) const
    {
        for (auto const& table : tables)
        {
            if (!HasTableGrid(table)) continue;
            if (rows && (x < table.rect.left - 60.0f || x > table.rect.right + 20.0f)) continue;
            if (!rows && (y < table.rect.top - 40.0f || y > table.rect.bottom + 20.0f)) continue;
            if (draggedAction) {
                const auto matches = draggedAction->tableId.v != 0 ? table.id == draggedAction->tableId
                    : std::ranges::any_of(table.SourceSpans(), [&](auto const& span) {
                        return span.container_id == draggedAction->sourcePosition.container_id;
                    });
                if (!matches) continue;
            }
            if (rows) {
                const auto edge = table.rowBoundaries.Nearest(y);
                const auto end = (std::max)(table.headerRows, table.dataEnd ? table.dataEnd : table.rowCount);
                return (std::clamp)(edge, table.headerRows, end);
            }
            return table.columnBoundaries.Nearest(x);
        }
        return std::nullopt;
    }

    std::optional<folia::TextPosition> EditorInteractionMap::MoveCaretVertically(
        folia::TextPosition position,
        bool down,
        float& goalX,
        float bodyLineHeight) const
    {
        auto current = LineIndexFor(position);
        if (!current || lines.empty()) return std::nullopt;
        auto x = goalX;
        if (x < 0.0f)
        {
            auto rect = CaretRectOnLine(lines[*current], position, bodyLineHeight);
            x = rect ? rect->left : lines[*current].rect.left;
            goalX = x;
        }
        if (lines.front().SourceSpans().empty() || lines.back().SourceSpans().empty()) return std::nullopt;
        if (!down && *current == 0)
        {
            auto const& span = lines.front().SourceSpans().front();
            return folia::TextPosition{span.container_id, span.source_range.start, folia::TextAffinity::Downstream};
        }
        if (down && *current + 1 >= lines.size())
        {
            auto const& span = lines.back().SourceSpans().back();
            return folia::TextPosition{span.container_id, span.source_range.end, folia::TextAffinity::Upstream};
        }
        auto targetIndex = down ? *current + 1 : *current - 1;
        auto const& currentLine = lines[*current];
        if (currentLine.tableIndex < tables.size() && currentLine.cellIndex < tables[currentLine.tableIndex].cells.size())
        {
            auto const& table = tables[currentLine.tableIndex];
            auto const& cell = table.cells[currentLine.cellIndex];
            auto sameCell = lines[targetIndex].tableIndex == currentLine.tableIndex && lines[targetIndex].cellIndex == currentLine.cellIndex;
            auto matchRow = [&](EditorVisualLine const& line, std::size_t row) {
                if (line.tableIndex != currentLine.tableIndex || line.cellIndex >= table.cells.size()) return false;
                auto const& candidate = table.cells[line.cellIndex];
                return candidate.row <= row && row < candidate.row + candidate.rowSpan
                    && candidate.column <= cell.column && cell.column < candidate.column + candidate.columnSpan;
            };
            if (!sameCell && down && cell.row + cell.rowSpan < table.rowCount)
            {
                for (std::size_t i = 0; i < lines.size(); ++i)
                    if (matchRow(lines[i], cell.row + cell.rowSpan)) { targetIndex = i; break; }
            }
            else if (!sameCell && !down && cell.row > 0)
            {
                for (std::size_t i = lines.size(); i > 0; --i)
                    if (matchRow(lines[i - 1], cell.row - 1)) { targetIndex = i - 1; break; }
            }
        }
        auto const& target = lines[targetIndex];
        if (target.SourceSpans().empty()) return std::nullopt;
        auto const& firstTargetSpan = target.SourceSpans().front();
        auto targetPosition = folia::TextPosition{firstTargetSpan.container_id, firstTargetSpan.source_range.start, folia::TextAffinity::Downstream};
        auto const& block = blocks[target.blockIndex];
        IDWriteTextLayout* layout = block.layout.Get();
        auto textOrigin = block.textOrigin;
        auto const* mapping = &block.displayToSource;
        if (target.tableIndex < tables.size() && target.cellIndex < tables[target.tableIndex].cells.size())
        {
            auto const& cell = tables[target.tableIndex].cells[target.cellIndex];
            layout = cell.layout.Get();
            textOrigin = cell.textOrigin;
            mapping = &cell.displayToSource;
        }
        if (layout)
        {
            BOOL trailing = FALSE, inside = FALSE;
            DWRITE_HIT_TEST_METRICS metrics{};
            auto targetX = (std::clamp)(x, target.rect.left, target.rect.right - 1.0f);
            if (SUCCEEDED(layout->HitTestPoint(targetX - textOrigin.x, (target.rect.top + target.rect.bottom) * 0.5f - textOrigin.y, &trailing, &inside, &metrics)))
            {
                auto displayPos = static_cast<std::size_t>(metrics.textPosition + (trailing ? metrics.length : 0));
                displayPos = (std::clamp)(displayPos, static_cast<std::size_t>(target.displayStart), static_cast<std::size_t>(target.displayEnd));
                if (auto resolved = ResolveMappedPosition(target, *mapping, displayPos)) targetPosition = *resolved;
            }
        }
        auto targetSpan = SourceSpanFor(target, targetPosition.container_id);
        if (!targetSpan) return std::nullopt;
        targetPosition.source_offset = (std::clamp)(targetPosition.source_offset, targetSpan->source_range.start, targetSpan->source_range.end);
        targetPosition.affinity = folia::TextAffinity::Downstream;
        auto downstreamLine = LineIndexFor(targetPosition);
        targetPosition.affinity = folia::TextAffinity::Upstream;
        auto upstreamLine = LineIndexFor(targetPosition);
        auto newUpstream = (!downstreamLine || *downstreamLine != targetIndex) && upstreamLine && *upstreamLine == targetIndex;
        targetPosition.affinity = newUpstream ? folia::TextAffinity::Upstream : folia::TextAffinity::Downstream;
        return targetPosition;
    }
}
