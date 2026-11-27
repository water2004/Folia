#include "pch.h"

import folia.core.render_model;
import folia.core.theme;
import folia.core.types;
import folia.markdown.utf;

#include "editor/rendering/layout/EditorContentPreparation.h"
#include "editor/rendering/layout/EditorTextLayoutEngine.h"
#include <dwrite_1.h>
#include <dwrite_3.h>

namespace winrt::Folia
{
    using folia::platform::editor::CodeSpanRanges;

    EditorTextLayoutEngine::EditorTextLayoutEngine(EditorRenderResources& resources, EditorStyleSheet const& styleSheet)
        : resources(resources), styleSheet(styleSheet)
    {
    }

    ::Microsoft::WRL::ComPtr<IDWriteTextLayout> EditorTextLayoutEngine::Create(std::wstring const& text, IDWriteTextFormat* format, float width) const
    {
        ::Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        auto result = resources.dwriteFactory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format, width, 100000.0f, layout.GetAddressOf());
        if (FAILED(result)) return {};
        return layout;
    }

    namespace
    {
        void ApplyCodeSpanSpacing(IDWriteTextLayout* layout, std::vector<InlineStyleRange> const& ranges)
        {
            auto spans = CodeSpanRanges(ranges);
            if (spans.empty()) return;
            ::Microsoft::WRL::ComPtr<IDWriteTextLayout1> spaced;
            winrt::check_hresult(layout->QueryInterface(IID_PPV_ARGS(spaced.GetAddressOf())));
            UINT32 count = 0;
            layout->GetClusterMetrics(nullptr, 0, &count);
            std::vector<DWRITE_CLUSTER_METRICS> clusters(count);
            winrt::check_hresult(layout->GetClusterMetrics(clusters.data(), count, &count));
            std::vector<UINT32> starts{0};
            for (auto const& cluster : clusters) starts.push_back(starts.back() + cluster.length);
            for (auto const& span : spans) {
                auto first = std::upper_bound(starts.begin(), starts.end(), span.start) - starts.begin() - 1;
                auto last = std::lower_bound(starts.begin(), starts.end(), span.start + span.length) - starts.begin() - 1;
                // Pad the shaped boundary clusters, preserving surrogate pairs,
                // combining marks and every source/display caret offset.
                winrt::check_hresult(spaced->SetCharacterSpacing(3, first == last ? 3.f : 0.f, 0,
                    {starts[first], clusters[first].length}));
                if (first != last) winrt::check_hresult(spaced->SetCharacterSpacing(0, 3, 0,
                    {starts[last], clusters[last].length}));
            }
        }

        struct LogicalIndent
        {
            UINT32 lineStart = 0;
            UINT32 contentStart = 0;
            UINT32 lineEnd = 0;
            float width = 0.0f;
        };

        std::vector<LogicalIndent> FindLogicalIndents(DisplayInlineText const& display, std::wstring const& text)
        {
            std::vector<LogicalIndent> result;
            UINT32 lineStart = 0;
            while (lineStart <= text.size())
            {
                auto newline = text.find(L'\n', lineStart);
                auto lineEnd = newline == std::wstring::npos ? static_cast<UINT32>(text.size()) : static_cast<UINT32>(newline);
                auto contentStart = lineStart;
                while (contentStart < lineEnd && contentStart < display.displayToSource.size()
                    && display.displayToSource[contentStart].kind == EditorDisplayPositionKind::BoundaryDecoration
                    && display.displayToSource[contentStart].affinity == folia::TextAffinity::Downstream)
                {
                    ++contentStart;
                }
                if (contentStart > lineStart) result.push_back({lineStart, contentStart, lineEnd, 0.0f});
                if (newline == std::wstring::npos) break;
                lineStart = static_cast<UINT32>(newline + 1);
            }
            return result;
        }

        void ShiftRangesForInsertion(DisplayInlineText& display, UINT32 position)
        {
            folia::platform::editor::ShiftStyleRangesForInsertion(display, position);
            for (auto& overlay : display.mathOverlays) if (overlay.displayStart >= position) ++overlay.displayStart;
            for (auto& overlay : display.imageOverlays) if (overlay.displayStart >= position) ++overlay.displayStart;
            for (auto& overlay : display.mermaidOverlays) if (overlay.displayStart >= position) ++overlay.displayStart;
            for (auto& overlay : display.indentOverlays) if (overlay.displayStart >= position) ++overlay.displayStart;
            for (auto& overlay : display.blockSpacingOverlays) if (overlay.displayStart >= position) ++overlay.displayStart;
            for (auto& overlay : display.taskCheckboxOverlays) if (overlay.displayStart >= position) ++overlay.displayStart;
            for (auto& overlay : display.footnoteOverlays) if (overlay.displayStart >= position) ++overlay.displayStart;
        }

        void InsertIndent(DisplayInlineText& display, UINT32 position, float width)
        {
            const auto textLength = static_cast<UINT32>(folia::utf16_len(display.text));
            position = (std::min)(position, textLength);
            EditorDisplayPosition mapping = position < display.displayToSource.size()
                ? display.displayToSource[position]
                : display.displayToSource.empty()
                    ? EditorDisplayPosition{}
                    : display.displayToSource.back();
            mapping.affinity = folia::TextAffinity::Downstream;
            mapping.kind = EditorDisplayPositionKind::BoundaryDecoration;
            display.text.insert(display.text.begin() + static_cast<std::ptrdiff_t>(folia::utf16_to_char_index(display.text, position)), U'\uFFFC');
            auto mappingIndex = (std::min)(static_cast<std::size_t>(position), display.displayToSource.size());
            display.displayToSource.insert(display.displayToSource.begin() + static_cast<std::ptrdiff_t>(mappingIndex), mapping);
            ShiftRangesForInsertion(display, position);
            display.indentOverlays.push_back({position, width});
        }

        DisplayInlineText SliceDisplay(DisplayInlineText const& source, UINT32 start, UINT32 end)
        {
            const auto sourceLength = static_cast<UINT32>(folia::utf16_len(source.text));
            start = (std::min)(start, sourceLength);
            end = (std::clamp)(end, start, sourceLength);
            DisplayInlineText result{folia::platform::editor::SliceInlineProjection(source, start, end)};
            result.mathBrackets = source.mathBrackets;
            result.pendingMermaid = source.pendingMermaid;
            for (auto overlay : source.mathOverlays)
            {
                if (overlay.displayStart < start || overlay.displayStart >= end) continue;
                overlay.displayStart -= start;
                result.mathOverlays.push_back(std::move(overlay));
            }
            for (auto overlay : source.blockSpacingOverlays)
            {
                if (overlay.displayStart < start || overlay.displayStart >= end) continue;
                overlay.displayStart -= start;
                result.blockSpacingOverlays.push_back(std::move(overlay));
            }
            for (auto overlay : source.imageOverlays)
            {
                if (overlay.displayStart < start || overlay.displayStart >= end) continue;
                overlay.displayStart -= start;
                result.imageOverlays.push_back(std::move(overlay));
            }
            for (auto overlay : source.mermaidOverlays)
            {
                if (overlay.displayStart < start || overlay.displayStart >= end) continue;
                overlay.displayStart -= start;
                result.mermaidOverlays.push_back(std::move(overlay));
            }
            for (auto overlay : source.taskCheckboxOverlays)
            {
                if (overlay.displayStart < start || overlay.displayStart >= end) continue;
                overlay.displayStart -= start;
                result.taskCheckboxOverlays.push_back(std::move(overlay));
            }
            for (auto overlay : source.footnoteOverlays)
            {
                auto overlayEnd = overlay.displayStart + overlay.displayLength;
                if (overlayEnd <= start || overlay.displayStart >= end) continue;
                overlay.displayStart = (std::max)(overlay.displayStart, start) - start;
                overlay.displayLength = (std::min)(overlayEnd, end) - (std::max)(overlayEnd - overlay.displayLength, start);
                result.footnoteOverlays.push_back(std::move(overlay));
            }
            return result;
        }

        std::optional<std::size_t> LogicalIndentAt(std::vector<LogicalIndent> const& indents, UINT32 position)
        {
            for (std::size_t index = 0; index < indents.size(); ++index)
            {
                if (position >= indents[index].contentStart && position < indents[index].lineEnd) return index;
            }
            return std::nullopt;
        }

        std::optional<std::uint8_t> HomogeneousHeadingLevel(std::vector<InlineStyleRange> const& ranges)
        {
            std::optional<std::uint8_t> level;
            for (auto const& range : ranges)
            {
                if (range.length == 0) continue;
                if (!range.style.heading_level) return std::nullopt;
                if (!level) level = *range.style.heading_level;
                else if (*level != *range.style.heading_level) return std::nullopt;
            }
            return level;
        }

        D2D1_COLOR_F ToD2D(folia::Color color)
        {
            constexpr auto scale = 1.0f / 255.0f;
            return D2D1::ColorF(
                color.r * scale,
                color.g * scale,
                color.b * scale,
                color.a * scale);
        }

        float BaseFontSize(folia::InlineStyle const& style, EditorStyleSheet const& styleSheet)
        {
            if (style.heading_level)
            {
                switch (*style.heading_level)
                {
                    case 1: return styleSheet.heading1.size;
                    case 2: return styleSheet.heading2.size;
                    case 3: return styleSheet.heading3.size;
                    case 4: return styleSheet.body.size * 1.15f;
                    default: return styleSheet.body.size;
                }
            }
            return style.code != folia::CodeStyle::None ? styleSheet.code.size : styleSheet.body.size;
        }

        float BaseLineHeight(folia::InlineStyle const& style, EditorStyleSheet const& styleSheet)
        {
            if (style.heading_level)
            {
                switch (*style.heading_level)
                {
                    case 1: return styleSheet.heading1.lineHeight;
                    case 2: return styleSheet.heading2.lineHeight;
                    case 3: return styleSheet.heading3.lineHeight;
                    default: return styleSheet.body.lineHeight;
                }
            }
            return style.code != folia::CodeStyle::None ? styleSheet.code.lineHeight : styleSheet.body.lineHeight;
        }
    }

    void DrawInlinePresentationBackgrounds(
        EditorRenderResources& resources,
        EditorStyleSheet const& styleSheet,
        IDWriteTextLayout* layout,
        D2D1_POINT_2F origin,
        std::vector<InlineStyleRange> const& ranges)
    {
        if (!layout || !resources.d2dContext) return;
        for (auto const& span : CodeSpanRanges(ranges)) {
            for (auto rect : InlineRangeRectangles(layout, span.start, span.length, origin)) {
                rect.top += 2; rect.bottom -= 1;
                resources.d2dContext->FillRoundedRectangle(D2D1::RoundedRect(rect, 4, 4),
                    resources.inlineCodeBackgroundBrush.Get());
            }
        }

        struct PresentationPaint
        {
            std::optional<folia::Color> background;
            bool highlight = false;
            bool inlineCode = false;
            bool operator==(PresentationPaint const&) const = default;
        };
        UINT32 start = 0, end = 0;
        PresentationPaint paint;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> backgroundBrush, underlineBrush;
        auto fill = [&](ID2D1SolidColorBrush* brush, D2D1_COLOR_F color, auto const& rectangles, bool underline) {
            brush->SetColor(color);
            for (auto rect : rectangles) {
                if (underline) {
                    rect.top = (std::max)(rect.top, rect.bottom - styleSheet.highlightUnderlineWidth);
                    resources.d2dContext->FillRectangle(rect, brush);
                } else if (paint.inlineCode) {
                    rect.top += 2; rect.bottom -= 1;
                    resources.d2dContext->FillRoundedRectangle(D2D1::RoundedRect(rect, 4, 4), brush);
                } else resources.d2dContext->FillRectangle(rect, brush);
            }
        };
        auto draw = [&] {
            if (start == end) return;
            const auto background = paint.background ? ToD2D(*paint.background) : styleSheet.highlightBackgroundColor;
            const auto underline = styleSheet.highlightUnderlineColor;
            const bool drawUnderline = paint.highlight && underline.a > 0 && styleSheet.highlightUnderlineWidth > 0;
            if (background.a <= 0 && !drawUnderline) return;
            const auto rectangles = InlineRangeRectangles(layout, start, end - start, origin);
            if (background.a > 0) {
                if (!backgroundBrush && FAILED(resources.d2dContext->CreateSolidColorBrush(background, backgroundBrush.GetAddressOf()))) return;
                fill(backgroundBrush.Get(), background, rectangles, false);
            }
            if (drawUnderline) {
                if (!underlineBrush && FAILED(resources.d2dContext->CreateSolidColorBrush(underline, underlineBrush.GetAddressOf()))) return;
                fill(underlineBrush.Get(), underline, rectangles, true);
            }
        };

        for (auto const& range : ranges) {
            auto const& presentation = range.style.presentation;
            if (range.length == 0 || !presentation) continue;
            const PresentationPaint next{
                presentation->background,
                !range.marker && presentation->highlight,
                !range.marker && range.style.code == folia::CodeStyle::Inline,
            };
            if (!next.background && !next.highlight) continue;
            // Foreground style changes within a highlight share the same painted run.
            if (range.start != end || next != paint) {
                draw();
                start = range.start;
                paint = next;
            }
            end = range.start + range.length;
        }
        draw();
    }

    ::Microsoft::WRL::ComPtr<IDWriteTextLayout> EditorTextLayoutEngine::CreateFlow(
        DisplayInlineText& display,
        IDWriteTextFormat* format,
        float width,
        std::function<void(IDWriteTextLayout*, DisplayInlineText const&)> const& configure) const
    {
        auto build = [&](DisplayInlineText const& value, float availableWidth)
        {
            auto layout = Create(ToWide(value.text), format, availableWidth);
            ApplyStyles(layout.Get(), value.ranges);
            ApplyMathInlineObjects(layout.Get(), value.mathOverlays);
            ApplyBlockSpacingInlineObjects(layout.Get(), value);
            ApplyIndentInlineObjects(layout.Get(), value.indentOverlays);
            ApplyTaskCheckboxInlineObjects(layout.Get(), value.taskCheckboxOverlays);
            if (configure) configure(layout.Get(), value);
            return layout;
        };

        if (!display.displayToSource.contains_kind(EditorDisplayPositionKind::BoundaryDecoration))
            return build(display, width);
        const auto baseText = ToWide(display.text);
        auto indents = FindLogicalIndents(display, baseText);
        if (indents.empty()) return build(display, width);

        const auto base = display;
        auto initial = build(base, width);
        if (!initial) return initial;
        for (auto& indent : indents)
        {
            FLOAT x = 0.0f;
            FLOAT y = 0.0f;
            DWRITE_HIT_TEST_METRICS metrics{};
            if (SUCCEEDED(initial->HitTestTextPosition(indent.contentStart, FALSE, &x, &y, &metrics))) indent.width = x;
        }
        std::erase_if(indents, [](auto const& value) { return value.width <= 0.0f; });
        if (indents.empty()) return initial;

        UINT32 initialLineCount = 0;
        if (initial->GetLineMetrics(nullptr, 0, &initialLineCount) != E_NOT_SUFFICIENT_BUFFER || initialLineCount == 0) return initial;
        std::vector<DWRITE_LINE_METRICS> initialMetrics(initialLineCount);
        if (FAILED(initial->GetLineMetrics(initialMetrics.data(), initialLineCount, &initialLineCount))) return initial;
        std::unordered_set<std::size_t> wrappedIndents;
        UINT32 initialLineStart = 0;
        for (UINT32 line = 0; line < initialLineCount; ++line)
        {
            if (initialLineStart > 0 && initialLineStart < baseText.size() && baseText[initialLineStart - 1] != L'\n')
            {
                if (auto indentIndex = LogicalIndentAt(indents, initialLineStart)) wrappedIndents.insert(*indentIndex);
            }
            initialLineStart += initialMetrics[line].length;
        }

        std::vector<std::pair<UINT32, float>> breaks;
        for (auto indentIndex : wrappedIndents)
        {
            auto const& indent = indents[indentIndex];
            auto content = SliceDisplay(base, indent.contentStart, indent.lineEnd);
            auto contentLayout = build(content, (std::max)(1.0f, width - indent.width));
            if (!contentLayout) continue;
            UINT32 lineCount = 0;
            if (contentLayout->GetLineMetrics(nullptr, 0, &lineCount) != E_NOT_SUFFICIENT_BUFFER || lineCount <= 1) continue;
            std::vector<DWRITE_LINE_METRICS> metrics(lineCount);
            if (FAILED(contentLayout->GetLineMetrics(metrics.data(), lineCount, &lineCount))) continue;
            UINT32 lineStart = 0;
            for (UINT32 line = 0; line < lineCount; ++line)
            {
                if (lineStart > 0 && indent.contentStart + lineStart < indent.lineEnd) {
                    breaks.push_back({indent.contentStart + lineStart, indent.width});
                }
                lineStart += metrics[line].length;
            }
        }
        if (breaks.empty()) return build(display, width);
        std::ranges::sort(breaks, {}, &std::pair<UINT32, float>::first);
        breaks.erase(std::unique(breaks.begin(), breaks.end(), [](auto const& left, auto const& right) {
            return left.first == right.first;
        }), breaks.end());
        UINT32 inserted = 0;
        for (auto const& [basePosition, indentWidth] : breaks)
        {
            InsertIndent(display, basePosition + inserted, indentWidth);
            ++inserted;
        }
        std::ranges::sort(display.indentOverlays, {}, &DisplayInlineText::IndentOverlay::displayStart);
        return build(display, width);
    }

    IDWriteTextFormat* EditorTextLayoutEngine::FormatFor(bool code, std::vector<InlineStyleRange> const& ranges) const
    {
        return FormatFor(FormatKindFor(code, ranges));
    }

    EditorFlowTextFormat EditorTextLayoutEngine::FormatKindFor(bool code, std::vector<InlineStyleRange> const& ranges) const
    {
        if (code) return EditorFlowTextFormat::Code;
        if (auto const level = HomogeneousHeadingLevel(ranges))
        {
            switch (*level)
            {
                case 1: return EditorFlowTextFormat::Heading1;
                case 2: return EditorFlowTextFormat::Heading2;
                case 3: return EditorFlowTextFormat::Heading3;
                default: break;
            }
        }
        return EditorFlowTextFormat::Body;
    }

    IDWriteTextFormat* EditorTextLayoutEngine::FormatFor(EditorFlowTextFormat format) const
    {
        switch (format)
        {
            case EditorFlowTextFormat::Code: return resources.codeFormat.Get();
            case EditorFlowTextFormat::Heading1: return resources.heading1Format.Get();
            case EditorFlowTextFormat::Heading2: return resources.heading2Format.Get();
            case EditorFlowTextFormat::Heading3: return resources.heading3Format.Get();
            default: return resources.textFormat.Get();
        }
    }

    float EditorTextLayoutEngine::LineHeightFor(bool code, std::vector<InlineStyleRange> const& ranges) const
    {
        return LineHeightFor(FormatKindFor(code, ranges));
    }

    float EditorTextLayoutEngine::LineHeightFor(EditorFlowTextFormat format) const
    {
        switch (format)
        {
            case EditorFlowTextFormat::Code: return styleSheet.code.lineHeight;
            case EditorFlowTextFormat::Heading1: return styleSheet.heading1.lineHeight;
            case EditorFlowTextFormat::Heading2: return styleSheet.heading2.lineHeight;
            case EditorFlowTextFormat::Heading3: return styleSheet.heading3.lineHeight;
            default: return styleSheet.body.lineHeight;
        }
    }

    void EditorTextLayoutEngine::ApplyStyles(IDWriteTextLayout* layout, std::vector<InlineStyleRange> const& ranges) const
    {
        if (!layout) return;
        bool requiresDefaultLineSpacing = false;
        for (auto const& range : ranges)
        {
            DWRITE_TEXT_RANGE textRange{ range.start, range.length };
            if (range.style.bold) layout->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, textRange);
            if (range.style.italic) layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, textRange);
            if (range.style.link || range.style.underline) layout->SetUnderline(true, textRange);
            if (range.style.strikethrough) layout->SetStrikethrough(true, textRange);
            if (range.style.code != folia::CodeStyle::None)
            {
                layout->SetFontFamilyName(styleSheet.code.family.c_str(), textRange);
                layout->SetFontSize(styleSheet.code.size, textRange);
                auto brush = range.style.code == folia::CodeStyle::Inline
                    ? resources.inlineCodeBrush.Get() : resources.codeBrush.Get();
                if (brush) layout->SetDrawingEffect(brush, textRange);
            }
            if (range.style.heading_level)
            {
                auto level = *range.style.heading_level;
                auto const* heading = level == 1 ? &styleSheet.heading1
                    : level == 2 ? &styleSheet.heading2
                    : level == 3 ? &styleSheet.heading3
                    : nullptr;
                auto size = heading ? heading->size
                    : level == 4 ? styleSheet.body.size * 1.15f
                    : styleSheet.body.size;
                if (heading && range.style.code == folia::CodeStyle::None)
                    layout->SetFontFamilyName(heading->family.c_str(), textRange);
                layout->SetFontSize(size, textRange);
                auto weight = heading ? heading->weight : DWRITE_FONT_WEIGHT_SEMI_BOLD;
                if (range.style.bold && weight < DWRITE_FONT_WEIGHT_SEMI_BOLD)
                    weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
                layout->SetFontWeight(weight, textRange);
                auto fontStyle = heading ? heading->style : styleSheet.body.style;
                if (range.style.italic) fontStyle = DWRITE_FONT_STYLE_ITALIC;
                layout->SetFontStyle(fontStyle, textRange);
            }
            if (range.completion)
            {
                layout->SetFontWeight(DWRITE_FONT_WEIGHT_NORMAL, textRange);
                layout->SetFontStyle(DWRITE_FONT_STYLE_NORMAL, textRange);
                layout->SetStrikethrough(range.style.strikethrough, textRange);
                layout->SetUnderline(FALSE, textRange);
                if (resources.mutedBrush) layout->SetDrawingEffect(resources.mutedBrush.Get(), textRange);
                continue;
            }
            if (range.marker)
            {
                auto size = BaseFontSize(range.style, styleSheet);
                layout->SetFontSize(size, textRange);
                layout->SetFontWeight(DWRITE_FONT_WEIGHT_NORMAL, textRange);
                layout->SetFontStyle(DWRITE_FONT_STYLE_NORMAL, textRange);
                layout->SetUnderline(false, textRange);
                layout->SetStrikethrough(false, textRange);
                if (range.icon != SyntaxIcon::None)
                {
                    auto block = range.iconPlacement == SyntaxIconPlacement::Block;
                    auto iconSize = (block ? BaseLineHeight(range.style, styleSheet) : size) * styleSheet.marker.sizeEm;
                    ApplySyntaxIconObject(layout, range.start, range.length,
                        iconSize, block ? 0.0f : size * styleSheet.marker.gapEm);
                    requiresDefaultLineSpacing = requiresDefaultLineSpacing || iconSize > BaseLineHeight(range.style, styleSheet);
                }
                if (resources.mutedBrush) layout->SetDrawingEffect(resources.mutedBrush.Get(), textRange);
            }
            if (range.footnoteControl)
            {
                auto const reference = *range.footnoteControl == EditorFootnoteControlKind::Reference;
                layout->SetFontSize(styleSheet.body.size * (reference ? 1.1f : 0.82f), textRange);
                layout->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, textRange);
                if (resources.accentBrush) layout->SetDrawingEffect(resources.accentBrush.Get(), textRange);
            }
            auto syntaxIndex = static_cast<std::size_t>(range.syntax);
            if (range.syntax != SyntaxHighlightKind::None && syntaxIndex < resources.syntaxBrushes.size() && resources.syntaxBrushes[syntaxIndex])
            {
                layout->SetDrawingEffect(resources.syntaxBrushes[syntaxIndex].Get(), textRange);
            }
            if (auto const& presentation = range.style.presentation; presentation && !range.marker)
            {
                if (presentation->font_family)
                {
                    auto family = winrt::to_hstring(*presentation->font_family);
                    layout->SetFontFamilyName(family.c_str(), textRange);
                }
                if (presentation->font_weight)
                {
                    layout->SetFontWeight(
                        static_cast<DWRITE_FONT_WEIGHT>(*presentation->font_weight),
                        textRange);
                }
                if (presentation->font_italic)
                {
                    layout->SetFontStyle(
                        *presentation->font_italic
                            ? DWRITE_FONT_STYLE_ITALIC
                            : DWRITE_FONT_STYLE_NORMAL,
                        textRange);
                }
                const auto fontSize = presentation->absolute_font_size
                    ? *presentation->absolute_font_size * styleSheet.fontScale
                    : BaseFontSize(range.style, styleSheet) * presentation->relative_font_scale;
                if (presentation->absolute_font_size
                    || std::abs(presentation->relative_font_scale - 1.0f) > 0.001f)
                {
                    layout->SetFontSize(std::clamp(fontSize, 6.0f, 144.0f), textRange);
                    requiresDefaultLineSpacing = requiresDefaultLineSpacing
                        || fontSize > BaseLineHeight(range.style, styleSheet);
                }
                if (presentation->foreground && resources.d2dContext)
                {
                    ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
                    if (SUCCEEDED(resources.d2dContext->CreateSolidColorBrush(
                            ToD2D(*presentation->foreground),
                            brush.GetAddressOf())))
                    {
                        layout->SetDrawingEffect(brush.Get(), textRange);
                    }
                }
                if (presentation->baseline != folia::InlineBaseline::Normal)
                {
                    ::Microsoft::WRL::ComPtr<IDWriteTypography> typography;
                    if (SUCCEEDED(resources.dwriteFactory->CreateTypography(typography.GetAddressOf())))
                    {
                        DWRITE_FONT_FEATURE feature{
                            presentation->baseline == folia::InlineBaseline::Superscript
                                ? DWRITE_FONT_FEATURE_TAG_SUPERSCRIPT
                                : DWRITE_FONT_FEATURE_TAG_SUBSCRIPT,
                            1};
                        if (SUCCEEDED(typography->AddFontFeature(feature)))
                            layout->SetTypography(typography.Get(), textRange);
                    }
                }
            }
        }
        if (requiresDefaultLineSpacing)
            layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_DEFAULT, 0.0f, 0.0f);
        ApplyCodeSpanSpacing(layout, ranges);
    }

    float EditorTextLayoutEngine::ApplyLineGap(IDWriteTextLayout* layout, float themeAdvance, bool continues) const
    {
        if (!layout || styleSheet.lineGapScale == 1.0f) return 0.0f;
        DWRITE_TEXT_METRICS text{};
        if (FAILED(layout->GetMetrics(&text)) || text.lineCount == 0 || (text.lineCount == 1 && !continues))
            return 0.0f;
        DWRITE_LINE_SPACING_METHOD method{};
        float advance = 0, baseline = 0;
        if (FAILED(layout->GetLineSpacing(&method, &advance, &baseline))) return 0.0f;
        if (method == DWRITE_LINE_SPACING_METHOD_UNIFORM) {
            FLOAT x = 0.0f, y = 0.0f;
            DWRITE_HIT_TEST_METRICS first{};
            if (FAILED(layout->HitTestTextPosition(0, FALSE, &x, &y, &first))) return 0.0f;
            auto clearance = (std::max)(0.0f, advance - first.height);
            auto extra = clearance * (styleSheet.lineGapScale - 1.0f);
            if (extra != 0.0f)
                winrt::check_hresult(layout->SetLineSpacing(method, advance + extra, baseline));
            return extra;
        }

        // A natural line can contain a tall formula or image. Put the additional
        // leading below its content and preserve the natural baseline.
        auto clearance = (std::max)(0.0f, themeAdvance - layout->GetFontSize());
        auto extra = clearance * (std::max)(0.0f, styleSheet.lineGapScale - 1.0f);
        ::Microsoft::WRL::ComPtr<IDWriteTextLayout3> extended;
        if (extra == 0.0f || FAILED(layout->QueryInterface(IID_PPV_ARGS(extended.GetAddressOf())))) return 0.0f;
        std::vector<DWRITE_LINE_METRICS> lines(text.lineCount);
        UINT32 count = 0;
        if (FAILED(layout->GetLineMetrics(lines.data(), text.lineCount, &count)) || count == 0) return 0.0f;
        auto fraction = extra / (std::max)(1.0f, themeAdvance);
        DWRITE_LINE_SPACING spacing{DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.0f + fraction,
            1.0f, 0.0f, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT};
        winrt::check_hresult(extended->SetLineSpacing(&spacing));
        return lines[count - 1].height * fraction;
    }

    float EditorTextLayoutEngine::MeasureHeight(IDWriteTextLayout* layout, float fallbackHeight,
        bool enforceMinimum, float trailingGap) const
    {
        if (!layout) return fallbackHeight;
        DWRITE_TEXT_METRICS metrics{};
        if (FAILED(layout->GetMetrics(&metrics))) return fallbackHeight;
        auto height = metrics.height - trailingGap;
        return enforceMinimum ? (std::max)(fallbackHeight, height) : height;
    }
}
