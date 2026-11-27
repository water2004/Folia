#pragma once

import folia.platform.editor_inline_layout;
import folia.platform.editor_inline_projection;

#include "editor/rendering/EditorRenderResources.h"
#include "editor/rendering/EditorStyleSheet.h"
#include "editor/rendering/layout/EditorFlowLayoutContext.h"

namespace winrt::Folia
{
    using folia::platform::editor::InlineStyleRange;
    struct DisplayInlineText;

    using folia::platform::editor::InlineRangeRectangles;
    using folia::platform::editor::SyntaxIconRectangles;

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
        void ApplyNaturalLineSpacing(IDWriteTextLayout* layout) const;
        float MeasureHeight(IDWriteTextLayout* layout, float fallbackHeight,
            bool enforceMinimum = true) const;

    private:
        EditorRenderResources& resources;
        EditorStyleSheet const& styleSheet;
    };
}
