#include "pch.h"

import folia.platform.editor_inline_layout;
#include "editor/rendering/EditorContentPreparation.h"

namespace winrt::Folia
{
    using folia::platform::editor::CreateInlinePlaceholder;

    void ApplyMathInlineObjects(IDWriteTextLayout* layout, std::vector<DisplayInlineText::MathOverlay> const& overlays)
    {
        if (!layout) return;
        if (!overlays.empty()) layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_DEFAULT, 0.0f, 0.0f);
        for (auto const& overlay : overlays)
        {
            auto const& fragment = overlay.fragment;
            auto baseline = (std::clamp)(fragment.height + fragment.verticalAlign, 0.0f, fragment.height);
            auto object = CreateInlinePlaceholder(
                {overlay.leadingSpace + fragment.width, fragment.height, baseline, FALSE},
                fragment.breakBefore ? DWRITE_BREAK_CONDITION_CAN_BREAK : DWRITE_BREAK_CONDITION_NEUTRAL);
            if (object)
            {
                layout->SetInlineObject(object.Get(), DWRITE_TEXT_RANGE{ overlay.displayStart, 1 });
                // Mathematical decorations are painted over the SVG once by
                // the shared math painter, not at the surrounding font's axis.
                layout->SetStrikethrough(FALSE, DWRITE_TEXT_RANGE{overlay.displayStart, 1});
            }
        }
    }

    void ApplyBlockSpacingInlineObjects(IDWriteTextLayout* layout, DisplayInlineText const& display)
    {
        folia::platform::editor::ApplyEditorBlockSpacing(
            layout, display.text, display.blockSpacingOverlays);
    }

    void ApplyInlinePlaceholder(IDWriteTextLayout* layout, UINT32 displayStart, float width, float height, float baseline)
    {
        if (!layout) return;
        auto object = CreateInlinePlaceholder({width, height, baseline, FALSE});
        if (object) layout->SetInlineObject(object.Get(), DWRITE_TEXT_RANGE{ displayStart, 1 });
    }

    void ApplyTaskCheckboxInlineObjects(
        IDWriteTextLayout* layout,
        std::vector<DisplayInlineText::TaskCheckboxOverlay> const& overlays)
    {
        if (!layout) return;
        for (auto const& overlay : overlays)
            ApplyInlinePlaceholder(layout, overlay.displayStart, overlay.advance, overlay.height, overlay.baseline);
    }

    void ApplyIndentInlineObjects(IDWriteTextLayout* layout, std::vector<DisplayInlineText::IndentOverlay> const& overlays)
    {
        if (!layout) return;
        for (auto const& overlay : overlays)
        {
            auto object = CreateInlinePlaceholder(
                {overlay.width, 1.0f, 1.0f, FALSE}, DWRITE_BREAK_CONDITION_MUST_BREAK, DWRITE_BREAK_CONDITION_MAY_NOT_BREAK);
            if (object) layout->SetInlineObject(object.Get(), DWRITE_TEXT_RANGE{ overlay.displayStart, 1 });
        }
    }

}
