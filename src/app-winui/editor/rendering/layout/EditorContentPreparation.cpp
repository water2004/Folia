#include "pch.h"
#include "editor/rendering/layout/EditorContentPreparation.h"
#include "editor/rendering/EditorCaretPresentation.h"
#include "editor/rendering/EditorStyleSheet.h"

import folia.core.render_model;
import folia.core.source_style;
import folia.markdown.utf;

namespace winrt::Folia
{
    namespace
    {
        SyntaxHighlightKind SourceSyntax(folia::SourceSyntaxKind kind)
        {
            using K = folia::SourceSyntaxKind;
            switch (kind)
            {
                case K::Marker: return SyntaxHighlightKind::Operator;
                case K::Heading: return SyntaxHighlightKind::Keyword;
                case K::Emphasis: return SyntaxHighlightKind::Property;
                case K::Strong: return SyntaxHighlightKind::Type;
                case K::Strikethrough: return SyntaxHighlightKind::Comment;
                case K::Highlight: return SyntaxHighlightKind::Constant;
                case K::Link: return SyntaxHighlightKind::Property;
                case K::Code: return SyntaxHighlightKind::String;
                case K::Math: return SyntaxHighlightKind::Number;
                case K::Escape: return SyntaxHighlightKind::Preprocessor;
                case K::Entity: return SyntaxHighlightKind::Constant;
                case K::Error: return SyntaxHighlightKind::Preprocessor;
                case K::None: return SyntaxHighlightKind::None;
            }
            return SyntaxHighlightKind::None;
        }
    }

    std::u32string FootnoteSuperscript(std::u32string_view number)
    {
        std::u32string result;
        result.reserve(number.size());
        for (auto value : number)
        {
            switch (value)
            {
                case U'0': result.push_back(U'\u2070'); break;
                case U'1': result.push_back(U'\u00b9'); break;
                case U'2': result.push_back(U'\u00b2'); break;
                case U'3': result.push_back(U'\u00b3'); break;
                case U'4': result.push_back(U'\u2074'); break;
                case U'5': result.push_back(U'\u2075'); break;
                case U'6': result.push_back(U'\u2076'); break;
                case U'7': result.push_back(U'\u2077'); break;
                case U'8': result.push_back(U'\u2078'); break;
                case U'9': result.push_back(U'\u2079'); break;
                default: result.push_back(value); break;
            }
        }
        return result;
    }

    void MergeDisplayText(DisplayInlineText& target, DisplayInlineText source)
    {
        auto offset = folia::platform::editor::MergeInlineProjection(target, std::move(source));
        for (auto& overlay : source.mathOverlays)
        {
            overlay.displayStart += offset;
            target.mathOverlays.push_back(std::move(overlay));
        }
        for (auto& preview : source.mathPreviews)
        {
            target.mathPreviews.push_back(std::move(preview));
        }
        for (auto& math : source.mathSources) target.mathSources.push_back(std::move(math));
        for (auto& bracket : source.mathBrackets) target.mathBrackets.push_back(std::move(bracket));
        for (auto& overlay : source.imageOverlays)
        {
            overlay.displayStart += offset;
            target.imageOverlays.push_back(std::move(overlay));
        }
        for (auto& overlay : source.mermaidOverlays)
        {
            overlay.displayStart += offset;
            target.mermaidOverlays.push_back(std::move(overlay));
        }
        for (auto& preview : source.mermaidPreviews)
        {
            target.mermaidPreviews.push_back(std::move(preview));
        }
        for (auto& overlay : source.indentOverlays)
        {
            overlay.displayStart += offset;
            target.indentOverlays.push_back(std::move(overlay));
        }
        for (auto& overlay : source.blockSpacingOverlays)
        {
            overlay.displayStart += offset;
            target.blockSpacingOverlays.push_back(std::move(overlay));
        }
        for (auto& overlay : source.taskCheckboxOverlays)
        {
            overlay.displayStart += offset;
            target.taskCheckboxOverlays.push_back(std::move(overlay));
        }
        for (auto& overlay : source.footnoteOverlays)
        {
            overlay.displayStart += offset;
            target.footnoteOverlays.push_back(std::move(overlay));
        }
        target.pendingMermaid = target.pendingMermaid || source.pendingMermaid;
        target.pendingMath = target.pendingMath || source.pendingMath;
    }

    static void AppendInlineSource(DisplayInlineText& display, folia::InlineRenderItem const& item)
    {
        auto const& semantic = item.special().semantic();
        if (!semantic.content_range)
        {
            AppendSourceText(display, semantic.source_text, item.source_span, item.style, false);
            return;
        }
        auto content = *semantic.content_range;
        if (item.kind == folia::InlineRenderItem::Kind::Math) {
            auto offset = content.start - item.source_span.source_range.start;
            display.mathSources.push_back({semantic.source_text.substr(offset, content.length()),
                {item.source_span.container_id, content}, {}});
        }
        auto append = [&](folia::SourceRange range, bool marker, SyntaxIcon icon = SyntaxIcon::None) {
            auto offset = range.start - item.source_span.source_range.start;
            auto firstRange = display.ranges.size();
            AppendSourceText(display, std::u32string_view{semantic.source_text}.substr(offset, range.length()),
                {item.source_span.container_id, range}, item.style, marker);
            if (display.ranges.size() > firstRange) display.ranges.back().icon = icon;
        };
        append({item.source_span.source_range.start, content.start}, true, semantic.opening_marker);
        append(content, false);
        append({content.end, item.source_span.source_range.end}, true, semantic.closing_marker);
    }

    void AppendMathFragments(DisplayInlineText& display, folia::platform::MathJaxSvg const& math, folia::TextSpan sourceSpan, bool editing, folia::InlineStyle style, D2D1_COLOR_F foreground)
    {
        auto sourceStart = sourceSpan.source_range.start;
        auto sourceEnd = sourceSpan.source_range.end;
        if (editing)
        {
            AppendGeneratedText(display, U"\u2003", {sourceSpan.container_id, sourceEnd, folia::TextAffinity::Downstream}, style);
        }
        float progress = 0.0f;
        for (std::size_t index = 0; index < math.fragments.size(); ++index)
        {
            auto const& fragment = math.fragments[index];
            auto length = sourceEnd - sourceStart;
            auto mappedOffset = editing || math.width <= 0.0f
                ? sourceEnd
                : sourceStart + static_cast<std::size_t>(std::floor((progress / math.width) * static_cast<float>(length)));
            if (index > 0 && fragment.breakBefore)
            {
                // MathJax emits fragments at legal line-break opportunities.
                // Keep them in one DirectWrite flow and expose an optional
                // break; a hard newline would force every fragment onto its
                // own line even when the remaining width is sufficient.
                AppendGeneratedText(display, U"\u200B", {sourceSpan.container_id, mappedOffset, folia::TextAffinity::Downstream}, style);
            }
            auto start = static_cast<UINT32>(display.displayToSource.size());
            display.text.push_back(U'\uFFFC');
            display.displayToSource.push_back({
                sourceSpan.container_id,
                mappedOffset,
                folia::TextAffinity::Downstream,
                EditorDisplayPositionKind::Generated});
            display.ranges.push_back(InlineStyleRange{ start, 1, style, false, SyntaxHighlightKind::None });
            auto fragmentStart = math.width > 0.0f ? progress / math.width : 0.0f;
            progress += fragment.breakSpace + fragment.width;
            auto fragmentEnd = math.width > 0.0f ? progress / math.width : 1.0f;
            display.mathOverlays.push_back(DisplayInlineText::MathOverlay{
                start,
                fragment,
                fragment.breakSpace,
                sourceSpan,
                fragmentStart,
                fragmentEnd,
                style.strikethrough,
                math.display,
                foreground,
            });
        }
    }

    DisplayInlineText BuildMathPreviewText(DisplayInlineText::MathPreview const& preview)
    {
        DisplayInlineText display;
        auto style = folia::InlineStyle::plain();
        style.strikethrough = preview.strikethrough;
        AppendMathFragments(
            display,
            preview.svg,
            preview.contentSpan,
            false,
            style, preview.foreground);
        display.displayToSource.push_back({
            preview.contentSpan.container_id,
            preview.contentSpan.source_range.end,
            folia::TextAffinity::Downstream,
            EditorDisplayPositionKind::Generated});
        return display;
    }

    float EditorInlineLayoutStyle::FontSizeFor(folia::InlineStyle const& style) const
    {
        return style.heading_level ? sheet.HeadingSize(*style.heading_level) : baseFontSize;
    }

    DisplayInlineText BuildDisplayInlineText(
        std::vector<folia::InlineRenderItem> const& items,
        folia::TextPosition caret,
        EditorDisplayPosition sourceEnd,
        folia::platform::MathJaxRenderer& mathJax,
        folia::platform::SvgNormalizer& svgNormalizer,
        EditorInlineLayoutStyle const& inlineStyle,
        float containerWidth,
        bool svgSupported,
        bool requestMath,
        bool highPriority)
    {
        DisplayInlineText display;
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            auto const& item = items[index];
            auto presentation = ResolveInlinePresentation(item, caret);
            if (presentation == EditorInlinePresentation::Hidden) continue;
            if (item.kind == folia::InlineRenderItem::Kind::Link)
            {
                if (presentation == EditorInlinePresentation::Source) AppendInlineSource(display, item);
                else MergeDisplayText(display, BuildDisplayInlineText(item.special().semantic().children, caret,
                    {item.source_span.container_id, item.source_span.source_range.end, folia::TextAffinity::Downstream},
                    mathJax, svgNormalizer, inlineStyle, containerWidth, svgSupported, requestMath, highPriority));
                continue;
            }
            if (item.kind == folia::InlineRenderItem::Kind::Image)
            {
                if (presentation == EditorInlinePresentation::Source) AppendInlineSource(display, item);
                else
                {
                    auto displayStart = static_cast<std::uint32_t>(display.displayToSource.size());
                    AppendGeneratedText(display, U"\uFFFC", {item.source_span.container_id, item.source_span.source_range.start, folia::TextAffinity::Downstream}, item.style);
                    display.imageOverlays.push_back(DisplayInlineText::ImageOverlay{
                        displayStart,
                        item.source_span,
                        item.special().semantic().src,
                        item.special().semantic().alt,
                        item.special().semantic().image_width,
                        item.special().semantic().image_height,
                        item.special().semantic().block_image,
                    });
                }
                continue;
            }
            if (item.kind == folia::InlineRenderItem::Kind::Math)
            {
                auto fontSize = inlineStyle.FontSizeFor(item.style);
                if (item.special().semantic().block_source) {
                    folia::RenderBlock block;
                    block.kind = folia::RenderBlockKind::Math;
                    block.id = item.source_span.container_id;
                    block.source_span = item.source_span;
                    block.payload = item.special().semantic().block_source;
                    auto const& offsets = block.special().content_to_source;
                    block.content_span = {block.id, offsets.empty()
                        ? folia::SourceRange{0, block.special().tex.size()}
                        : folia::SourceRange{offsets.front(), offsets.back()}};
                    MergeDisplayText(display, BuildMathBlockText(block, caret, mathJax,
                        svgNormalizer, inlineStyle.foreground, fontSize, containerWidth,
                        svgSupported, requestMath, highPriority,
                        item.special().semantic().continuation_indent_columns));
                    continue;
                }
                if (!svgSupported)
                {
                    AppendInlineSource(display, item);
                    continue;
                }
                auto displayMath = item.special().display == folia::MathDisplayMode::Block;
                auto editing = presentation == EditorInlinePresentation::Source;
                auto purpose = editing ? folia::platform::MathRenderPurpose::Preview : folia::platform::MathRenderPurpose::Document;
                auto tex = folia::cps_to_utf8(item.text);
                auto foreground = InlineForeground(item.style, inlineStyle.foreground);
                auto color = SvgColor(foreground);
                // highPriority identifies the visible band. Offscreen layout
                // reuses existing geometry and must not restart canceled jobs.
                auto [math, pending] = mathJax.Layout(tex, displayMath, fontSize, containerWidth, color, highPriority, purpose);
                display.pendingMath = display.pendingMath || pending;
                auto delimiterLength = item.special().display == folia::MathDisplayMode::Block
                    ? std::size_t{0}
                    : item.special().math_delim == folia::MathDelimiter::InlineParen ? std::size_t{2} : std::size_t{1};
                auto contentStart = (std::min)(item.source_span.source_range.start + delimiterLength, item.source_span.source_range.end);
                auto contentEnd = item.source_span.source_range.end >= delimiterLength
                    ? (std::max)(contentStart, item.source_span.source_range.end - delimiterLength)
                    : contentStart;
                if (!math || !static_cast<bool>(*math))
                {
                    AppendInlineSource(display, item);
                    continue;
                }

                if (editing)
                {
                    AppendInlineSource(display, item);
                    display.mathPreviews.push_back(DisplayInlineText::MathPreview{
                        *math,
                        folia::TextSpan{item.source_span.container_id, {contentStart, contentEnd}},
                        item.style.strikethrough,
                        foreground,
                    });
                    continue;
                }
                AppendMathFragments(display, *math, {item.source_span.container_id, {contentStart, contentEnd}}, false, item.style, foreground);
                continue;
            }
            if (item.kind == folia::InlineRenderItem::Kind::FootnoteReference)
            {
                auto displayStart = static_cast<std::uint32_t>(display.displayToSource.size());
                auto ordinal = item.special().display_text.empty()
                    ? folia::utf8_to_cps(item.special().semantic().footnote_label)
                    : item.special().display_text;
                auto label = FootnoteSuperscript(ordinal);
                AppendGeneratedText(
                    display,
                    label,
                    {item.source_span.container_id, item.source_span.source_range.start, folia::TextAffinity::Downstream},
                    item.style,
                    EditorDisplayPositionKind::BoundaryDecoration);
                auto displayLength = static_cast<std::uint32_t>(folia::utf16_len(label));
                if (!display.ranges.empty()) {
                    display.ranges.back().footnoteControl = EditorFootnoteControlKind::Reference;
                }
                display.footnoteOverlays.push_back({
                    displayStart,
                    displayLength,
                    item.source_span,
                    item.special().semantic().footnote_label,
                    EditorFootnoteControlKind::Reference});
                continue;
            }
            if (item.kind == folia::InlineRenderItem::Kind::Marker && item.source_span.source_range.empty())
            {
                auto markerPosition = folia::TextPosition{
                    item.source_span.container_id,
                    item.source_span.source_range.start,
                    item.special().generated_boundary_affinity.value_or(folia::TextAffinity::Upstream),
                };
                if (item.special().marker_role == folia::MarkerRole::TaskCheckbox)
                {
                    auto const& markerText = item.special().display_text.empty()
                        ? item.text : item.special().display_text;
                    auto prefixLength = markerText.size() >= item.text.size()
                        && std::equal(item.text.rbegin(), item.text.rend(), markerText.rbegin())
                        ? markerText.size() - item.text.size()
                        : std::size_t{0};
                    if (prefixLength > 0)
                    {
                        AppendGeneratedText(
                            display,
                            markerText.substr(0, prefixLength),
                            markerPosition,
                            item.style,
                            EditorDisplayPositionKind::BoundaryDecoration);
                    }
                    auto displayStart = static_cast<std::uint32_t>(display.displayToSource.size());
                    AppendGeneratedText(
                        display,
                        U"\uFFFC",
                        markerPosition,
                        item.style,
                        EditorDisplayPositionKind::BoundaryDecoration);
                    auto fontSize = inlineStyle.baseFontSize;
                    auto boxSize = (std::clamp)(fontSize * 0.86f, 14.0f, 18.0f);
                    auto height = (std::max)(boxSize, fontSize * 1.2f);
                    display.taskCheckboxOverlays.push_back({
                        displayStart,
                        markerPosition,
                        item.special().task_checked,
                        boxSize + (std::max)(7.0f, fontSize * 0.42f),
                        height,
                        fontSize * 0.95f,
                        boxSize,
                    });
                    continue;
                }
                auto const& markerText = item.special().display_text.empty()
                    ? item.text : item.special().display_text;
                auto displayStart = static_cast<std::uint32_t>(display.displayToSource.size());
                AppendGeneratedText(
                    display,
                    markerText,
                    markerPosition,
                    item.style,
                    EditorDisplayPositionKind::BoundaryDecoration);
                if (item.special().marker_role == folia::MarkerRole::Heading && !display.ranges.empty())
                    display.ranges.back().marker = true;
                if (item.special().marker_role == folia::MarkerRole::BlockSpacing)
                {
                    auto objectOffset = markerText.find(U'\uFFFC');
                    if (objectOffset != std::u32string::npos)
                        display.blockSpacingOverlays.push_back({
                            displayStart + static_cast<std::uint32_t>(objectOffset),
                            static_cast<std::uint32_t>(markerText.size() - objectOffset),
                            item.special().block_spacing_height});
                }
                if (item.special().marker_role == folia::MarkerRole::FootnoteLabel)
                {
                    auto displayLength = static_cast<std::uint32_t>(folia::utf16_len(markerText));
                    if (!display.ranges.empty()) {
                        display.ranges.back().footnoteControl = EditorFootnoteControlKind::DefinitionLabel;
                    }
                    display.footnoteOverlays.push_back({
                        displayStart,
                        displayLength,
                        item.source_span,
                        item.special().semantic().footnote_label,
                        EditorFootnoteControlKind::DefinitionLabel});
                }
            }
            else
            {
                auto rangeStart = display.ranges.size();
                auto const& text = item.kind == folia::InlineRenderItem::Kind::Marker
                    && !item.special().display_text.empty() ? item.special().display_text : item.text;
                // An explicit empty editable text item owns a source row even
                // without glyphs. Indentation and padding never supply it.
                if (item.kind == folia::InlineRenderItem::Kind::Text && text.empty())
                    AppendGeneratedText(display, U"\u200B",
                        {item.source_span.container_id, item.source_span.source_range.start, folia::TextAffinity::Downstream},
                        item.style, EditorDisplayPositionKind::Source);
                else
                    AppendDisplayText(display, text, item.source_span, item.style, item.kind == folia::InlineRenderItem::Kind::Marker);
                auto syntax = SourceSyntax(item.source_syntax);
                for (auto rangeIndex = rangeStart; rangeIndex < display.ranges.size(); ++rangeIndex)
                {
                    display.ranges[rangeIndex].syntax = syntax;
                    display.ranges[rangeIndex].icon = item.special().syntax_marker;
                }
            }
        }
        if (!display.text.empty() && display.text.back() == U'\n')
            AppendGeneratedText(display, U"\u200B", sourceEnd, folia::InlineStyle::plain(), sourceEnd.kind);
        // CST boundaries do not need duplicate DirectWrite style runs. Keep
        // controls distinct, and merge only adjacent text with equal styling.
        std::vector<InlineStyleRange> ranges;
        for (auto& range : display.ranges) {
            if (!ranges.empty()) {
                auto& previous = ranges.back();
                if (!previous.marker && !range.marker
                    && previous.icon == SyntaxIcon::None && range.icon == SyntaxIcon::None
                    && !previous.footnoteControl && !range.footnoteControl
                    && previous.completion == range.completion && previous.syntax == range.syntax
                    && previous.style == range.style && previous.start + previous.length == range.start) {
                    previous.length += range.length;
                    continue;
                }
            }
            ranges.push_back(std::move(range));
        }
        display.ranges = std::move(ranges);
        display.displayToSource.push_back(sourceEnd);
        return display;
    }

}
