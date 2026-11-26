#include "pch.h"
#include "editor/rendering/EditorCaretPresentation.h"

import folia.markdown.table_metadata;

namespace winrt::Folia {
    namespace {
    bool CaretTouchesSpan(folia::TextPosition caret, folia::TextSpan const& span)
    {
        if (caret.container_id != span.container_id) return false;
        auto start = span.source_range.start;
        auto end = span.source_range.end;
        auto offset = caret.source_offset;
        if (start <= offset && offset <= end)
        {
            return true;
        }
        return (start > 0 && offset == start - 1) || (end < (std::numeric_limits<std::size_t>::max)() && offset == end + 1);
    }

    }
    EditorInlinePresentation ResolveInlinePresentation(folia::InlineRenderItem const& item, folia::TextPosition caret) {
        using K = folia::InlineRenderItem::Kind;
        using P = EditorInlinePresentation;
        auto const& payload = item.special();
        if (item.kind == K::Marker) {
            if (payload.visibility == folia::MarkerVisibility::HiddenButEditable) return P::Hidden;
            if (payload.marker_role == folia::MarkerRole::Heading && caret.container_id != item.source_span.container_id)
                return P::Hidden;
            if (payload.marker_construct && payload.visibility == folia::MarkerVisibility::WhenCaretInsideNode
                && !(caret.container_id == item.source_span.container_id
                    && caret.source_offset >= payload.marker_construct->start
                    && caret.source_offset <= payload.marker_construct->end)) return P::Hidden;
            return P::Rendered;
        }
        if (item.kind == K::Math && payload.semantic().block_source)
            return caret.container_id == item.source_span.container_id ? P::Source : P::Rendered;
        if (item.kind == K::Math || item.kind == K::Link || item.kind == K::Image)
            return CaretTouchesSpan(caret, item.source_span) ? P::Source : P::Rendered;
        return P::Rendered;
    }

    std::optional<std::size_t> EditorTableCellIndex(
        std::span<const folia::TextSpan> cells, folia::NodeId owner, const folia::OwnerIndex& index)
    {
        if (cells.empty() || owner.v == 0) return std::nullopt;
        const auto first = index.find(cells.front().container_id.v);
        const auto active = index.find(owner.v);
        if (!first || !active || active->second < first->second) return std::nullopt;
        const auto cell = active->second - first->second;
        return cell < cells.size() && cells[cell].container_id == owner ? std::optional{cell} : std::nullopt;
    }

    std::optional<std::size_t> EditorTableCheckboxIndex(folia::RenderBlockSpecial const& table, std::size_t cell)
    {
        if (table.table_checkboxes.empty()) return std::nullopt;
        const auto columns = table.column_count;
        if (const auto* merge = folia::table_merge_at(table.table_metadata.get(),
                {cell / columns, cell % columns}, table.row_count, columns)) {
            for (auto row = merge->first.row; row <= merge->last.row; ++row)
                for (auto column = merge->first.column; column <= merge->last.column; ++column) {
                    const auto part = row * columns + column;
                    if (!table.table_cell_spans[part].source_range.empty())
                        return table.table_checkboxes.contains(part) ? std::optional{part} : std::nullopt;
                }
        }
        return table.table_checkboxes.contains(cell) ? std::optional{cell} : std::nullopt;
    }

    EditorCaretPresentationState CaretPresentationState(folia::RenderBlock const& block, folia::TextPosition caret,
        const folia::OwnerIndex& index) {
        EditorCaretPresentationState state;
        using K = folia::InlineRenderItem::Kind;
        using P = EditorInlinePresentation;
        auto inlines = [&](auto const& self, auto const& items) -> void {
            for (auto const& item : items) {
                auto mode = ResolveInlinePresentation(item, caret);
                auto const& payload = item.special();
                if (item.kind == K::Marker && mode == P::Rendered
                    && (payload.marker_role == folia::MarkerRole::Heading
                        || (payload.marker_construct && payload.visibility == folia::MarkerVisibility::WhenCaretInsideNode)))
                    state.push_back({item.source_span, mode, {}});
                if (mode == P::Source)
                    state.push_back({item.source_span, mode,
                        item.kind == K::Math ? std::optional{caret.source_offset} : std::nullopt});
                if (item.kind == K::Link && mode == P::Rendered) self(self, payload.semantic().children);
            }
        };
        auto visit = [&](auto const& self, folia::RenderBlock const& source) -> void {
            if (source.source_mode || source.source_code) return;
            if (source.kind == folia::RenderBlockKind::Math) {
                if (caret.container_id == source.id) state.push_back({source.source_span, P::Source, caret.source_offset});
            } else if (source.kind == folia::RenderBlockKind::Code) {
                return;
            } else if (source.kind == folia::RenderBlockKind::Table) {
                const auto& table = source.special();
                if (auto cell = EditorTableCellIndex(table.table_cell_spans, caret.container_id, index)) {
                    if (auto checkbox = EditorTableCheckboxIndex(table, *cell))
                        state.push_back({table.table_cell_spans[*checkbox], P::Source, {}});
                    if (table.table_metadata && table.table_metadata->folded
                        && *cell >= table.table_metadata->header_rows * table.column_count)
                        state.push_back({source.source_span, P::Source, {}});
                    inlines(inlines, table.table_cells[*cell]);
                }
            } else if (!source.inline_items.empty()) inlines(inlines, source.inline_items);
            else for (auto const& child : source.child_blocks) self(self, child);
        };
        visit(visit, block);
        return state;
    }
}
