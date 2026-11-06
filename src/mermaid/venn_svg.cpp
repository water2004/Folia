module folia.mermaid.venn_svg;
import std;
import folia.mermaid.venn_layout;
import folia.mermaid.number;
import folia.mermaid.rough_hachure;
import folia.mermaid.svg_text;
import folia.mermaid.theme_color;

namespace folia::mermaid {
namespace {
using View = std::u16string_view;

std::u16string config_string(const ConfigValue& config, View key, View fallback = {}) {
    const auto entry = config.find(key);
    return std::u16string(entry && entry->kind == ConfigKind::string ? View(entry->string) : fallback);
}
std::string number(double value) {
    auto result = format_number(value);
    if (!result) throw std::runtime_error("Nonfinite Venn geometry");
    return *result;
}
std::string xml(View value, bool attribute = false) {
    auto result = escape_svg_text(value, u"Venn text");
    if (!result) throw std::runtime_error("Invalid Venn SVG text");
    if (!attribute) return *result;
    std::string quoted;
    for (char c : *result) {
        if (c == '"') quoted += "&quot;";
        else if (c == '\n') quoted += "&#10;";
        else if (c == '\t') quoted += "&#9;";
        else quoted += c;
    }
    return quoted;
}
std::string attr(std::string_view name, View value) {
    return " " + std::string(name) + "=\"" + xml(value, true) + "\"";
}
std::string attr(std::string_view name, double value) {
    return " " + std::string(name) + "=\"" + number(value) + "\"";
}
View style(const VennPaint* paint, View key, View fallback = {}) {
    if (paint) {
        auto it = paint->find(key);
        if (it != paint->end() && !it->second.empty()) return it->second;
    }
    return fallback;
}
const VennPaint* style_for(const VennDiagram& diagram, const VennSets& sets) {
    const auto entry = diagram.styles.find(sets);
    return entry == diagram.styles.end() ? nullptr : &entry->second;
}
std::string point(VennPoint p) { return number(p.x) + " " + number(p.y); }
std::string region_path(const VennRegion& region) {
    if (region.arcs.empty()) return "M 0 0";
    const auto& first = region.arcs.front();
    if (region.arcs.size() == 1) {
        const auto r = first.circle.radius;
        const auto arc = " a " + number(r) + " " + number(r) + " 0 1 0 ";
        return "M " + point(first.circle.center) + " m " + number(-r) + " 0" +
            arc + number(2*r) + " 0" + arc + number(-2*r) + " 0";
    }
    auto path = "M " + point(first.start);
    for (const auto& arc : region.arcs) {
        const auto r = number(arc.circle.radius);
        path += " A " + r + " " + r + " 0 " + (arc.large ? "1" : "0") + " 1 " + point(arc.end);
    }
    return path;
}
std::vector<RoughArc> region_rough_arcs(const VennRegion& region) {
    auto point = [](VennPoint p) { return RoughPoint{p.x, p.y}; };
    if (region.arcs.size() == 1) {
        const auto& circle = region.arcs.front().circle;
        const auto r = circle.radius;
        const RoughPoint left{circle.center.x - r, circle.center.y};
        const RoughPoint right{left.x + 2 * r, left.y};
        return {{left, right, r, true, false}, {right, {right.x - 2 * r, right.y}, r, true, false}};
    }
    std::vector<RoughArc> arcs;
    if (!region.arcs.empty()) {
        auto start = point(region.arcs.front().start);
        for (const auto& arc : region.arcs) {
            const auto end = point(arc.end);
            arcs.push_back({start, end, arc.circle.radius, arc.large, true});
            start = end;
        }
    }
    return arcs;
}
void draw_hatch(std::string& svg, std::string_view path, View color, double transparency) {
    svg += "<path d=\"" + std::string(path) + "\"" +
        attr("stroke", theme_detail::color_adjust(color, {{'a', -transparency}})) +
        " stroke-width=\"2\" fill=\"none\"/>";
}
std::string hand_drawn_stroke_width(View width, double fallback) {
    if (width.empty()) return attr("stroke-width", fallback);
    if (width.starts_with(u"+")) width.remove_prefix(1);
    const std::string ascii(width.begin(), width.end());
    double parsed;
    const auto result = std::from_chars(ascii.data(), ascii.data() + ascii.size(), parsed);
    if (result.ec == std::errc{} && std::isfinite(parsed) && parsed >= 0) return attr("stroke-width", parsed);
    return {};
}
std::u16string text_color(View base, bool dark) {
    try {
        return theme_detail::color_lighten(base, dark ? 30 : -30);
    } catch (const std::runtime_error&) {
        return std::u16string(base);
    }
}
void draw_text_nodes(std::string& svg, const VennRenderInput& input, double scale,
    const std::vector<VennRegion>& regions) {
    const auto& texts = input.document.texts;
    if (texts.empty()) return;
    using TextGroup = std::pair<VennSets, std::vector<const VennTextNode*>>;
    std::vector<TextGroup> groups;
    for (const auto& node : texts) {
        const auto group = std::ranges::find(groups, node.sets, &TextGroup::first);
        if (group == groups.end()) groups.push_back({node.sets, {&node}});
        else group->second.push_back(&node);
    }
    svg += "<g class=\"venn-text-nodes\">";
    for (const auto& [sets, nodes] : groups) {
        auto region = std::ranges::find_if(regions, [&](const auto& r) { return r.data.sets == sets; });
        if (region == regions.end()) continue;
        const auto [x, y] = region->label;
        auto radius = std::numeric_limits<double>::infinity();
        auto smallest = radius;
        for (const auto& [center, r] : region->circles) {
            radius = std::min(radius, r - std::hypot(center.x - x, center.y - y));
            smallest = std::min(smallest, r);
        }
        if (radius <= 0) radius = smallest * 0.6;
        const auto width = std::max(80 * scale, radius * 1.9);
        const auto height = std::max(60 * scale, radius * 1.9);
        const auto offset = region->data.label.empty() ? 0.0 : std::min(32 * scale, radius / 4);
        const auto left = x - width / 2;
        const auto top = y - height / 2 + offset + (nodes.size() <= 2 ? 30 * scale : 0);
        const auto columns = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(nodes.size()))));
        const auto rows = (nodes.size() + columns - 1) / columns;
        const auto cell_width = width / static_cast<double>(columns);
        const auto cell_height = height / static_cast<double>(rows);
        svg += "<g class=\"venn-text-area\"" + attr("font-size", 40 * scale) + ">";
        if (input.debug) {
            svg += "<circle class=\"venn-text-debug-circle\" fill=\"none\" stroke=\"purple\"" +
                attr("cx", x) + attr("cy", y) + attr("r", radius) +
                attr("stroke-width", 1.5 * scale) + " stroke-dasharray=\"" +
                number(6 * scale) + " " + number(4 * scale) + "\"/>";
        }
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const auto cell_x = left + static_cast<double>(i % columns) * cell_width;
            const auto cell_y = top + static_cast<double>(i / columns) * cell_height;
            if (input.debug) {
                svg += "<rect class=\"venn-text-debug-cell\" fill=\"none\" stroke=\"teal\"" +
                    attr("x", cell_x) + attr("y", cell_y) + attr("width", cell_width) +
                    attr("height", cell_height) + attr("stroke-width", scale) +
                    " stroke-dasharray=\"" + number(4 * scale) + " " + number(3 * scale) + "\"/>";
            }
            const auto* node = nodes[i];
            svg += "<foreignObject class=\"venn-text-node-fo\" overflow=\"visible\"" +
                attr("x", cell_x + cell_width * 0.05) + attr("y", cell_y + cell_height * 0.05) +
                attr("width", cell_width * 0.9) + attr("height", cell_height * 0.9) +
                "><span xmlns=\"http://www.w3.org/1999/xhtml\" class=\"venn-text-node\""
                " style=\"display:flex;width:100%;height:100%;white-space:normal;align-items:center;"
                "justify-content:center;text-align:center;overflow-wrap:normal;word-break:normal;color:" +
                xml(style(style_for(input.document, {node->id}), u"color", input.theme.intersection_text), true) +
                "\">" + xml(node->label.empty() ? node->id : node->label) + "</span></foreignObject>";
        }
        svg += "</g>";
    }
    svg += "</g>";
}
}

std::expected<VennRenderInput, std::string> prepare_venn_render(const PreparedDiagram& prepared) {
    auto document = parse_venn(encode_diagram_source(prepared.code.with_comments));
    if (!document) return std::unexpected(document.error());
    if (document->title.empty() && prepared.title) document->title = *prepared.title;
    const auto& config = prepared.effective_config;
    VennRenderInput input;
    input.hand_drawn = config_string(config, u"look") == u"handDrawn";
    if (const auto seed = config.find(u"handDrawnSeed");
        seed && seed->kind == ConfigKind::number && std::isfinite(seed->number)) {
        const auto wrapped = std::fmod(std::trunc(seed->number), 4294967296.0);
        input.hand_drawn_seed = static_cast<std::uint32_t>(wrapped < 0 ? wrapped + 4294967296.0 : wrapped);
    }
    input.document = std::move(*document);
    if (const auto cfg = config.find(u"venn")) {
        for (const auto& [key, target] : {
            std::pair{u"width", &input.width}, {u"height", &input.height}, {u"padding", &input.padding}}) {
            if (const auto entry = cfg->find(key)) {
                if (entry->kind != ConfigKind::number || !std::isfinite(entry->number) || entry->number < 0)
                    return std::unexpected("Invalid Venn canvas dimension");
                *target = entry->number;
            }
        }
        if (auto entry = cfg->find(u"useMaxWidth"); entry && entry->kind == ConfigKind::boolean)
            input.use_max_width = entry->boolean;
        if (auto entry = cfg->find(u"useDebugLayout"); entry && entry->kind == ConfigKind::boolean)
            input.debug = entry->boolean;
    }
    input.theme.font_family = config_string(config, u"fontFamily", u"Arial");
    if (const auto vars = config.find(u"themeVariables")) {
        auto& theme = input.theme;
        theme.font_family = config_string(*vars, u"fontFamily", theme.font_family);
        theme.primary = config_string(*vars, u"primaryColor");
        theme.intersection_text = config_string(*vars, u"vennSetTextColor",
            config_string(*vars, u"primaryTextColor", config_string(*vars, u"textColor")));
        theme.title = config_string(*vars, u"vennTitleTextColor", config_string(*vars, u"titleColor"));
        theme.root_fill = config_string(*vars, u"textColor");
        for (char16_t digit = u'1'; digit <= u'8'; ++digit) {
            auto color = config_string(*vars, std::u16string(u"venn") + digit);
            if (!color.empty()) theme.palette.push_back(std::move(color));
        }
        try {
            theme.dark = theme_detail::color_is_dark(config_string(*vars, u"background", u"#f4f4f4"));
        } catch (const std::runtime_error&) {
            return std::unexpected("Invalid Venn theme background");
        }
    }
    return input;
}

std::expected<VennSvg, std::string> render_venn_svg(const VennRenderInput& input) {
    const auto& document = input.document;
    const auto& theme = input.theme;
    const auto scale = input.width / 1600;
    const auto title_height = document.title.empty() ? 0.0 : 48 * scale;
    const auto height = input.height - title_height;
    auto regions = layout_venn(document, input.width, height, 15);
    if (!regions) return std::unexpected(regions.error());
    const auto has_zero_set = std::ranges::any_of(document.areas, [](const auto& area) {
        return area.sets.size() == 1 && area.size == 0;
    });
    std::optional<std::vector<VennRegion>> alternate_layout;
    if ((input.hand_drawn || !document.texts.empty()) && (input.padding != 15 || has_zero_set)) {
        auto layout = layout_venn(document, input.width, height, input.padding, true);
        if (!layout) return std::unexpected(layout.error());
        alternate_layout = std::move(*layout);
    }
    const auto& layout = alternate_layout ? *alternate_layout : *regions;
    try {
        std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " +
            number(input.width) + " " + number(input.height) + "\"" + attr("font-family", theme.font_family);
        if (!theme.root_fill.empty()) svg += attr("fill", theme.root_fill);
        if (input.use_max_width)
            svg += " width=\"100%\" style=\"max-width: " + number(input.width) + "px;\"";
        else svg += attr("width", input.width) + attr("height", input.height);
        svg += ">";
        if (!document.title.empty()) {
            // The Mermaid stylesheet overrides the title's presentation font size.
            svg += "<text class=\"venn-title\" font-size=\"32\" text-anchor=\"middle\""
                " dominant-baseline=\"middle\" x=\"50%\"" + attr("y", 32 * scale) +
                attr("fill", theme.title) + ">" + xml(document.title) + "</text>";
        }
        svg += "<g transform=\"translate(0, " + number(title_height) + ")\">";
        std::size_t circle_index = 0;
        for (const auto& region : *regions) {
            const auto& sets = region.data.sets;
            const auto paint = style_for(document, sets);
            const bool singleton = sets.size() == 1;
            std::u16string key;
            for (const auto& id : sets) {
                if (!key.empty()) key += u'_';
                key += id;
            }
            const auto classes = singleton ? "venn-circle venn-set-" + std::to_string(circle_index % 8)
                                           : std::string("venn-intersection");
            svg += "<g class=\"venn-area " + classes + "\"" + attr("data-venn-sets", key) + ">";
            auto label_paint = theme.intersection_text;
            if (singleton) {
                const auto palette = theme.palette.empty() ? View(theme.primary)
                    : View(theme.palette[circle_index % theme.palette.size()]);
                const auto fill = style(paint, u"fill", palette);
                const auto stroke = style(paint, u"stroke", fill);
                const auto stroke_width = style(paint, u"stroke-width");
                if (input.hand_drawn) {
                    const auto area = std::ranges::find_if(layout, [&](const auto& r) { return r.data.sets == sets; });
                    const auto& circle = area->circles.front();
                    const auto diameter = 2 * circle.radius;
                    auto paths = rough_ellipse_hachure({circle.center.x, circle.center.y}, diameter, diameter,
                        -41 + static_cast<double>(circle_index) * 60, input.hand_drawn_seed);
                    svg += "<g>";
                    draw_hatch(svg, paths.fill_path, fill, 0.7);
                    if (stroke != u"none")
                        svg += "<path d=\"" + paths.stroke_path + "\"" + attr("stroke", stroke) +
                            hand_drawn_stroke_width(stroke_width, 5 * scale) + " fill=\"none\"/>";
                    svg += "</g>";
                } else {
                    svg += "<path d=\"" + region_path(region) + "\"" + attr("fill", fill) +
                        attr("fill-opacity", style(paint, u"fill-opacity", u"0.1")) +
                        attr("stroke", stroke) + " stroke-opacity=\"0.95\"";
                    svg += stroke_width.empty() ? attr("stroke-width", 5 * scale) : attr("stroke-width", stroke_width);
                    svg += "/>";
                }
                label_paint = text_color(fill, theme.dark);
                ++circle_index;
            } else {
                auto fill = style(paint, u"fill");
                if (input.hand_drawn && !fill.empty()) {
                    svg += "<g>";
                    draw_hatch(svg, rough_arc_cross_hatch(region_rough_arcs(region), input.hand_drawn_seed), fill, 0.3);
                    svg += "</g>";
                } else {
                    svg += "<path d=\"" + region_path(region) + "\"";
                    if (!input.hand_drawn) svg += attr("fill", fill.empty() ? u"transparent" : fill);
                    svg += attr("fill-opacity", fill.empty() ? 0 : 1) + "/>";
                }
            }
            const auto x = attr("x", std::floor(region.label.x)), y = attr("y", std::floor(region.label.y));
            const View label = region.data.label.empty() && singleton ? View(sets.front()) : View(region.data.label);
            // Mermaid invokes Venn.js on a detached SVG, where its wrapping
            // callback sees zero computed text length. Browser SVG lays out
            // the resulting single tspan after attachment.
            svg += "<text class=\"label\" text-anchor=\"middle\" dy=\".35em\"" + x + y +
                attr("font-size", 48 * scale) + attr("fill", style(paint, u"color", label_paint)) +
                "><tspan dy=\"0.35em\"" + x + y + ">" + xml(label) + "</tspan></text></g>";
        }
        draw_text_nodes(svg, input, scale, layout);
        svg += "</g></svg>";
        return VennSvg{std::move(svg), input.width, input.height};
    } catch (const std::runtime_error& error) {
        return std::unexpected(error.what());
    }
}
}
