// Derived from Mermaid 12.2.0 pieRenderer.ts / pieStyles.ts. MIT; see ../LICENSE.
module folia.mermaid.pie_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
import folia.mermaid.theme_color;

namespace folia::mermaid {
namespace {
std::unexpected<PieError> failure(std::u16string message) {
    return std::unexpected(PieError{0, std::move(message)});
}
bool color(std::string_view value) {
    try {
        theme_detail::color_adjust(std::u16string(value.begin(), value.end()), {});
        return true;
    } catch (const std::runtime_error&) { return false; }
}
std::string attribute(std::string value) {
    std::string result;
    for (auto c : value) {
        if (c == '"') result += "&quot;";
        else if (c == '\t') result += "&#9;";
        else if (c == '\n') result += "&#10;";
        else result += c;
    }
    return result;
}
}

std::expected<PieSvg, PieError> render_pie_svg(PieDocument const& document,
    PieMeasurements const& measured, PieSvgTheme const& theme, PieSvgConfig const& config) {
    auto layout = layout_pie(document, measured, config.layout);
    if (!layout) return std::unexpected(layout.error());
    for (auto value : {theme.stroke_width, theme.outer_stroke_width, theme.opacity,
        theme.title_size, theme.section_size, theme.legend_size})
        if (!std::isfinite(value) || value < 0) return failure(u"Invalid Pie theme dimension.");
    if (theme.opacity > 1 || theme.title_size == 0 || theme.section_size == 0 || theme.legend_size == 0)
        return failure(u"Invalid Pie theme font size or opacity.");
    for (auto const& paint : theme.colors) if (!color(paint)) return failure(u"Invalid Pie palette color.");
    for (auto const& paint : {theme.stroke_color, theme.outer_stroke_color,
        theme.title_color, theme.section_color, theme.legend_color})
        if (!color(paint)) return failure(u"Invalid Pie theme color.");
    auto font = escape_svg_text(theme.font_family, u"Pie font family");
    if (!font) return failure(font.error());
    if (font->empty()) return failure(u"Pie font family is required.");
    auto title = escape_svg_text(layout->document.title, u"Pie title");
    if (!title) return failure(title.error());
    std::vector<std::string> legends;
    for (auto const& legend : layout->legends) {
        auto text = escape_svg_text(legend.text, u"Pie legend");
        if (!text) return failure(text.error());
        legends.push_back(std::move(*text));
    }
    auto num = [](double value) { return *format_number(value); };
    auto translate = [&](double x, double y) { return "translate("+num(x)+","+num(y)+")"; };
    std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\""+
        num(layout->viewbox_x)+" 0 "+num(layout->width)+" "+num(layout->height)+"\"";
    if (config.use_max_width) svg += " width=\"100%\" style=\"max-width: "+num(layout->width)+"px;\"";
    else svg += " width=\""+num(layout->width)+"\" height=\""+num(layout->height)+"\"";
    svg += "><style>.pieCircle.highlighted{scale:1.05;opacity:1;}"
        ".pieCircle.highlightedOnHover:hover{transition-duration:250ms;scale:1.05;opacity:1;}</style>";
    svg += "<g transform=\"translate(225,225)\" font-family=\""+attribute(*font)+"\"><g";
    if (config.layout.legend_position == PieLegendPosition::left ||
        config.layout.legend_position == PieLegendPosition::top)
        svg += " transform=\""+translate(layout->pie_x,layout->pie_y)+"\"";
    svg += "><circle cx=\"0\" cy=\"0\" r=\""+num(185+theme.outer_stroke_width/2)+
        "\" class=\"pieOuterCircle\" fill=\"none\" stroke=\""+theme.outer_stroke_color+
        "\" stroke-width=\""+num(theme.outer_stroke_width)+"\"/>";
    for (auto const& slice : layout->slices) {
        std::string classes = "pieCircle";
        if (config.highlight_slice == u"hover") classes += " highlightedOnHover";
        else if (config.highlight_slice == layout->document.sections[slice.section_index].label)
            classes += " highlighted";
        svg += "<path d=\""+slice.geometry.path+"\" fill=\""+theme.colors[slice.section_index%12]+
            "\" class=\""+classes+"\" stroke=\""+theme.stroke_color+"\" stroke-width=\""+
            num(theme.stroke_width)+"\" opacity=\""+num(theme.opacity)+"\"/>";
    }
    for (auto const& slice : layout->slices)
        svg += "<text transform=\""+translate(slice.geometry.label_x,slice.geometry.label_y)+
            "\" text-anchor=\"middle\" class=\"slice\" fill=\""+theme.section_color+
            "\" font-size=\""+num(theme.section_size)+"\">"+slice.percentage+"</text>";
    svg += "</g><text x=\"0\" y=\"-200\" class=\"pieTitleText\" text-anchor=\"middle\" fill=\""+
        theme.title_color+"\" font-size=\""+num(theme.title_size)+"\">"+*title+"</text>";
    for (std::size_t i = 0; i < layout->legends.size(); ++i) {
        auto const& legend = layout->legends[i];
        auto const& paint = theme.colors[i%12];
        svg += "<g class=\"legend\" transform=\""+translate(legend.x,legend.y)+
            "\"><rect width=\"18\" height=\"18\" fill=\""+paint+"\" stroke=\""+paint+
            "\"/><text x=\"22\" y=\"14\" fill=\""+theme.legend_color+"\" font-size=\""+
            num(theme.legend_size)+"\">"+legends[i]+"</text></g>";
    }
    svg += "</g></svg>";
    return PieSvg{std::move(svg),layout->width,layout->height};
}
}
