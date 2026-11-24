// Project block-local source into UTF-16 display runs for native text layout.
export module folia.platform.editor_inline_projection;
import std;
import folia.core.render_model;
import folia.core.syntax_marker;
import folia.highlight;
import folia.markdown.source_offset_map;
import folia.markdown.utf;
import folia.platform.editor_display_mapping;

export namespace folia::platform::editor
{
    using folia::highlight::SyntaxHighlightKind;
    enum class SyntaxIconPlacement { Inline, Block };

    struct InlineStyleRange
    {
        std::uint32_t start = 0;
        std::uint32_t length = 0;
        folia::InlineStyle style;
        bool marker = false;
        SyntaxHighlightKind syntax = SyntaxHighlightKind::None;
        std::optional<EditorFootnoteControlKind> footnoteControl;
        folia::SyntaxMarker icon = folia::SyntaxMarker::None;
        SyntaxIconPlacement iconPlacement = SyntaxIconPlacement::Inline;
        bool completion = false;
    };

    struct InlineTextProjection
    {
        std::u32string text;
        EditorDisplayMapping displayToSource;
        std::vector<InlineStyleRange> ranges;
    };

    EditorDisplayPosition InlineItemsEndPosition(std::vector<folia::InlineRenderItem> const& items, folia::TextPosition fallback)
    {
        EditorDisplayPosition result{fallback};
        for (auto const& item : items)
        {
            if (item.source_span.container_id.v != 0)
            {
                auto generatedBoundary = item.kind == folia::InlineRenderItem::Kind::Marker
                    && item.source_span.source_range.empty();
                auto affinity = generatedBoundary
                    ? item.special().generated_boundary_affinity.value_or(folia::TextAffinity::Downstream)
                    : folia::TextAffinity::Upstream;
                result = {
                    item.source_span.container_id,
                    item.source_span.source_range.end,
                    affinity,
                    // A suffix remains decoration. A prefix ends at the
                    // source boundary of its possibly empty editable body.
                    generatedBoundary && affinity == folia::TextAffinity::Upstream
                        ? EditorDisplayPositionKind::BoundaryDecoration : EditorDisplayPositionKind::Source,
                };
            }
        }
        return result;
    }

    void AppendDisplayText(InlineTextProjection& display, std::u32string_view text, folia::TextSpan sourceSpan, folia::InlineStyle style, bool marker)
    {
        // During construction there is one mapping per UTF-16 unit. The final
        // caret endpoint is appended only when the complete display is returned.
        auto start = static_cast<std::uint32_t>(display.displayToSource.size());
        display.text += text;
        auto length = static_cast<std::uint32_t>(folia::utf16_len(text));
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            auto position = folia::TextPosition{
                sourceSpan.container_id,
                sourceSpan.source_range.start + (std::min)(index, sourceSpan.source_range.length()),
                folia::TextAffinity::Downstream};
            display.displayToSource.push_back(EditorDisplayPosition{
                position,
                EditorDisplayPositionKind::Source});
            if (text[index] > 0xffff) display.displayToSource.push_back(EditorDisplayPosition{
                position,
                EditorDisplayPositionKind::Source});
        }
        if (length > 0)
        {
            InlineStyleRange range;
            range.start = start;
            range.length = length;
            range.style = style;
            range.marker = marker;
            display.ranges.push_back(range);
        }
    }

    void AppendGeneratedText(
        InlineTextProjection& display,
        std::u32string const& text,
        folia::TextPosition sourcePosition,
        folia::InlineStyle style,
        EditorDisplayPositionKind kind = EditorDisplayPositionKind::Generated)
    {
        auto start = static_cast<std::uint32_t>(display.displayToSource.size());
        display.text += text;
        auto length = folia::utf16_len(text);
        display.displayToSource.insert(
            display.displayToSource.end(),
            length,
            EditorDisplayPosition{sourcePosition, kind});
        if (!text.empty()) display.ranges.push_back(InlineStyleRange{start, static_cast<std::uint32_t>(length), style, false, SyntaxHighlightKind::None});
    }

    void AppendSourceText(InlineTextProjection& display, std::u32string_view sourceText, folia::TextSpan sourceSpan, folia::InlineStyle style, bool marker)
    {
        auto length = (std::min)(sourceSpan.source_range.length(), sourceText.size());
        AppendDisplayText(display, sourceText.substr(0, length), sourceSpan, style, marker);
    }

    void AppendProjectedSourceText(
        InlineTextProjection& display,
        std::u32string_view text,
        folia::NodeId owner,
        folia::SourceOffsetMap const& sourceOffsets,
        folia::InlineStyle style)
    {
        auto start = static_cast<std::uint32_t>(display.displayToSource.size());
        display.text.append(text);
        auto source = sourceOffsets.begin();
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            auto offset = sourceOffsets.empty() ? index : *source;
            if (!sourceOffsets.empty() && index + 1 < sourceOffsets.size()) ++source;
            auto mapping = EditorDisplayPosition{
                {owner, offset, folia::TextAffinity::Downstream},
                EditorDisplayPositionKind::Source};
            display.displayToSource.push_back(mapping);
            if (text[index] > 0xffff) display.displayToSource.push_back(mapping);
        }
        if (!text.empty())
        {
            display.ranges.push_back(InlineStyleRange{
                start,
                static_cast<std::uint32_t>(folia::utf16_len(text)),
                style,
                false,
                SyntaxHighlightKind::None});
        }
    }

    std::uint32_t MergeInlineProjection(InlineTextProjection& target, InlineTextProjection&& source)
    {
        auto offset = static_cast<std::uint32_t>(target.displayToSource.size());
        auto characterCount = folia::utf16_len(source.text);
        target.text += source.text;
        if (source.displayToSource.size() > characterCount) source.displayToSource.resize(characterCount);
        target.displayToSource.insert(target.displayToSource.end(), source.displayToSource.begin(), source.displayToSource.end());
        for (auto& range : source.ranges)
        {
            range.start += offset;
            target.ranges.push_back(std::move(range));
        }
        return offset;
    }

    // start and end are bounded UTF-16 offsets in the source projection.
    InlineTextProjection SliceInlineProjection(InlineTextProjection const& source, std::uint32_t start, std::uint32_t end)
    {
        InlineTextProjection result;
        const auto beginCodepoint = folia::utf16_to_char_index(source.text, start);
        const auto endCodepoint = folia::utf16_to_char_index(source.text, end);
        result.text = source.text.substr(beginCodepoint, endCodepoint - beginCodepoint);
        if (start < source.displayToSource.size())
        {
            const auto mappingEnd = (std::min)(static_cast<std::size_t>(end), source.displayToSource.size());
            result.displayToSource.insert(result.displayToSource.end(),
                source.displayToSource.begin() + static_cast<std::ptrdiff_t>(start),
                source.displayToSource.begin() + static_cast<std::ptrdiff_t>(mappingEnd));
        }
        EditorDisplayPosition endPosition = end < source.displayToSource.size()
            ? source.displayToSource[end]
            : source.displayToSource.empty() ? EditorDisplayPosition{} : source.displayToSource.back();
        result.displayToSource.push_back(endPosition);

        for (auto const& range : source.ranges)
        {
            const auto rangeEnd = range.start + range.length;
            const auto overlapStart = (std::max)(start, range.start);
            const auto overlapEnd = (std::min)(end, rangeEnd);
            if (overlapStart >= overlapEnd) continue;
            auto sliced = range;
            sliced.start = overlapStart - start;
            sliced.length = overlapEnd - overlapStart;
            result.ranges.push_back(std::move(sliced));
        }
        return result;
    }

    void ShiftStyleRangesForInsertion(InlineTextProjection& display, std::uint32_t position)
    {
        for (auto& range : display.ranges)
        {
            if (range.start >= position) ++range.start;
            else if (range.start + range.length > position) ++range.length;
        }
    }

    struct CodeSpanRange { std::uint32_t start, length; folia::InlineStyle const* style; };
    std::vector<CodeSpanRange> CodeSpanRanges(std::vector<InlineStyleRange> const& ranges)
    {
        std::vector<CodeSpanRange> spans;
        for (auto const& range : ranges) {
            if (range.marker || range.length == 0 || range.style.code != folia::CodeStyle::Inline) continue;
            if (!spans.empty() && spans.back().start + spans.back().length == range.start
                && spans.back().style->code_span_id == range.style.code_span_id)
                spans.back().length += range.length;
            else spans.push_back({range.start, range.length, &range.style});
        }
        return spans;
    }
}
