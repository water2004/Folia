#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace source_svg_paint {

// Expected-side projection of the pinned MathJax 4.1.3 SVG mtable stylesheet.
// Source fixtures remain unchanged. Only table line/frame paint supplied by
// that stylesheet is made explicit; all geometry, glyphs, transforms, ordering
// and other bytes continue to participate in the exact SVG comparison.
//
// Source: @mathjax/src/cjs/output/svg/Wrappers/mtable.js, SvgMtable.styles.
// Its default stroke width is 70px in the 1000-unit SVG coordinate system;
// dashed uses 140, and dotted uses 0,140 with a round cap. The inherited stroke
// color remains inherited. tools/math-port/svg-paint checks these declarations
// against the independently locked upstream module and actual RGBA rendering.
inline std::string materialize(std::string_view source) {
    std::string output;
    output.reserve(source.size());
    std::size_t cursor = 0;
    while (cursor < source.size()) {
        const auto line = source.find("<line ", cursor);
        const auto rect = source.find("<rect ", cursor);
        const auto begin = line < rect ? line : rect;
        if (begin == source.npos)
            break;
        const auto end = source.find('>', begin);
        if (end == source.npos)
            throw std::invalid_argument("unterminated Source SVG paint fixture element");
        const auto tag = source.substr(begin, end - begin + 1);
        const bool table = begin == line ? tag.contains(" data-line=\"")
                                        : tag.contains(" data-frame=\"");
        output.append(source.substr(cursor, begin - cursor));
        if (!table) {
            output.append(tag);
        } else {
            if (!tag.ends_with("/>"))
                throw std::invalid_argument("noncanonical Source SVG table fixture element");
            // Upstream also emits a nonstandard stroke-thickness attribute for
            // nondefault widths. CSS does not interpret it. Do not silently
            // turn those cases into a claim of upstream pixel equivalence.
            constexpr std::array paint_attributes{
                " fill=\"", " stroke=\"", " stroke-width=\"", " stroke-thickness=\"",
                " stroke-dasharray=\"", " stroke-linecap=\"", " style=\""};
            for (const auto attribute : paint_attributes)
                if (tag.contains(attribute))
                    throw std::invalid_argument("Source SVG table fixture already has explicit paint");
            const bool solid = tag.contains(" class=\"mjx-solid\"");
            const bool dashed = tag.contains(" class=\"mjx-dashed\"");
            const bool dotted = tag.contains(" class=\"mjx-dotted\"");
            if (!solid && !dashed && !dotted)
                throw std::invalid_argument("unknown Source SVG table paint class");
            output.append(tag.substr(0, tag.size() - 2));
            output += " fill=\"none\" stroke-width=\"70\"";
            if (dashed)
                output += " stroke-dasharray=\"140\"";
            if (dotted)
                output += " stroke-linecap=\"round\" stroke-dasharray=\"0 140\"";
            output += "/>";
        }
        cursor = end + 1;
    }
    output.append(source.substr(cursor));
    return output;
}

} // namespace source_svg_paint
