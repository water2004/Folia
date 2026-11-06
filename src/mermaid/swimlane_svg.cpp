module folia.mermaid.swimlane_svg;
import std;
import folia.mermaid.swimlane_layout;
import folia.mermaid.number;
import folia.mermaid.svg_text;

namespace folia::mermaid {
namespace {
const ConfigValue& member(const ConfigValue& value, std::u16string_view key) {
    static const ConfigValue empty{};
    const auto* found = value.find(key);
    return found ? *found : empty;
}
std::u16string string(const ConfigValue& value, std::u16string_view fallback = {}) {
    return value.kind == ConfigKind::string ? value.string : std::u16string(fallback);
}
double number(const ConfigValue& value, double fallback) {
    return value.kind == ConfigKind::number && std::isfinite(value.number) && value.number > 0
        ? value.number : fallback;
}
std::string narrow(std::u16string_view value) {
    auto encoded = escape_svg_text(value, u"Swimlane theme value");
    if (!encoded) throw std::runtime_error("Invalid Swimlane theme value");
    return *encoded;
}
std::string n(double value) {
    auto formatted = format_number(value);
    if (!formatted) throw std::runtime_error("Invalid Swimlane geometry");
    return *formatted;
}
std::string xml(std::u16string_view value) {
    auto escaped = escape_svg_text(value, u"Swimlane label");
    if (!escaped) throw std::runtime_error("Invalid Swimlane label");
    return *escaped;
}
std::expected<SvgTextMetrics, std::string> measure_lines(
    std::span<const std::u16string> lines, const SvgTextMeasure& measure,
    double font_size, unsigned weight) {
    SvgTextMetrics result;
    for (const auto& line : lines) {
        auto metrics = measure(line.empty() ? std::u16string_view(u"\u200b") : line,
                               font_size, weight);
        if (!metrics) return std::unexpected(metrics.error());
        result.width = std::max(result.width, metrics->width);
        result.height = std::max(result.height, metrics->height);
        result.baseline = std::max(result.baseline, metrics->baseline);
        result.middle_offset = std::max(result.middle_offset, metrics->middle_offset);
    }
    result.height *= static_cast<double>(std::max<std::size_t>(lines.size(), 1));
    return result;
}
void label_svg(std::string& out, std::span<const std::u16string> lines,
               const SvgTextMetrics& metrics, double x, double y,
               double font_size, std::string_view css_class) {
    const double line_step = font_size * 1.5;
    const double first = y - (static_cast<double>(lines.size()) - 1) * line_step / 2 +
        metrics.baseline - metrics.middle_offset;
    out += "<text class=\"" + std::string(css_class) + "\" x=\"" + n(x) +
        "\" y=\"" + n(first) + "\" text-anchor=\"middle\">";
    for (std::size_t i = 0; i < lines.size(); ++i)
        out += "<tspan x=\"" + n(x) + "\" dy=\"" +
            (i ? n(line_step) : "0") + "\">" + xml(lines[i]) + "</tspan>";
    out += "</text>";
}
void foreign_label(std::string& out, std::span<const std::u16string> lines,
                   double x, double y, double width, double height,
                   std::u16string_view family, double font_size,
                   std::string_view color, std::string_view css_class,
                   bool table_cell = false) {
    const double left = x - width / 2;
    const double top = y - height / 2;
    out += "<foreignObject class=\"" + std::string(css_class) + "\" x=\"" + n(left) +
        "\" y=\"" + n(top) + "\" width=\"" + n(width) + "\" height=\"" +
        n(height) + "\"><div xmlns=\"http://www.w3.org/1999/xhtml\" style=\"display:" +
        std::string(table_cell ? "table-cell" : "table") + ";white-space:nowrap;line-height:1.5;max-width:" +
        n(width) + "px;text-align:center;width:" + n(width) + "px;color:" +
        std::string(color) + ";font-family:" + xml(family) + ";font-size:" +
        n(font_size) + "px\"><span class=\"nodeLabel\">";
    for (const auto& line : lines) out += "<p style=\"margin:0\">" + xml(line) + "</p>";
    out += "</span></div></foreignObject>";
}
double marker_offset(std::u16string_view marker) {
    if (marker == u"arrow_point") return 4;
    if (marker == u"aggregation" || marker == u"extension" || marker == u"composition") return 17.25;
    if (marker == u"dependency") return 6;
    if (marker == u"lollipop") return 13.5;
    return 0;
}
std::vector<FlowchartPoint> offset_markers(
    std::span<const FlowchartPoint> source, const FlowchartEdge& edge) {
    std::vector<FlowchartPoint> points(source.begin(), source.end());
    if (points.size() < 2) return points;
    const auto apply = [&](std::size_t point_index, FlowchartPoint toward, double offset) {
        auto& point = points[point_index];
        const double dx = toward.x - point.x, dy = toward.y - point.y;
        const double length = std::hypot(dx, dy);
        if (length > 0 && offset > 0) {
            point.x += offset * dx / length;
            point.y += offset * dy / length;
        }
    };
    apply(0, points[1], marker_offset(edge.arrow_start));
    const auto last = points.size() - 1;
    apply(last, points[last - 1], marker_offset(edge.arrow_end));
    return points;
}
std::string edge_path(std::span<const FlowchartPoint> points, bool rounded) {
    if (points.size() < 2) return {};
    std::string path = "M" + n(points.front().x) + "," + n(points.front().y);
    constexpr double epsilon = 1e-5;
    for (std::size_t i = 1; i + 1 < points.size(); ++i) {
        const auto previous = points[i - 1], current = points[i], next = points[i + 1];
        const double dx1 = current.x - previous.x, dy1 = current.y - previous.y;
        const double dx2 = next.x - current.x, dy2 = next.y - current.y;
        const double length1 = std::hypot(dx1, dy1), length2 = std::hypot(dx2, dy2);
        if (!rounded || length1 < epsilon || length2 < epsilon) {
            path += "L" + n(current.x) + "," + n(current.y);
            continue;
        }
        const double nx1 = dx1 / length1, ny1 = dy1 / length1;
        const double nx2 = dx2 / length2, ny2 = dy2 / length2;
        const double dot = std::clamp(nx1 * nx2 + ny1 * ny2, -1.0, 1.0);
        const double angle = std::acos(dot);
        if (angle < epsilon || std::abs(std::numbers::pi - angle) < epsilon) {
            path += "L" + n(current.x) + "," + n(current.y);
            continue;
        }
        const double cut = std::min({5.0 / std::sin(angle / 2), length1 / 2, length2 / 2});
        const double start_x = current.x - nx1 * cut, start_y = current.y - ny1 * cut;
        const double end_x = current.x + nx2 * cut, end_y = current.y + ny2 * cut;
        path += "L" + n(start_x) + "," + n(start_y) + "Q" + n(current.x) + "," +
            n(current.y) + " " + n(end_x) + "," + n(end_y);
    }
    const auto& last = points.back();
    path += "L" + n(last.x) + "," + n(last.y);
    return path;
}
}

std::expected<SwimlaneRenderInput, std::string> prepare_swimlane_render(
    const PreparedDiagram& prepared) {
    if (prepared.type != u"swimlane")
        return std::unexpected("Expected a Swimlane diagram");

    // Mermaid Swimlane deliberately shares Flowchart grammar and database.
    auto graph = parse_flowchart(prepared);
    if (!graph) return std::unexpected(graph.error());

    SwimlaneRenderInput result;
    result.document = std::move(*graph);
    const auto& variables = member(prepared.effective_config, u"themeVariables");
    result.theme.font_family = string(member(prepared.effective_config, u"fontFamily"), u"Arial");
    result.theme.font_size = number(member(variables, u"fontSize"), 16);
    result.theme.background = narrow(string(member(variables, u"mainBkg"), u"#ffffff"));
    result.theme.text = narrow(string(member(variables, u"nodeTextColor"),
        string(member(variables, u"textColor"), u"#333333")));
    result.theme.line = narrow(string(member(variables, u"lineColor"), u"#333333"));
    result.theme.node_fill = narrow(string(member(variables, u"nodeBkg"), u"#ECECFF"));
    result.theme.node_border = narrow(string(member(variables, u"nodeBorder"), u"#9370DB"));
    result.theme.lane_fill = narrow(string(member(variables, u"clusterBkg"), u"#ffffde"));
    result.theme.lane_border = narrow(string(member(variables, u"clusterBorder"), u"#aaaa33"));
    result.theme.lane_text = narrow(string(member(variables, u"titleColor"), u"#333333"));
    result.theme.edge_label_background = narrow(string(member(variables, u"edgeLabelBackground"), u"#e8e8e8"));
    result.theme.arrowhead = narrow(string(member(variables, u"arrowheadColor"), u"#333333"));
    result.theme.stroke_width = number(member(variables, u"strokeWidth"), 1);
    return result;
}

std::expected<SwimlaneSvg, std::string> render_swimlane_svg(
    const SwimlaneRenderInput& input, const SvgTextMeasure& measure) {
    if (!measure) return std::unexpected("Swimlane rendering requires platform text measurement");
    try {
        const auto& graph = input.document;
        std::vector<std::vector<std::u16string>> node_lines(graph.nodes.size());
        std::vector<SvgTextMetrics> node_metrics(graph.nodes.size());
        std::vector<FlowchartNodeBox> node_boxes(graph.nodes.size());
        for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
            const auto& node = graph.nodes[i];
            if (node.group) continue;
            auto lines = flowchart_label_lines(node.label, node.label_type);
            if (!lines) return std::unexpected(lines.error());
            node_lines[i] = std::move(*lines);
            auto metrics = measure_lines(node_lines[i], measure, input.theme.font_size, 400);
            if (!metrics) return std::unexpected(metrics.error());
            node_metrics[i] = *metrics;
            const double label_width = std::max(120.0, metrics->width);
            const double label_height = std::max(input.theme.font_size * 1.5,
                input.theme.font_size * 1.5 * static_cast<double>(node_lines[i].size()));
            if (node.shape == u"diamond") {
                const double side = label_width + label_height + 30.0;
                node_boxes[i] = {side, side};
            } else {
                node_boxes[i] = {std::max(180.0, label_width + 60.0),
                                 std::max(54.0, label_height + 30.0)};
            }
        }
        std::vector<std::vector<std::u16string>> edge_lines(graph.edges.size());
        std::vector<SvgTextMetrics> edge_metrics(graph.edges.size());
        std::vector<FlowchartEdgeLabelSize> edge_sizes(graph.edges.size());
        for (std::size_t i = 0; i < graph.edges.size(); ++i) {
            const auto& edge = graph.edges[i];
            if (edge.label.empty()) continue;
            auto lines = flowchart_label_lines(edge.label, edge.label_type);
            if (!lines) return std::unexpected(lines.error());
            edge_lines[i] = std::move(*lines);
            auto metrics = measure_lines(edge_lines[i], measure, input.theme.font_size, 400);
            if (!metrics) return std::unexpected(metrics.error());
            edge_metrics[i] = *metrics;
            edge_sizes[i] = {metrics->width,
                input.theme.font_size * 1.5 * static_cast<double>(edge_lines[i].size())};
        }
        auto layout = layout_swimlanes(graph, node_boxes, edge_sizes);
        if (!layout) return std::unexpected(layout.error());
        if (layout->nodes.size() < graph.nodes.size() || layout->edges.size() != graph.edges.size())
            return std::unexpected("Swimlane layout returned inconsistent graph geometry");

        double left = std::numeric_limits<double>::infinity();
        double top = std::numeric_limits<double>::infinity();
        double right = -std::numeric_limits<double>::infinity();
        double bottom = -std::numeric_limits<double>::infinity();
        auto include = [&](double x, double y, double width, double height) {
            left = std::min(left, x); top = std::min(top, y);
            right = std::max(right, x + width); bottom = std::max(bottom, y + height);
        };
        for (const auto& node : layout->nodes)
            include(node.x - node.width / 2, node.y - node.height / 2, node.width, node.height);
        for (const auto& edge : layout->edges) for (const auto& point : edge.points)
            include(point.x - 6, point.y - 6, 12, 12);
        if (!(right > left && bottom > top)) return std::unexpected("Swimlane layout produced an empty canvas");

        std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" +
            n(left - 8) + " " + n(top - 8) + " " + n(right - left + 16) + " " +
            n(bottom - top + 16) + "\"><style>.swimlane{fill:" + input.theme.lane_fill +
            ";stroke:" + input.theme.lane_border + "}.swimlane-label{fill:" + input.theme.lane_text +
            "}.swimlane-node{fill:" + input.theme.node_fill + ";stroke:" +
            input.theme.node_border + "}.swimlane-edge{fill:none;stroke:" + input.theme.line +
            ";stroke-width:" + n(input.theme.stroke_width) + "}.swimlane-label,.swimlane-node-label,.swimlane-edge-label{font-family:\"" +
            xml(input.theme.font_family) + "\";font-size:" + n(input.theme.font_size) + "px;fill:" +
            input.theme.text + "}.swimlane-label{fill:" + input.theme.lane_text +
            "}.swimlane-node{stroke-width:" + n(input.theme.stroke_width) +
            "}.edgeLabel div{background-color:" + input.theme.edge_label_background + "}</style>";
        svg += "<defs><marker id=\"swimlane-arrow\" viewBox=\"0 0 10 10\" refX=\"9\" refY=\"5\" markerWidth=\"6\" markerHeight=\"6\" orient=\"auto-start-reverse\"><path d=\"M 0 0 L 10 5 L 0 10 z\" fill=\"" + input.theme.arrowhead + "\"/></marker></defs>";
        svg += "<g class=\"swimlane-diagram\" font-family=\"" + xml(input.theme.font_family) + "\">";
        for (std::size_t i = 0; i < layout->nodes.size(); ++i) {
            const FlowchartNode* source = i < graph.nodes.size() ? &graph.nodes[i] : nullptr;
            const bool default_lane = layout->nodes[i].id == u"__swimlane_default__";
            if ((!source || !source->group) && !default_lane) continue;
            const auto& placed = layout->nodes[i];
            svg += "<g class=\"cluster swimlane\"><rect x=\"" + n(placed.x - placed.width/2) +
                "\" y=\"" + n(placed.y - placed.height/2) + "\" width=\"" +
                n(placed.width) + "\" height=\"" + n(placed.height) + "\" rx=\"0\"/>";
            if (i < layout->title_boxes.size() && layout->title_boxes[i] &&
                (source || default_lane)) {
                const auto& title_box = *layout->title_boxes[i];
                std::array<std::u16string, 1> title{source
                    ? (source->label.empty() ? source->id : source->label)
                    : std::u16string{}};
                double width = 0;
                if (source) {
                    auto metrics = measure_lines(title, measure, input.theme.font_size, 400);
                    if (!metrics) return std::unexpected(metrics.error());
                    width = metrics->width;
                }
                const double title_height = default_lane ? 0 : 24;
                if (graph.direction == u"LR" || graph.direction == u"RL") {
                    // Mermaid's empty synthetic default-lane label has a zero
                    // measured box and is anchored 4px from the lane edge; a
                    // visible lane title is centered in its 32px band.
                    const double center_x = placed.x - placed.width / 2 +
                        (default_lane ? 4.0 : 16.0);
                    svg += "<g class=\"cluster-label swimlane-label\" transform=\"translate(" +
                        n(center_x) + "," + n(placed.y) + ") rotate(-90) translate(" +
                        n(-width / 2) + "," + n(-title_height / 2) + ")\">";
                    foreign_label(svg, title, width / 2, title_height / 2, width, title_height,
                        input.theme.font_family, input.theme.font_size, input.theme.lane_text,
                        "swimlane-label-content", true);
                    svg += "</g>";
                } else {
                    svg += "<g class=\"cluster-label swimlane-label\">";
                    foreign_label(svg, title, title_box.x, title_box.y, width, title_height,
                        input.theme.font_family, input.theme.font_size, input.theme.lane_text,
                        "swimlane-label-content", true);
                    svg += "</g>";
                }
            }
            svg += "</g>";
        }
        for (std::size_t i = 0; i < layout->edges.size(); ++i) {
            const auto& edge = layout->edges[i];
            if (edge.points.size() < 2) continue;
            const auto& source_edge = graph.edges[i];
            auto points = offset_markers(edge.points, source_edge);
            const bool rounded = source_edge.curve.empty() || source_edge.curve == u"basis" ||
                source_edge.curve == u"rounded";
            const auto path = edge_path(points, rounded);
            svg += "<g class=\"edgePath\"><path class=\"swimlane-edge\" d=\"" + path + "\" marker-end=\"url(#swimlane-arrow)\"/>";
            if (edge.label && i < edge_lines.size() && !edge_lines[i].empty())
                foreign_label(svg, edge_lines[i], edge.label->x, edge.label->y,
                    edge_metrics[i].width, edge_sizes[i].height, input.theme.font_family,
                    input.theme.font_size, input.theme.text, "edgeLabel swimlane-edge-label");
            svg += "</g>";
        }
        for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
            const auto& source = graph.nodes[i];
            if (source.group) continue;
            const auto& placed = layout->nodes[i];
            svg += "<g class=\"node swimlane-node\">";
            if (source.shape == u"diamond") {
                svg += "<polygon points=\"" + n(placed.x) + "," + n(placed.y-placed.height/2) +
                    " " + n(placed.x+placed.width/2) + "," + n(placed.y) + " " +
                    n(placed.x) + "," + n(placed.y+placed.height/2) + " " +
                    n(placed.x-placed.width/2) + "," + n(placed.y) + "\"/>";
            } else {
                svg += "<rect x=\"" + n(placed.x - placed.width/2) +
                    "\" y=\"" + n(placed.y - placed.height/2) + "\" width=\"" +
                    n(placed.width) + "\" height=\"" + n(placed.height) + "\" rx=\"5\"/>";
            }
            foreign_label(svg, node_lines[i], placed.x, placed.y, 120,
                input.theme.font_size * 1.5 * static_cast<double>(node_lines[i].size()),
                input.theme.font_family, input.theme.font_size, input.theme.text,
                "swimlane-node-label");
            svg += "</g>";
        }
        svg += "</g></svg>";
        return SwimlaneSvg{std::move(svg), right - left + 16, bottom - top + 16};
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}
}
