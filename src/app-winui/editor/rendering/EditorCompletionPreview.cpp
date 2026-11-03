#include "pch.h"
#include "editor/rendering/EditorContentPreparation.h"
import folia.markdown.utf;

namespace winrt::Folia {
void InsertCompletionPreview(DisplayInlineText& display, folia::TextPosition caret, std::u32string_view text) {
    if (text.empty()) return;
    auto found = std::ranges::find_if(display.displayToSource, [&](auto const& point) {
        return point.container_id == caret.container_id && point.source_offset == caret.source_offset
            && point.kind == EditorDisplayPositionKind::Source;
    });
    if (found == display.displayToSource.end()) found = std::ranges::find_if(display.displayToSource, [&](auto const& point) {
        return point.container_id == caret.container_id && point.source_offset == caret.source_offset
            && point.kind != EditorDisplayPositionKind::BoundaryDecoration;
    });
    if (found == display.displayToSource.end()) return;
    auto position = static_cast<UINT32>(found - display.displayToSource.begin());
    auto count = static_cast<UINT32>(folia::utf16_len(text));
    UINT32 units = 0; std::size_t codepoint = 0;
    while (codepoint < display.text.size() && units < position) units += display.text[codepoint++] > 0xffff ? 2u : 1u;
    if (units != position) return;
    std::vector<InlineStyleRange> ranges;
    for (auto range : display.ranges) {
        auto end = range.start + range.length;
        if (range.start >= position) { range.start += count; ranges.push_back(range); }
        else if (end <= position) ranges.push_back(range);
        else {
            auto right = range;
            range.length = position - range.start; ranges.push_back(range);
            right.start = position + count; right.length = end - position; ranges.push_back(right);
        }
    }
    InlineStyleRange ghost{position, count, folia::InlineStyle::plain()}; ghost.completion = true;
    ranges.push_back(ghost); display.ranges = std::move(ranges);
    auto shift = [&](auto& overlays) { for (auto& overlay : overlays) if (overlay.displayStart >= position) overlay.displayStart += count; };
    shift(display.mathOverlays); shift(display.imageOverlays); shift(display.mermaidOverlays);
    shift(display.indentOverlays); shift(display.blockSpacingOverlays); shift(display.taskCheckboxOverlays); shift(display.footnoteOverlays);
    display.text.insert(codepoint, text);
    EditorDisplayMapping mappings(count, {caret, EditorDisplayPositionKind::GeneratedPreview});
    display.displayToSource.insert(found, mappings.begin(), mappings.end());
}
}
