#include "pch.h"
#include "editor/rendering/EditorContentPreparation.h"

import folia.core.render_model;
import folia.markdown.utf;

namespace winrt::Folia
{
    namespace
    {
        void AppendMathBlockSource(DisplayInlineText& display, folia::RenderBlock const& block)
        {
            auto const& special = block.special();
            auto span = block.content_span;
            if (!special.content_to_source.empty())
                span.source_range = {special.content_to_source.front(), special.content_to_source.back()};
            display.mathSources.push_back({special.tex, span, special.content_to_source});
            auto source = std::u32string_view{special.raw_source};
            auto append = [&](folia::SourceRange range, SyntaxIcon icon, bool marker) {
                auto before = display.ranges.size();
                AppendSourceText(display, source.substr(range.start, range.length()),
                    {block.id, range}, {}, marker);
                if (display.ranges.size() > before) {
                    display.ranges.back().icon = icon;
                    display.ranges.back().iconPlacement = SyntaxIconPlacement::Block;
                }
            };
            std::size_t cursor = 0;
            for (auto const& marker : special.source_markers)
            {
                append({cursor, marker.range.start}, SyntaxIcon::None, false);
                append(marker.range, marker.icon, true);
                cursor = marker.range.end;
            }
            append({cursor, source.size()}, SyntaxIcon::None, false);
        }
    }

    DisplayInlineText BuildCodeBlockText(folia::RenderBlock const& block, folia::TextPosition caret, TreeSitterHighlighter& highlighter)
    {
        (void)caret;
        DisplayInlineText display;
        auto const& special = block.special();
        const auto contentStart = special.content_to_source.empty()
            ? block.content_span.source_range.start
            : special.content_to_source.front();
        // The header row is generated presentation chrome. It reserves space
        // for the editable language selector without entering Markdown source,
        // selection, copying, or saving.
        AppendGeneratedText(
            display,
            U"\u200B\n",
            {block.id, contentStart, folia::TextAffinity::Downstream},
            folia::InlineStyle::plain(),
            EditorDisplayPositionKind::Generated);
        const auto codeDisplayStart = static_cast<std::uint32_t>(
            folia::utf16_len(display.text));
        // An indented code block has no user-visible delimiter. Its leading
        // indentation is structural Markdown syntax and must stay hidden even
        // while the block owns the caret. content_to_source keeps every
        // displayed boundary in the one authoritative source coordinate.
        auto code = special.code_text;
        // The block-source projection already excludes the separator line
        // ending immediately before a closing fence. Any remaining terminal
        // line ending belongs to the user's code and denotes a real empty row.
        // Fences, info strings and indentation stay hidden in rendered mode.
        AppendProjectedSourceText(
            display,
            code,
            block.id,
            special.content_to_source,
            folia::InlineStyle::plain());
        if (special.language && !code.empty())
        {
            auto highlights = highlighter.Highlight(*special.language, folia::cps_to_utf8(code));
            for (auto const& highlight : highlights)
            {
                auto start = (std::min)(static_cast<std::size_t>(highlight.start), code.size());
                auto end = (std::min)(start + static_cast<std::size_t>(highlight.length), code.size());
                auto displayStart = codeDisplayStart + static_cast<std::uint32_t>(
                    folia::char_index_to_utf16(code, start));
                auto displayEnd = codeDisplayStart + static_cast<std::uint32_t>(
                    folia::char_index_to_utf16(code, end));
                if (displayStart < displayEnd)
                {
                    display.ranges.push_back(InlineStyleRange{
                        displayStart,
                        displayEnd - displayStart,
                        folia::InlineStyle::plain(),
                        false,
                        highlight.kind,
                    });
                }
            }
        }
        const auto endpoint = special.content_to_source.empty()
            ? code.size()
            : special.content_to_source[(std::min)(code.size(), special.content_to_source.size() - 1)];
        if (display.text.empty())
        {
            AppendGeneratedText(
                display,
                U"\u200B",
                {block.id, endpoint, folia::TextAffinity::Downstream},
                folia::InlineStyle::plain(),
                EditorDisplayPositionKind::Source);
        }
        display.displayToSource.push_back({block.id, endpoint, folia::TextAffinity::Downstream});
        return display;
    }

    DisplayInlineText BuildMathBlockText(
        folia::RenderBlock const& block,
        folia::TextPosition caret,
        MathJaxRenderer& mathJax,
        SvgNormalizer& svgNormalizer,
        D2D1_COLOR_F svgColor,
        float fontSize,
        float containerWidth,
        bool svgSupported,
        bool /*requestMath*/,
        bool highPriority)
    {
        DisplayInlineText display;
        auto const& special = block.special();
        auto span = block.content_span;
        auto editing = caret.container_id == block.id;
        if (!svgSupported)
        {
            const auto endpoint = editing ? special.raw_source.size() : special.content_to_source.empty()
                ? special.tex.size()
                : special.content_to_source.back();
            if (editing) AppendMathBlockSource(display, block);
            else AppendProjectedSourceText(
                display,
                special.tex,
                block.id,
                special.content_to_source,
                folia::InlineStyle::plain());
            if (display.text.empty())
            {
                AppendGeneratedText(
                    display,
                    U"\u200B",
                    {block.id, endpoint, folia::TextAffinity::Downstream},
                    folia::InlineStyle::plain(),
                    EditorDisplayPositionKind::Source);
            }
            display.displayToSource.push_back({block.id, endpoint, folia::TextAffinity::Downstream});
            return display;
        }

        auto purpose = editing ? MathRenderPurpose::Preview : MathRenderPurpose::Document;
        auto tex = folia::cps_to_utf8(special.tex);
        auto color = SvgColor(svgColor);
        auto math = highPriority
            ? mathJax.Render(tex, true, fontSize, containerWidth, color, {}, purpose)
            : mathJax.Lookup(tex, true, fontSize, containerWidth, color, purpose);
        display.pendingMath = !math;
        if (editing)
        {
            AppendMathBlockSource(display, block);
            // The preview is rendered below this editable source as its own
            // non-interactive visual block. It must never enter the source
            // display mapping or become a caret target.
            if (math && static_cast<bool>(*math))
                display.mathPreviews.push_back(DisplayInlineText::MathPreview{
                    *math,
                    span,
                    false,
                    svgColor,
                });
        }
        else if (!math || !static_cast<bool>(*math))
        {
            AppendProjectedSourceText(
                display,
                special.tex,
                block.id,
                special.content_to_source,
                folia::InlineStyle::plain());
        }
        else
        {
            AppendMathFragments(display, *math, span, false, folia::InlineStyle::plain(), svgColor);
        }
        const auto endpoint = editing ? special.raw_source.size() : special.content_to_source.empty()
            ? special.tex.size()
            : special.content_to_source.back();
        if (display.text.empty())
        {
            AppendGeneratedText(
                display,
                U"\u200B",
                {block.id, endpoint, folia::TextAffinity::Downstream},
                folia::InlineStyle::plain(),
                EditorDisplayPositionKind::Source);
        }
        display.displayToSource.push_back({block.id, endpoint, folia::TextAffinity::Downstream});
        return display;
    }

}
