#include "pch.h"
#include "editor/rendering/EditorCaretPresentation.h"

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

    EditorCaretPresentationState CaretPresentationState(folia::RenderBlock const& block, folia::TextPosition caret) {
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
                for (auto const& cell : source.special().table_cells) inlines(inlines, cell);
            } else if (!source.inline_items.empty()) inlines(inlines, source.inline_items);
            else for (auto const& child : source.child_blocks) self(self, child);
        };
        visit(visit, block);
        return state;
    }
}
