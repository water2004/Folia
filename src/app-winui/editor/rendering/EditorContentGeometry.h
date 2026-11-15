#pragma once

import folia.core.render_model;

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

    // Background geometry and foreground painting must use identical width
    // constraints, including the source-mode line-number gutter.
    inline EditorDocumentMetrics EditorDocumentLayoutMetrics(
        folia::RenderModel const& model, float surfaceWidth,
        float horizontalPadding, float codeFontSize)
    {
        auto padding = (std::min)(horizontalPadding, (std::max)(12.0f, surfaceWidth * 0.06f));
        auto sourceDocument = !model.blocks.empty() && model.blocks.front().source_mode;
        auto lines = sourceDocument
            ? (std::max)(std::uint32_t{1}, model.blocks.back().source_line_number) : std::uint32_t{0};
        auto digits = sourceDocument ? std::to_wstring(lines).size() : std::size_t{0};
        auto desiredGutter = sourceDocument
            ? (std::max)(24.0f, codeFontSize * 0.64f * static_cast<float>(digits) + 12.0f) : 0.0f;
        auto availableGutter = (std::max)(0.0f, surfaceWidth - padding - 14.0f - 80.0f - 8.0f);
        auto gutter = (std::min)(desiredGutter, availableGutter);
        if (gutter < 20.0f) gutter = 0.0f;
        auto left = gutter > 0.0f ? gutter + 8.0f : padding;
        auto right = (std::max)(left + 1.0f, surfaceWidth - padding - 14.0f);
        return {left, right, right - left, 4.0f, gutter};
    }

    // Rich-text document and formula preparation use identical width inputs.
    inline float EditorDocumentContentWidth(float surfaceWidth, float horizontalPadding)
    {
        auto padding = (std::min)(horizontalPadding, (std::max)(12.0f, surfaceWidth * 0.06f));
        return (std::max)(padding + 1.0f, surfaceWidth - padding - 14.0f) - padding;
    }

    inline float EditorBlockContentWidth(folia::RenderBlock const& block, float documentWidth)
    {
        auto flow = !block.inline_items.empty()
            && (block.kind == folia::RenderBlockKind::Quote || block.kind == folia::RenderBlockKind::Callout);
        return (std::max)(1.0f, documentWidth
            - (flow ? 0.0f : block.block_style.padding_left)
            - (flow ? 0.0f : block.block_style.padding_right));
    }

    inline float EditorTableCellContentWidth(float tableWidth, std::size_t columns)
    {
        return (std::max)(1.0f, (std::max)(1.0f, tableWidth)
            / static_cast<float>((std::max)(std::size_t{1}, columns)) - 20.0f);
    }
}
