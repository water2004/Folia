#include "pch.h"
import std;
#include "editor/rendering/EditorMathSyntax.h"
#include "editor/rendering/EditorTextLayoutEngine.h"

namespace winrt::Folia {
    namespace {
        class SourceDisplayIndex {
            struct Entry { std::size_t source; UINT32 display; };
            std::vector<Entry> entries;
        public:
            SourceDisplayIndex(EditorDisplayMapping const& mapping, folia::TextSpan span) {
                for (std::size_t i = 0; i + 1 < mapping.size(); ++i)
                    if (mapping[i].kind == EditorDisplayPositionKind::Source && mapping[i].container_id == span.container_id
                        && span.source_range.contains(mapping[i].source_offset))
                        entries.push_back({mapping[i].source_offset, static_cast<UINT32>(i)});
                std::ranges::stable_sort(entries, {}, &Entry::source);
            }
            std::vector<DWRITE_TEXT_RANGE> Ranges(folia::SourceRange source) const {
                std::vector<DWRITE_TEXT_RANGE> ranges;
                auto first = std::ranges::lower_bound(entries, source.start, {}, &Entry::source);
                auto end = std::ranges::lower_bound(entries, source.end, {}, &Entry::source);
                for (; first != end; ++first) {
                    if (!ranges.empty() && ranges.back().startPosition + ranges.back().length == first->display)
                        ++ranges.back().length;
                    else ranges.push_back({first->display, 1});
                }
                return ranges;
            }
        };
        SyntaxHighlightKind HighlightKind(folia::math::editor::TokenKind kind) {
            using K = folia::math::editor::TokenKind;
            switch (kind) {
            case K::Command: return SyntaxHighlightKind::Function;
            case K::Number: return SyntaxHighlightKind::Number;
            case K::Operator: case K::Delimiter: return SyntaxHighlightKind::Operator;
            case K::Comment: return SyntaxHighlightKind::Comment;
            case K::Text: return SyntaxHighlightKind::String;
            case K::Environment: return SyntaxHighlightKind::Type;
            default: return SyntaxHighlightKind::None;
            }
        }
    }

    void HighlightMathSources(DisplayInlineText& display, folia::TextPosition caret, TreeSitterHighlighter& highlighter) {
        for (auto const& math : display.mathSources) {
            auto const& analysis = highlighter.AnalyzeMath(math.tex);
            SourceDisplayIndex index(display.displayToSource, math.span);
            auto offset = [&](std::size_t local) {
                return math.toSource.empty() ? math.span.source_range.start + local
                    : math.toSource[(std::min)(local, math.toSource.size() - 1)];
            };
            auto sourceSpan = [&](folia::math::editor::SourceRange local) {
                return folia::TextSpan{math.span.container_id, {offset(local.start), offset(local.end)}};
            };
            for (auto const& token : analysis.tokens) {
                auto kind = HighlightKind(token.kind);
                if (kind == SyntaxHighlightKind::None) continue;
                for (auto range : index.Ranges(sourceSpan(token.range).source_range)) {
                    auto existing = std::ranges::find_if(display.ranges, [&](auto const& style) {
                        return style.start <= range.startPosition && range.startPosition < style.start + style.length;
                    });
                    auto style = existing == display.ranges.end() ? folia::InlineStyle::plain() : existing->style;
                    display.ranges.push_back({range.startPosition, range.length, std::move(style), false, kind});
                }
            }
            if (caret.container_id != math.span.container_id
                || caret.source_offset < math.span.source_range.start || caret.source_offset > math.span.source_range.end) continue;
            auto local = math.toSource.empty() ? caret.source_offset - math.span.source_range.start
                : static_cast<std::size_t>(std::lower_bound(math.toSource.begin(), math.toSource.end(), caret.source_offset) - math.toSource.begin());
            if (auto match = analysis.MatchAt(local)) {
                display.mathBrackets.push_back({sourceSpan(match->at), match->mate.has_value()});
                if (match->mate) display.mathBrackets.push_back({sourceSpan(*match->mate), true});
            }
        }
    }

    void PaintMathBrackets(EditorRenderResources& resources, EditorStyleSheet const& sheet,
        IDWriteTextLayout* layout, D2D1_POINT_2F origin, DisplayInlineText const& display,
        EditorDisplayMapping const& mapping) {
        if (!layout || !resources.mathBracketBrush) return;
        for (auto const& bracket : display.mathBrackets)
            for (auto range : SourceDisplayIndex(mapping, bracket.span).Ranges(bracket.span.source_range))
                for (auto rect : InlineRangeRectangles(layout, range.startPosition, range.length, origin)) {
                    auto setColor = [&](folia::Color color) {
                        resources.mathBracketBrush->SetColor(D2D1::ColorF(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
                    };
                    setColor(bracket.matched ? sheet.editor.bracket_match_background : sheet.editor.bracket_unmatched_background);
                    resources.d2dContext->FillRectangle(rect, resources.mathBracketBrush.Get());
                    setColor(bracket.matched ? sheet.editor.bracket_match_border : sheet.editor.bracket_unmatched_border);
                    auto width = sheet.editor.bracket_border_width;
                    auto inset = width / 2;
                    if (rect.right - rect.left > width && rect.bottom - rect.top > width)
                        resources.d2dContext->DrawRectangle(D2D1::RectF(rect.left + inset, rect.top + inset,
                            rect.right - inset, rect.bottom - inset), resources.mathBracketBrush.Get(), width);
                }
    }
}
