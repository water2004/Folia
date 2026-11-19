#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace source_svg_paint {

// Expected-side projection of the pinned MathJax 4.1.3 SVG stylesheet. Source
// fixtures remain unchanged. Only independently observed static SVG paint and
// viewport declarations become explicit; geometry, glyphs, transforms, child
// order and all other bytes continue to participate in exact SVG comparison.
//
// Sources: @mathjax/src/cjs/output/svg.js, Wrappers/{mtable,merror,TextNode}.js.
// Its default stroke width is 70px in the 1000-unit SVG coordinate system;
// dashed uses 140, and dotted uses 0,140 with a round cap. The inherited stroke
// color remains inherited. tools/math-port/svg-paint checks these declarations
// against the independently locked upstream modules and actual RGBA rendering.
namespace detail {
inline std::string_view attribute(std::string_view tag, std::string_view name) {
    const auto key = " " + std::string(name) + "=\"";
    const auto start = tag.find(key);
    if (start == tag.npos)
        return {};
    const auto begin = start + key.size();
    const auto end = tag.find('"', begin);
    if (end == tag.npos)
        throw std::invalid_argument("unterminated Source SVG attribute");
    return tag.substr(begin, end - begin);
}

inline bool has_attribute(std::string_view tag, std::string_view name) {
    return tag.contains(" " + std::string(name) + "=\"");
}

inline void erase_attribute(std::string &tag, std::string_view name) {
    const auto key = " " + std::string(name) + "=\"";
    const auto begin = tag.find(key);
    if (begin == tag.npos)
        return;
    const auto end = tag.find('"', begin + key.size());
    if (end == tag.npos)
        throw std::invalid_argument("unterminated Source SVG attribute");
    tag.erase(begin, end - begin + 1);
}

inline void append(std::string &tag, std::string_view attributes) {
    tag.insert(tag.size() - (tag.ends_with("/>") ? 2 : 1), attributes);
}

struct Element {
    std::string_view name;
    std::string_view tag;
    bool svg = false;
    bool within_host_svg = false;
};

inline bool html_void(std::string_view name) {
    constexpr std::array names{"area", "base", "br", "col", "command", "embed", "hr", "img",
                               "input", "keygen", "link", "menuitem", "meta", "param",
                               "source", "track", "wbr"};
    for (const auto candidate : names)
        if (name == candidate)
            return true;
    return false;
}

inline bool raw_text(std::string_view name) {
    // Match the pinned LiteAdaptor's PCDATA elements. Keeping their
    // contents opaque also preserves historic unescaped title fixture text.
    constexpr std::array names{"option", "textarea", "fieldset", "title", "style", "script"};
    for (const auto candidate : names)
        if (name == candidate)
            return true;
    return false;
}

inline bool table(Element const &element) {
    return element.name == "g" && attribute(element.tag, "data-mml-node") == "mtable";
}

inline void table_paint(std::string &tag) {
    // CSS does not interpret upstream's nonstandard stroke-thickness. Reject
    // such new fixtures instead of claiming they are equivalent to width 70.
    constexpr std::array paint_attributes{
        "fill", "stroke", "stroke-width", "stroke-thickness",
        "stroke-dasharray", "stroke-linecap", "style"};
    for (const auto name : paint_attributes)
        if (has_attribute(tag, name))
            throw std::invalid_argument("Source SVG table fixture already has explicit paint");
    const auto style = attribute(tag, "class");
    if (style != "mjx-solid" && style != "mjx-dashed" && style != "mjx-dotted")
        throw std::invalid_argument("unknown Source SVG table paint class");
    const bool dashed = style == "mjx-dashed", dotted = style == "mjx-dotted";
    append(tag, " fill=\"none\" stroke-width=\"70\"");
    if (dashed)
        append(tag, " stroke-dasharray=\"140\"");
    if (dotted)
        append(tag, " stroke-linecap=\"round\" stroke-dasharray=\"0 140\"");
}
} // namespace detail

inline std::string materialize(std::string_view source) {
    std::string output;
    output.reserve(source.size());
    std::vector<detail::Element> parents;
    std::size_t cursor = 0;
    while (cursor < source.size()) {
        const auto begin = source.find('<', cursor);
        if (begin == source.npos)
            break;
        const auto suffix = source.substr(begin);
        if (suffix.starts_with("<!--") || suffix.starts_with("<![CDATA[")) {
            const std::string_view close = suffix.starts_with("<!--") ? "-->" : "]]>";
            const auto finish = source.find(close, begin + 4);
            if (finish == source.npos)
                throw std::invalid_argument("unterminated Source SVG literal section");
            output.append(source.substr(cursor, finish + close.size() - cursor));
            cursor = finish + close.size();
            continue;
        }
        auto end = begin + 1;
        char quote = 0;
        for (; end < source.size(); ++end) {
            const auto value = source[end];
            if (quote) {
                if (value == quote) quote = 0;
            } else if (value == '\'' || value == '"') {
                quote = value;
            } else if (value == '>') {
                break;
            }
        }
        if (end == source.size())
            throw std::invalid_argument("unterminated Source SVG paint fixture element");
        const auto original = source.substr(begin, end - begin + 1);
        const bool closing = original.starts_with("</");
        const auto name_start = closing ? 2u : 1u;
        const auto name_end = original.find_first_of(" \t\r\n/>", name_start);
        const auto name = original.substr(name_start, name_end - name_start);
        output.append(source.substr(cursor, begin - cursor));
        if (closing) {
            if (parents.empty() || parents.back().name != name)
                throw std::invalid_argument("unbalanced Source SVG paint fixture");
            parents.pop_back();
            output.append(original);
        } else {
            const detail::Element empty{};
            const auto &parent = parents.empty() ? empty : parents.back();
            const auto &grandparent = parents.size() < 2 ? empty : parents[parents.size() - 2];
            std::string tag(original);
            const bool svg = name == "svg" || (parent.svg && parent.name != "foreignObject");
            const bool host_svg = name == "svg" && parent.name == "mjx-container" &&
                detail::attribute(parent.tag, "jax") == "SVG";
            const bool within_host_svg = host_svg || parent.within_host_svg;
            const bool table_child = svg && (detail::table(parent) ||
                (parent.name == "g" && grandparent.name == "svg" &&
                 detail::has_attribute(grandparent.tag, "data-table")));
            if (table_child && ((name == "line" && detail::has_attribute(tag, "data-line")) ||
                                (name == "rect" && detail::has_attribute(tag, "data-frame"))))
                detail::table_paint(tag);
            if (svg && parent.name == "g" && detail::attribute(parent.tag, "data-mml-node") == "merror") {
                if (name == "g" || (name == "rect" && detail::has_attribute(tag, "data-background"))) {
                    // The stylesheet outranks presentation colors. Inline CSS
                    // remains byte-for-byte intact and keeps higher priority.
                    detail::erase_attribute(tag, "fill");
                    detail::erase_attribute(tag, "stroke");
                    detail::append(tag, name == "g" ? " fill=\"red\" stroke=\"red\""
                                                    : " fill=\"yellow\" stroke=\"none\"");
                }
            }
            if (svg && within_host_svg && name == "a")
                detail::append(tag, " fill=\"blue\" stroke=\"blue\"");
            if (svg && within_host_svg && (name == "path" || name == "use") &&
                detail::has_attribute(tag, "data-c")) {
                detail::erase_attribute(tag, "stroke-width");
                detail::append(tag, " stroke-width=\"3\"");
            }
            const bool table_viewport = svg && parent.name == "g" && detail::table(grandparent);
            if (name == "svg" && (host_svg || table_viewport))
                detail::append(tag, " overflow=\"visible\"");
            output.append(tag);
            if (!original.ends_with("/>") && !name.starts_with('!') && !name.starts_with('?') &&
                (svg || !detail::html_void(name))) {
                // Some upstream title fixtures deliberately contain unescaped
                // '<tag>' text. Preserve it literally; it is not SVG structure.
                if (detail::raw_text(name)) {
                    const auto close = "</" + std::string(name) + ">";
                    const auto finish = source.find(close, end + 1);
                    if (finish == source.npos)
                        throw std::invalid_argument("unterminated Source SVG text element");
                    output.append(source.substr(end + 1, finish + close.size() - end - 1));
                    cursor = finish + close.size();
                    continue;
                }
                parents.push_back({name, original, svg, within_host_svg});
            }
        }
        cursor = end + 1;
    }
    output.append(source.substr(cursor));
    if (!parents.empty())
        throw std::invalid_argument("unclosed Source SVG paint fixture element");
    return output;
}

} // namespace source_svg_paint
