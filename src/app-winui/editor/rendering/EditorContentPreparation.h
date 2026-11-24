#pragma once

import folia.highlight;
import folia.markdown.ids;
import folia.markdown.image_dimension;
import folia.core.render_model;
import folia.markdown.text_edit;
import folia.markdown.source_offset_map;
import folia.platform.editor_display_mapping;
import folia.platform.editor_inline_projection;
import folia.platform.editor_block_spacing;

#include "windows/media/math/MathJaxRenderer.h"
#include "windows/media/mermaid/MermaidRenderer.h"
#include "windows/media/svg/SvgNormalizer.h"
#include "editor/rendering/EditorSyntaxIcons.h"

namespace winrt::Folia
{
    using folia::highlight::SyntaxHighlightKind;
    using folia::highlight::SyntaxHighlightRange;
    using folia::highlight::TreeSitterHighlighter;
    using folia::platform::editor::EditorDisplayMapping;
    using folia::platform::editor::EditorDisplayPosition;
    using folia::platform::editor::EditorDisplayPositionKind;
    using folia::platform::editor::EditorFootnoteControlKind;
    using folia::platform::editor::InlineItemsEndPosition;
    using folia::platform::editor::InlineStyleRange;
    using folia::platform::editor::SyntaxIconPlacement;

    inline std::optional<float> ResolveImageDimension(
        std::optional<folia::ImageDimension> const& dimension,
        std::optional<float> percentBasis = std::nullopt)
    {
        if (!dimension) return std::nullopt;
        if (dimension->unit == folia::ImageDimensionUnit::Pixels) return dimension->value;
        if (!percentBasis || *percentBasis <= 0.0f) return std::nullopt;
        return *percentBasis * dimension->value / 100.0f;
    }

    struct DisplayInlineText : folia::platform::editor::InlineTextProjection
    {
        using BlockSpacingOverlay = folia::platform::editor::EditorBlockSpacing;
        struct MathOverlay
        {
            std::uint32_t displayStart = 0;
            folia::platform::MathJaxSvgFragment fragment;
            float leadingSpace = 0.0f;
            folia::TextSpan sourceSpan;
            float progressStart = 0.0f;
            float progressEnd = 1.0f;
            bool strikethrough = false;
            bool displayMath = false;
            D2D1_COLOR_F foreground{0, 0, 0, 1};
        };

        struct MathPreview
        {
            folia::platform::MathJaxSvg svg;
            folia::TextSpan contentSpan;
            bool strikethrough = false;
            D2D1_COLOR_F foreground{0, 0, 0, 1};
        };
        struct MathSource
        {
            std::u32string tex;
            folia::TextSpan span;
            // Block projections can include indentation/fences in source.
            // Inline math is linear and leaves this projection empty.
            folia::SourceOffsetMap toSource;
        };
        struct MathBracket
        {
            folia::TextSpan span;
            bool matched = false;
        };

        struct ImageOverlay
        {
            std::uint32_t displayStart = 0;
            folia::TextSpan sourceSpan;
            std::string source;
            std::string alt;
            std::optional<folia::ImageDimension> width;
            std::optional<folia::ImageDimension> height;
            bool block = false;
        };

        struct MermaidOverlay
        {
            std::uint32_t displayStart = 0;
            folia::platform::MermaidSvg svg;
            folia::TextSpan sourceSpan;
        };

        struct MermaidPreview
        {
            folia::platform::MermaidSvg svg;
        };

        struct IndentOverlay
        {
            std::uint32_t displayStart = 0;
            float width = 0.0f;
        };

        struct TaskCheckboxOverlay
        {
            std::uint32_t displayStart = 0;
            folia::TextPosition sourcePosition;
            bool checked = false;
            float advance = 0.0f;
            float height = 0.0f;
            float baseline = 0.0f;
            float boxSize = 0.0f;
        };

        struct FootnoteOverlay
        {
            std::uint32_t displayStart = 0;
            std::uint32_t displayLength = 0;
            folia::TextSpan sourceSpan;
            std::string label;
            EditorFootnoteControlKind kind = EditorFootnoteControlKind::Reference;
        };

        std::vector<MathOverlay> mathOverlays;
        std::vector<MathPreview> mathPreviews;
        std::vector<MathSource> mathSources;
        std::vector<MathBracket> mathBrackets;
        std::vector<ImageOverlay> imageOverlays;
        std::vector<MermaidOverlay> mermaidOverlays;
        std::vector<MermaidPreview> mermaidPreviews;
        std::vector<IndentOverlay> indentOverlays;
        std::vector<BlockSpacingOverlay> blockSpacingOverlays;
        std::vector<TaskCheckboxOverlay> taskCheckboxOverlays;
        std::vector<FootnoteOverlay> footnoteOverlays;
        bool pendingMermaid = false;
        bool pendingMath = false;
    };

    std::wstring ToWide(std::u32string_view text);
    std::string SvgColor(D2D1_COLOR_F color);
    D2D1_COLOR_F InlineForeground(folia::InlineStyle const& style, D2D1_COLOR_F fallback);
    bool IsMermaidLanguage(std::optional<std::string> const& language);
    void MergeDisplayText(DisplayInlineText& target, DisplayInlineText source);
    void InsertCompletionPreview(DisplayInlineText&, folia::TextPosition, std::u32string_view, std::u32string_view before = {});
    void AppendMathFragments(
        DisplayInlineText& display,
        folia::platform::MathJaxSvg const& math,
        folia::TextSpan sourceSpan,
        bool editing,
        folia::InlineStyle style, D2D1_COLOR_F foreground);
    void ApplyInlinePlaceholder(IDWriteTextLayout* layout, UINT32 displayStart, float width, float height, float baseline);
    void ApplyMathInlineObjects(IDWriteTextLayout* layout, std::vector<DisplayInlineText::MathOverlay> const& overlays);
    void ApplyBlockSpacingInlineObjects(IDWriteTextLayout* layout, DisplayInlineText const& display);
    void ApplyIndentInlineObjects(IDWriteTextLayout* layout, std::vector<DisplayInlineText::IndentOverlay> const& overlays);
    void ApplyTaskCheckboxInlineObjects(IDWriteTextLayout* layout, std::vector<DisplayInlineText::TaskCheckboxOverlay> const& overlays);
    DisplayInlineText BuildMathPreviewText(DisplayInlineText::MathPreview const& preview);
    void HighlightMathSources(DisplayInlineText& display, folia::TextPosition caret, TreeSitterHighlighter& highlighter);
    DisplayInlineText BuildDisplayInlineText(
        std::vector<folia::InlineRenderItem> const& items,
        folia::TextPosition caret,
        EditorDisplayPosition sourceEnd,
        folia::platform::MathJaxRenderer& mathJax,
        folia::platform::SvgNormalizer& svgNormalizer,
        D2D1_COLOR_F svgColor,
        float fontSize,
        float containerWidth,
        bool svgSupported,
        bool requestMath,
        bool highPriority);
    DisplayInlineText BuildCodeBlockText(folia::RenderBlock const& block, folia::TextPosition caret, TreeSitterHighlighter& highlighter);
    DisplayInlineText BuildMathBlockText(
        folia::RenderBlock const& block,
        folia::TextPosition caret,
        folia::platform::MathJaxRenderer& mathJax,
        folia::platform::SvgNormalizer& svgNormalizer,
        D2D1_COLOR_F svgColor,
        float fontSize,
        float containerWidth,
        bool svgSupported,
        bool requestMath,
        bool highPriority,
        std::size_t continuationIndentColumns = 0);
}
