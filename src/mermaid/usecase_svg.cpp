// Mermaid 12.2.0 Usecase DAGRE SVG projection. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.usecase_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;

namespace folia::mermaid {
namespace {

const ConfigValue& member(const ConfigValue& value, std::u16string_view key) {
    static const ConfigValue empty{};
    const auto* found = value.find(key);
    return found ? *found : empty;
}

double number(const ConfigValue& value, double fallback) {
    return value.kind == ConfigKind::number ? value.number : fallback;
}

std::u16string string(const ConfigValue& value, std::u16string_view fallback) {
    return value.kind == ConfigKind::string ? value.string : std::u16string(fallback);
}

std::string n(double value) {
    auto result = format_number(value);
    if (!result) throw std::runtime_error("Invalid Use Case geometry");
    return *result;
}

std::string esc(std::u16string_view value) {
    auto result = escape_svg_text(value, u"Use Case text");
    if (!result) throw std::runtime_error("Invalid Use Case text");
    return *result;
}

std::string attribute(std::u16string_view value) {
    std::string result;
    for (char ch : esc(value)) {
        if (ch == '"') result += "&quot;";
        else result += ch;
    }
    return result;
}

std::string color(const ConfigValue& value, std::u16string_view key, std::string fallback) {
    const auto text = string(member(value, key), {});
    return text.empty() ? std::move(fallback) : attribute(text);
}

struct Label {
    std::u16string text;
    double width = 0, height = 0;
};

constexpr double edge_font_size = 10;
constexpr double dagre_margin = 8;
constexpr double actor_padding = 10;
constexpr double actor_figure_width = 56, actor_figure_height = 72, actor_label_gap = 8;
constexpr std::string_view stick_figure =
    "M 0 -12 C 6.627 -12 12 -17.373 12 -24 C 12 -30.627 6.627 -36 0 -36 "
    "C -6.627 -36 -12 -30.627 -12 -24 C -12 -17.373 -6.627 -12 0 -12 Z "
    "M 0 -12 V 8 M -17 -5 H 17 M 0 8 L -15 28 M 0 8 L 15 28";

std::expected<Label, std::string> measure_label(const UsecaseTextMeasure& measure,
    std::u16string text, std::u16string_view family, double size, double minimum_width = 0,
    double padding = 0) {
    if (text.empty()) return Label{};
    const auto metrics = measure(text, family, size, 400);
    if (!metrics) return std::unexpected(metrics.error());
    // The HTML label's height follows its 1.5 line-height. Width is measured
    // by the platform and rounded to the browser's 1/64 px layout grid.
    const double width = std::round(metrics->width * 64) / 64;
    return Label{std::move(text), std::max(minimum_width, width + 2 * padding), size * 1.5};
}

void paint_label(std::string& svg, const Label& label, FlowchartPoint center,
                 double size, std::u16string_view family, std::string_view color,
                 std::string_view css_class, std::string_view background = {}) {
    if (label.text.empty()) return;
    svg += "<foreignObject x=\"" + n(center.x - label.width / 2) +
        "\" y=\"" + n(center.y - label.height / 2) + "\" width=\"" + n(label.width) +
        "\" height=\"" + n(label.height) + "\"><div xmlns=\"http://www.w3.org/1999/xhtml\""
        " style=\"text-align:center;white-space:nowrap;line-height:1.5;font-family:" +
        attribute(family) + ";font-size:" + n(size) + "px;color:" + std::string(color);
    if (!background.empty()) svg += ";background-color:" + std::string(background);
    svg += "\"><span class=\"" + std::string(css_class) + "\">" +
        esc(label.text) + "</span></div></foreignObject>";
}

std::string point(FlowchartPoint p) { return n(p.x) + "," + n(p.y); }

FlowchartPoint mix(FlowchartPoint a, FlowchartPoint b, double fraction) {
    return {std::lerp(a.x, b.x, fraction), std::lerp(a.y, b.y, fraction)};
}

// D3 curveBasis on Dagre points, with endpoint offsets applied before fitting.
std::string basis(std::span<const FlowchartPoint> points) {
    std::string path = "M" + point(points.front());
    if (points.size() == 2) return path + "L" + point(points.back());
    path += "L" + point(mix(points[0], points[1], 1.0 / 6));
    const auto segment = [&](FlowchartPoint a, FlowchartPoint b, FlowchartPoint c) {
        path += "C" + point(mix(a, b, 1.0 / 3)) + "," + point(mix(a, b, 2.0 / 3)) +
            "," + point({(a.x + 4 * b.x + c.x) / 6, (a.y + 4 * b.y + c.y) / 6});
    };
    for (std::size_t i = 2; i < points.size(); ++i) segment(points[i - 2], points[i - 1], points[i]);
    segment(points[points.size() - 2], points.back(), points.back());
    return path + "L" + point(points.back());
}

FlowchartPoint ellipse_intersect(const FlowchartPlacedNode& node, FlowchartPoint toward) {
    const double dx = toward.x - node.x, dy = toward.y - node.y;
    const double scale = std::hypot(2 * dx / node.width, 2 * dy / node.height);
    return scale == 0 ? FlowchartPoint{node.x, node.y} :
        FlowchartPoint{node.x + dx / scale, node.y + dy / scale};
}

void shorten(FlowchartPoint& end, FlowchartPoint adjacent, double distance) {
    const auto length = std::hypot(end.x - adjacent.x, end.y - adjacent.y);
    if (length > 0) end = mix(end, adjacent, distance / length);
}

enum class Marker { none, point, circle, cross, extension };
struct Markers { Marker start = Marker::none, end = Marker::none; };

Markers edge_markers(const UsecaseEdge& edge) {
    if (edge.kind == UsecaseRelationship::generalization) return {.end = Marker::extension};
    if (edge.arrow == UsecaseArrow::none) return {};
    const Marker marker = edge.arrow == UsecaseArrow::point ? Marker::point :
        edge.arrow == UsecaseArrow::circle ? Marker::circle : Marker::cross;
    return edge.reverse ? Markers{.start = marker} : Markers{.end = marker};
}

std::string marker_id(Marker marker, bool start) {
    std::string_view name = "point";
    if (marker == Marker::extension) name = "extension";
    else if (marker == Marker::circle) name = "circle";
    else if (marker == Marker::cross) name = "cross";
    return "uc-" + std::string(name) + (start ? "-start" : "-end");
}

std::string marker_definitions(const UsecaseRenderTheme& theme) {
    std::string out = "<defs>";
    for (bool start : {true, false}) {
        out += "<marker id=\"" + marker_id(Marker::point, start) +
            "\" viewBox=\"0 0 10 10\" refX=\"" + (start ? "4.5" : "5") +
            "\" refY=\"5\" markerWidth=\"8\" markerHeight=\"8\" orient=\"auto\""
            " markerUnits=\"userSpaceOnUse\"><path d=\"" +
            (start ? "M 0 5 L 10 10 L 10 0 z" : "M 0 0 L 10 5 L 0 10 z") +
            "\" fill=\"" + theme.line + "\" stroke=\"" + theme.line + "\"/></marker>";
        out += "<marker id=\"" + marker_id(Marker::circle, start) +
            "\" viewBox=\"0 0 10 10\" refX=\"" + (start ? "-1" : "11") +
            "\" refY=\"5\" markerWidth=\"11\" markerHeight=\"11\" orient=\"auto\""
            " markerUnits=\"userSpaceOnUse\"><circle cx=\"5\" cy=\"5\" r=\"5\" fill=\"" +
            theme.background + "\" stroke=\"" + theme.line + "\"/></marker>";
        out += "<marker id=\"" + marker_id(Marker::cross, start) +
            "\" viewBox=\"0 0 11 11\" refX=\"" + (start ? "-1" : "12") +
            "\" refY=\"5.2\" markerWidth=\"11\" markerHeight=\"11\" orient=\"auto\""
            " markerUnits=\"userSpaceOnUse\"><path d=\"M 1,1 l 9,9 M 10,1 l -9,9\" fill=\"none\""
            " stroke-width=\"2\" stroke=\"" + theme.line + "\"/></marker>";
    }
    out += "<marker id=\"uc-extension-end\" refX=\"1\" refY=\"7\" markerWidth=\"20\""
        " markerHeight=\"28\" orient=\"auto\" markerUnits=\"userSpaceOnUse\">"
        "<path d=\"M 1,1 V 13 L18,7 Z\" fill=\"" + theme.background +
        "\" stroke=\"" + theme.line + "\"/></marker>";
    return out + "</defs>";
}

std::u16string relationship_label(const UsecaseEdge& edge) {
    if (edge.kind == UsecaseRelationship::include) return u"include";
    if (edge.kind == UsecaseRelationship::extend) return u"extend";
    return edge.label;
}

}

std::expected<UsecaseRenderInput, std::string> prepare_usecase_render(const PreparedDiagram& prepared) {
    auto parsed = parse_usecase(prepared.code.cleaned);
    if (!parsed) return std::unexpected(parsed.error());
    UsecaseRenderInput out;
    out.document = std::move(*parsed);
    if (out.document.title.empty() && prepared.title) out.document.title = *prepared.title;
    const auto& config = prepared.effective_config;
    const auto& vars = member(config, u"themeVariables");
    const auto& usecase = member(config, u"usecase");
    auto& theme = out.theme;
    theme.font_family = string(member(config, u"fontFamily"), u"Arial");
    theme.actor_font_family = string(member(usecase, u"actorFontFamily"), theme.font_family);
    theme.usecase_font_family = string(member(usecase, u"usecaseFontFamily"), theme.font_family);
    theme.font_size = number(member(usecase, u"usecaseFontSize"), 12);
    theme.actor_font_size = number(member(usecase, u"actorFontSize"), 14);
    theme.text = color(vars, u"primaryTextColor", "#333333");
    theme.actor_text = color(vars, u"actorTextColor", theme.text);
    theme.line = color(vars, u"lineColor", "#333333");
    theme.background = color(vars, u"mainBkg", "#ffffff");
    theme.node_fill = color(vars, u"usecaseBkg", theme.background);
    theme.node_border = color(vars, u"usecaseBorder",
        color(vars, u"nodeBorder", color(vars, u"primaryColor", "#9370DB")));
    theme.actor_fill = color(vars, u"usecaseActorBkg", color(vars, u"actorBkg", theme.background));
    theme.actor_border = color(vars, u"usecaseActorBorder", color(vars, u"actorBorder",
        color(vars, u"primaryColor", theme.line)));
    theme.label_background = color(vars, u"edgeLabelBackground", theme.background);
    theme.include_line = color(vars, u"usecaseIncludeLine", theme.line);
    theme.extend_line = color(vars, u"usecaseExtendLine", theme.line);
    out.node_spacing = number(member(usecase, u"nodeSpacing"), 50);
    out.rank_spacing = number(member(usecase, u"rankSpacing"), 50);
    out.diagram_padding = number(member(usecase, u"diagramPadding"), 20);
    out.min_node_width = number(member(usecase, u"minNodeWidth"), 120);
    return out;
}

std::expected<UsecaseSvg, std::string> render_usecase_svg(
    const UsecaseRenderInput& input, const UsecaseTextMeasure& measure) {
    if (!measure) return std::unexpected("Use Case rendering requires platform text measurement");
    try {
        const auto& theme = input.theme;
        FlowchartInput graph;
        graph.direction = input.document.direction;
        graph.node_spacing = input.node_spacing;
        graph.rank_spacing = input.rank_spacing;
        std::vector<FlowchartNodeBox> boxes;
        std::vector<FlowchartEdgeLabelSize> edge_sizes;
        std::vector<Label> labels, edge_labels;
        std::unordered_map<std::u16string, std::size_t> node_index;

        // UsecaseDB publishes all actors first, followed by use cases.
        std::vector<const UsecaseNode*> nodes;
        for (auto kind : {UsecaseNodeKind::actor, UsecaseNodeKind::usecase}) {
            for (const auto& node : input.document.nodes)
                if (node.kind == kind) nodes.push_back(&node);
        }
        for (const auto* node : nodes) {
            const bool actor = node->kind == UsecaseNodeKind::actor;
            auto label = measure_label(measure, node->label,
                actor ? theme.actor_font_family : theme.usecase_font_family,
                actor ? theme.actor_font_size : theme.font_size, actor ? 0 : input.min_node_width);
            if (!label) return std::unexpected(label.error());
            node_index.emplace(node->id, graph.nodes.size());
            graph.nodes.push_back({.id = node->id});
            if (actor) {
                boxes.push_back({std::max(actor_figure_width, label->width) + 2 * actor_padding,
                    actor_figure_height + actor_label_gap + label->height + 2 * actor_padding});
            } else {
                boxes.push_back({label->width + 40, label->height + 40});
            }
            labels.push_back(std::move(*label));
        }
        for (const auto& edge : input.document.edges) {
            graph.edges.push_back({.id = edge.id, .start = edge.from, .end = edge.to,
                .min_length = edge.min_length});
            auto label = measure_label(measure, relationship_label(edge), theme.font_family,
                edge_font_size, 0, 2);
            if (!label) return std::unexpected(label.error());
            edge_sizes.push_back({label->width, label->height});
            edge_labels.push_back(std::move(*label));
        }
        auto layout = layout_flowchart(graph, boxes, edge_sizes);
        if (!layout) return std::unexpected(layout.error());

        for (auto& node : layout->nodes) {
            node.x += dagre_margin;
            node.y += dagre_margin;
        }
        for (auto& edge : layout->edges) {
            for (auto& p : edge.points) {
                p.x += dagre_margin;
                p.y += dagre_margin;
            }
            if (edge.label) {
                edge.label->x += dagre_margin;
                edge.label->y += dagre_margin;
            }
        }

        const double padding = input.diagram_padding;
        UsecaseSvg result;
        result.width = layout->width + 2 * padding;
        result.height = layout->height + 2 * padding;
        std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" +
            n(dagre_margin - padding) + " " + n(dagre_margin - padding) + " " +
            n(result.width) + " " + n(result.height) + "\" aria-roledescription=\"usecase\">";
        svg += marker_definitions(theme);
        svg += "<g class=\"edgePaths\">";
        for (std::size_t i = 0; i < layout->edges.size(); ++i) {
            auto points = layout->edges[i].points;
            if (points.size() < 2) continue;
            const auto& edge = input.document.edges[i];
            const auto a = node_index.at(edge.from), b = node_index.at(edge.to);
            if (nodes[a]->kind == UsecaseNodeKind::usecase)
                points.front() = ellipse_intersect(layout->nodes[a], points[1]);
            if (nodes[b]->kind == UsecaseNodeKind::usecase)
                points.back() = ellipse_intersect(layout->nodes[b], points[points.size() - 2]);
            const auto markers = edge_markers(edge);
            if (markers.start == Marker::point) shorten(points.front(), points[1], 4);
            if (markers.end == Marker::point || markers.end == Marker::extension)
                shorten(points.back(), points[points.size() - 2], markers.end == Marker::point ? 4 : 17.25);
            const bool include = edge.kind == UsecaseRelationship::include;
            const bool extend = edge.kind == UsecaseRelationship::extend;
            svg += "<path data-et=\"edge\" class=\"relationship\" fill=\"none\" stroke=\"" +
                (include ? theme.include_line : extend ? theme.extend_line : theme.line) +
                "\" d=\"" + basis(points) + "\"";
            svg += include || extend ? " stroke-dasharray=\"3\"" : " stroke-dasharray=\"0\"";
            if (markers.start != Marker::none)
                svg += " marker-start=\"url(#" + marker_id(markers.start, true) + ")\"";
            if (markers.end != Marker::none)
                svg += " marker-end=\"url(#" + marker_id(markers.end, false) + ")\"";
            svg += "/>";
        }
        svg += "</g><g class=\"edgeLabels\">";
        for (std::size_t i = 0; i < layout->edges.size(); ++i) {
            const auto& edge = layout->edges[i];
            if (edge.label) paint_label(svg, edge_labels[i], *edge.label, edge_font_size,
                theme.font_family, theme.text, "edgeLabel", theme.label_background);
        }
        svg += "</g><g class=\"nodes\">";
        for (std::size_t i = 0; i < layout->nodes.size(); ++i) {
            const auto& box = layout->nodes[i];
            const bool actor = nodes[i]->kind == UsecaseNodeKind::actor;
            const auto& label = labels[i];
            svg += "<g class=\"node\" data-usecase-id=\"" + attribute(nodes[i]->id) +
                "\" transform=\"translate(" + n(box.x) + "," + n(box.y) + ")\">";
            if (actor) {
                const double figure_top = -box.height / 2 + actor_padding;
                svg += "<rect x=\"" + n(-box.width / 2) + "\" y=\"" + n(-box.height / 2) +
                    "\" width=\"" + n(box.width) + "\" height=\"" + n(box.height) +
                    "\" opacity=\"0\" aria-hidden=\"true\"/><path fill=\"" + theme.actor_fill +
                    "\" stroke=\"" + theme.actor_border + "\" stroke-width=\"2\" transform=\"translate(0," +
                    n(figure_top + actor_figure_height / 2) + ")\" d=\"" + std::string(stick_figure) + "\"/>";
                paint_label(svg, label, {0, figure_top + actor_figure_height + actor_label_gap + label.height / 2},
                    theme.actor_font_size, theme.actor_font_family, theme.actor_text, "nodeLabel");
            } else {
                svg += "<ellipse rx=\"" + n(box.width / 2) + "\" ry=\"" + n(box.height / 2) +
                    "\" fill=\"" + theme.node_fill + "\" stroke=\"" + theme.node_border +
                    "\" stroke-width=\"2\"/>";
                paint_label(svg, label, {}, theme.font_size, theme.usecase_font_family, theme.text, "nodeLabel");
            }
            svg += "</g>";
        }
        result.svg = svg + "</g></svg>";
        return result;
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}

}
