#pragma once

import folia.core.render_model;
import folia.markdown.table_metadata;

namespace winrt::Folia
{
    struct EditorDocumentMetrics
    {
        float left = 0.0f;
        float right = 0.0f;
        float width = 0.0f;
        float sourceGutterLeft = 4.0f;
        float sourceGutterWidth = 0.0f;
    };

    inline float EditorHorizontalInset(float surfaceWidth, float horizontalPadding, float marginScale = 1.0f)
    {
        auto inset = (std::min)(horizontalPadding, (std::max)(12.0f, surfaceWidth * 0.06f));
        return (std::min)(inset * marginScale, surfaceWidth * 0.25f);
    }

    // Background geometry and foreground painting must use identical width
    // constraints, including the source-mode line-number gutter.
    inline EditorDocumentMetrics EditorDocumentLayoutMetrics(
        folia::RenderModel const& model, float surfaceWidth,
        float horizontalPadding, float codeFontSize, float marginScale = 1.0f)
    {
        auto padding = EditorHorizontalInset(surfaceWidth, horizontalPadding, marginScale);
        auto sourceDocument = !model.blocks.empty() && model.blocks.front().source_mode;
        auto lines = sourceDocument
            ? (std::max)(std::uint32_t{1}, model.blocks.back().source_line_number) : std::uint32_t{0};
        auto digits = sourceDocument ? std::to_wstring(lines).size() : std::size_t{0};
        auto desiredGutter = sourceDocument
            ? (std::max)(24.0f, codeFontSize * 0.64f * static_cast<float>(digits) + 12.0f) : 0.0f;
        auto availableGutter = (std::max)(0.0f, surfaceWidth - padding - 80.0f - 8.0f);
        auto gutter = (std::min)(desiredGutter, availableGutter);
        if (gutter < 20.0f) gutter = 0.0f;
        auto left = gutter > 0.0f ? gutter + 8.0f : padding;
        auto right = (std::max)(left + 1.0f, surfaceWidth - padding);
        return {left, right, right - left, 4.0f, gutter};
    }

    // Rich-text document and formula preparation use identical width inputs.
    inline float EditorDocumentContentWidth(float surfaceWidth, float horizontalPadding)
    {
        auto padding = EditorHorizontalInset(surfaceWidth, horizontalPadding);
        return (std::max)(1.0f, surfaceWidth - 2.0f * padding);
    }

    inline float EditorBlockContentWidth(folia::RenderBlock const& block, float documentWidth)
    {
        auto flow = !block.inline_items.empty()
            && (block.kind == folia::RenderBlockKind::Quote || block.kind == folia::RenderBlockKind::Callout);
        return (std::max)(1.0f, documentWidth
            - (flow ? 0.0f : block.block_style.padding_left)
            - (flow ? 0.0f : block.block_style.padding_right));
    }

    inline float EditorTableCellPadding(folia::TableMetadata const* metadata)
    {
        return metadata && metadata->compact ? 8.0f : 10.0f;
    }

    struct EditorTableMerge
    {
        std::size_t rows = 1;
        std::size_t columns = 1;
    };

    inline std::vector<EditorTableMerge> ResolveEditorTableMerges(
        folia::TableMetadata const* metadata, std::size_t rows, std::size_t columns)
    {
        if (!metadata || metadata->merges.empty()) return {};
        std::vector<EditorTableMerge> spans(rows * columns);
        std::vector<bool> occupied(spans.size());
        for (auto const& merge : metadata->merges)
        {
            if (merge.last.row >= rows || merge.last.column >= columns) continue;
            auto eachCell = [&](auto visit) {
                for (auto row = merge.first.row; row <= merge.last.row; ++row)
                    for (auto column = merge.first.column; column <= merge.last.column; ++column)
                        if (!visit(row * columns + column)) return false;
                return true;
            };
            if (!eachCell([&](auto index) { return !occupied[index]; })) continue;
            eachCell([&](auto index) { occupied[index] = true; spans[index] = {0, 0}; return true; });
            spans[merge.first.row * columns + merge.first.column] = {
                merge.last.row - merge.first.row + 1, merge.last.column - merge.first.column + 1};
        }
        return spans;
    }

    inline folia::TableCellAppearance EditorTableCellAppearance(
        folia::TableMetadata const* metadata, std::size_t row, std::size_t column)
    {
        if (metadata)
            if (const auto found = metadata->cells.find({row, column}); found != metadata->cells.end())
                return found->second;
        return {};
    }

    inline float EditorTableCellFontSize(
        float bodySize, folia::TableMetadata const* metadata, const folia::TableCellAppearance& appearance)
    {
        return bodySize * 0.93f * (metadata ? metadata->font_scale : 1.0f) * appearance.font_scale;
    }

    inline std::vector<float> EditorTableColumnBoundaries(
        folia::RenderBlockSpecial const& table, float available)
    {
        const auto count = table.column_count;
        std::vector<float> boundaries(count + 1);
        if (count == 0) return boundaries;
        const auto& metadata = table.table_metadata;
        auto explicitWidth = [&](std::size_t column) {
            const auto savedWidth = metadata && column < metadata->widths.size()
                ? metadata->widths[column] : 0.0f;
            return (std::max)(0.0f, savedWidth);
        };
        float fixed = 0.0f;
        std::size_t automatic = 0;
        for (std::size_t column = 0; column < count; ++column)
        {
            auto width = explicitWidth(column);
            fixed += width;
            automatic += width == 0.0f;
        }
        const auto autoWidth = automatic == 0 ? 0.0f
            : (std::max)(40.0f, (available - fixed) / static_cast<float>(automatic));
        for (std::size_t column = 0; column < count; ++column)
        {
            const auto width = explicitWidth(column);
            boundaries[column + 1] = boundaries[column] + (width > 0.0f ? width : autoWidth);
        }
        if (boundaries.back() > available && !(metadata && metadata->wide))
        {
            auto scale = available / boundaries.back();
            for (auto& boundary : boundaries) boundary *= scale;
        }
        return boundaries;
    }

    inline float EditorTableCellContentWidth(float width, folia::TableMetadata const* metadata)
    {
        return (std::max)(1.0f, width - 2.0f * EditorTableCellPadding(metadata));
    }
}
