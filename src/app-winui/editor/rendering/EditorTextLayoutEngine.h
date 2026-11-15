#pragma once

#include "editor/rendering/EditorRenderResources.h"
#include "editor/rendering/EditorStyleSheet.h"
#include "editor/rendering/EditorFlowLayoutContext.h"

namespace winrt::Folia
{
    struct InlineStyleRange;
    struct DisplayInlineText;

    std::vector<D2D1_RECT_F> InlineRangeRectangles(
        IDWriteTextLayout* layout, UINT32 start, UINT32 length, D2D1_POINT_2F origin);
    std::vector<D2D1_RECT_F> SyntaxIconRectangles(
        IDWriteTextLayout* layout, InlineStyleRange const& range, D2D1_POINT_2F origin);

    void DrawInlinePresentationBackgrounds(
        EditorRenderResources& resources,
        EditorStyleSheet const& styleSheet,
        IDWriteTextLayout* layout,
        D2D1_POINT_2F origin,
        std::vector<InlineStyleRange> const& ranges);

    struct EditorTextLayoutEngine
    {
        EditorTextLayoutEngine(EditorRenderResources& resources, EditorStyleSheet const& styleSheet);

        ::Microsoft::WRL::ComPtr<IDWriteTextLayout> Create(std::wstring const& text, IDWriteTextFormat* format, float width) const;
        ::Microsoft::WRL::ComPtr<IDWriteTextLayout> CreateFlow(
            DisplayInlineText& display,
            IDWriteTextFormat* format,
            float width,
            std::function<void(IDWriteTextLayout*, DisplayInlineText const&)> const& configure) const;
        IDWriteTextFormat* FormatFor(bool code, std::vector<InlineStyleRange> const& ranges) const;
        EditorFlowTextFormat FormatKindFor(bool code, std::vector<InlineStyleRange> const& ranges) const;
        IDWriteTextFormat* FormatFor(EditorFlowTextFormat format) const;
        float LineHeightFor(bool code, std::vector<InlineStyleRange> const& ranges) const;
        float LineHeightFor(EditorFlowTextFormat format) const;
        void ApplyStyles(IDWriteTextLayout* layout, std::vector<InlineStyleRange> const& ranges) const;
        float MeasureHeight(IDWriteTextLayout* layout, float fallbackHeight,
            bool enforceMinimum = true) const;

    private:
        EditorRenderResources& resources;
        EditorStyleSheet const& styleSheet;
    };
}
