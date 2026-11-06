// Mermaid 12.2.0 Usecase DAGRE SVG projection. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.usecase_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
import folia.mermaid.flowchart_elk;
import folia.mermaid.theme_color;
import folia.mermaid.rough_polygon;

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

double font_size(const ConfigValue& value, double fallback) {
    if (value.kind == ConfigKind::number) return value.number;
    if (value.kind != ConfigKind::string) return fallback;
    std::u16string_view text(value.string);
    if (text.ends_with(u"px")) text.remove_suffix(2);
    const std::string ascii(text.begin(), text.end());
    double size = 0;
    const auto [end, error] = std::from_chars(ascii.data(), ascii.data() + ascii.size(), size);
    return error == std::errc{} && end == ascii.data() + ascii.size() && size > 0 ? size : fallback;
}

std::u16string string(const ConfigValue& value, std::u16string_view fallback) {
    return value.kind == ConfigKind::string ? value.string : std::u16string(fallback);
}

std::string n(double value) {
    auto result = format_number(value);
    if (!result) throw std::runtime_error("Invalid Use Case geometry");
    return *result;
}
std::string number(double value) { return n(value); }

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

std::vector<std::string> palette(const ConfigValue& value) {
    std::vector<std::string> out;
    if (value.kind != ConfigKind::sequence) return out;
    for (const auto& entry : value.sequence) {
        if (entry.kind != ConfigKind::string) return {};
        out.push_back(attribute(entry.string));
    }
    return out;
}

unsigned font_weight(const ConfigValue& value) {
    if (value.kind == ConfigKind::number && value.number >= 1 && value.number <= 999)
        return static_cast<unsigned>(value.number);
    if (value.string == u"bold") return 700;
    const auto text = attribute(value.string);
    unsigned weight = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), weight);
    return error == std::errc{} && end == text.data() + text.size() && weight >= 1 && weight <= 999
        ? weight : 400;
}

struct Label {
    std::vector<std::u16string> lines;
    std::vector<FlowchartLabelLine> runs;
    std::u16string family;
    double size = 0, width = 0, height = 0, baseline = 0, line_step = 0;
    double glyph_width = 0, glyph_top = 0, font_height = 0;
    unsigned weight = 400;
    bool html = true;
};
enum class SvgLabelPosition { centered, centered_bounds, start, edge };

constexpr double edge_font_size = 10;
constexpr double dagre_margin = 8;
constexpr double actor_padding = 10;
constexpr double actor_figure_width = 56, actor_figure_height = 72, actor_label_gap = 8;
constexpr std::string_view stick_figure =
    "M 0 -12 C 6.627 -12 12 -17.373 12 -24 C 12 -30.627 6.627 -36 0 -36 "
    "C -6.627 -36 -12 -30.627 -12 -24 C -12 -17.373 -6.627 -12 0 -12 Z "
    "M 0 -12 V 8 M -17 -5 H 17 M 0 8 L -15 28 M 0 8 L 15 28";
constexpr std::string_view hollow_figure =
    "M -22 -10 H 22 V 0 H 6 L 22 17 L 13 28 L 0 13 L -13 28 L -22 17 L -6 0 H -22 Z";
constexpr std::string_view awesome_figure =
    "M 0 -34 C 7.18 -34 13 -28.18 13 -21 C 13 -13.82 7.18 -8 0 -8 "
    "C -7.18 -8 -13 -13.82 -13 -21 C -13 -28.18 -7.18 -34 0 -34 Z "
    "M -24 25 C -24 7 -14 -3 0 -3 C 14 -3 24 7 24 25 C 24 28 21 30 18 30 H -18 C -21 30 -24 28 -24 25 Z";

constexpr bool elliptical(const UsecaseNode& node) {
    return node.kind == UsecaseNodeKind::usecase && node.shape == UsecaseShape::ellipse;
}

std::string shape_paint(std::string_view fill, std::string_view stroke, double width) {
    return " fill=\"" + std::string(fill) + "\" stroke=\"" + std::string(stroke) +
        "\" stroke-width=\"" + n(width) + "\"";
}

struct NodePaint { std::string_view fill, border; };

void include_rough(std::optional<RoughBounds>& bounds, const std::optional<RoughBounds>& drawing) {
    if (!drawing) return;
    if (!bounds) bounds = drawing;
    else {
        bounds->left = std::min(bounds->left, drawing->left);
        bounds->top = std::min(bounds->top, drawing->top);
        bounds->right = std::max(bounds->right, drawing->right);
        bounds->bottom = std::max(bounds->bottom, drawing->bottom);
    }
}

std::string paint_rough(const RoughPolygonResult& drawing, NodePaint paint, std::string_view style = {},
                        double stroke_width = 1.3) {
    std::string svg = "<g class=\"label-container\"" + std::string(style) + ">";
    if (!drawing.fill_path.empty()) {
        svg += "<path d=\"" + drawing.fill_path + "\"" + shape_paint("none", paint.fill, 1.5) +
            " stroke-dasharray=\"0 0\"/>";
    }
    if (!drawing.stroke_path.empty()) {
        svg += "<path d=\"" + drawing.stroke_path + "\"" + shape_paint("none", paint.border, stroke_width) +
            " stroke-dasharray=\"0 0\"/>";
    }
    return svg + "</g>";
}

NodePaint palette_paint(const UsecaseRenderTheme& theme, NodePaint paint, std::size_t index, bool enabled) {
    if (enabled && !theme.palette_border.empty()) {
        index %= theme.palette_border.size();
        paint.border = theme.palette_border[index];
        if (!theme.palette_fill.empty()) paint.fill = theme.palette_fill[index % theme.palette_fill.size()];
    }
    return paint;
}

std::string paint_actor(const UsecaseNode& node, NodePaint paint,
    double center, std::string_view style, std::string_view shadow, double path_width,
    const RoughPolygonOptions* rough = nullptr) {
    const auto fill = node.actor == UsecaseActor::hollow ? "none" : paint.fill;
    std::string svg = "<g class=\"usecase-actor-glyph\" transform=\"translate(0," + n(center) + ")\"" +
        shape_paint(fill, paint.border, 2) + std::string(style) + ">";
    if (node.actor == UsecaseActor::icon) {
        if (rough) svg += paint_rough(sketch_rectangle(-26, -28, 52, 52, *rough), paint);
        else svg += "<rect x=\"-26\" y=\"-28\" width=\"52\" height=\"52\" rx=\"4\"" +
            std::string(shadow) + "/>";
        svg += "<svg x=\"-21\" y=\"-23\" width=\"42\" height=\"42\" viewBox=\"0 0 80 80\" aria-hidden=\"true\">"
            "<rect width=\"80\" height=\"80\" fill=\"#087ebf\" stroke=\"none\"/>"
            "<text transform=\"translate(21.16,64.67)\" fill=\"#fff\" stroke=\"none\" "
            "font-family=\"ArialMT, Arial\" font-size=\"67.75\">?</text></svg>";
    } else {
        const auto glyph = node.actor == UsecaseActor::normal ? stick_figure :
            node.actor == UsecaseActor::hollow ? hollow_figure : awesome_figure;
        if (rough) {
            auto options = *rough;
            if (node.actor == UsecaseActor::hollow) options.fill = RoughFill::none;
            svg += paint_rough(sketch_svg_path(glyph, options), paint);
            if (node.actor == UsecaseActor::hollow)
                svg += paint_rough(sketch_ellipse({0, -23}, 18, 18, options), paint);
        } else {
            if (node.actor == UsecaseActor::hollow)
                svg += "<circle cy=\"-23\" r=\"9\"" + std::string(shadow) + "/>";
            svg += "<path d=\"" + std::string(glyph) + "\"";
            if (path_width > 0) svg += " stroke-width=\"" + n(path_width) + "\"";
            svg += "/>";
        }
    }
    if (node.business) {
        const bool hollow = node.actor == UsecaseActor::hollow;
        const double radius = hollow ? 9 : 12;
        const double center_y = hollow ? -23 : -24;
        constexpr double angle = std::numbers::pi / 3;
        const double x = radius * 0.6 * std::sin(angle);
        const double y = center_y + radius * 0.6 * std::cos(angle);
        const double dx = radius * 0.8 * std::cos(angle), dy = -radius * 0.8 * std::sin(angle);
        svg += "<path class=\"usecase-business-marker\" fill=\"none\" d=\"M" +
            n(x - dx) + "," + n(y - dy) + "L" + n(x + dx) + "," + n(y + dy) + "\"/>";
    }
    return svg + "</g>";
}

std::expected<Label, std::string> measure_label(const UsecaseTextMeasure& measure,
    std::u16string_view text, std::u16string_view family, double size, double minimum_width = 0,
    double padding = 0, bool markdown = false, unsigned weight = 400, double wrapping = 0,
    bool html = true) {
    if (text.empty()) return Label{};
    std::u16string encoded;
    if (!markdown) {
        for (const auto ch : text) {
            if (ch == u'<') encoded += u"&lt;";
            else if (ch == u'>') encoded += u"&gt;";
            else encoded += ch;
        }
        text = encoded;
    }
    auto lines = html ? flowchart_label_runs(text, markdown ? u"markdown" : u"text") :
        flowchart_svg_label_runs(text, markdown ? u"markdown" : u"text");
    if (!lines) return std::unexpected(lines.error());
    Label out{.family = std::u16string(family), .size = size, .line_step = size * (html ? 1.5 : 1.1),
        .weight = weight, .html = html};
    double glyph_height = 0;
    for (const auto& runs : *lines) {
        std::u16string line;
        std::vector<SvgTextRunStyle> styles;
        for (const auto& run : runs) {
            const auto strong = html ? (weight < 350 ? 400u : weight < 550 ? 700u : 900u) : 700u;
            styles.push_back({line.size(), run.text.size(), run.strong ? strong : html ? weight : 400u,
                run.emphasis});
            line += run.text;
        }
        auto measure_slice = [&](std::size_t begin, std::size_t end) {
            std::vector<SvgTextRunStyle> sliced;
            for (const auto& run : styles) {
                const auto start = std::max(run.offset, begin);
                const auto stop = std::min(run.offset + run.length, end);
                if (start < stop)
                    sliced.push_back({start - begin, stop - start, run.weight, run.italic});
            }
            const bool styled = std::ranges::any_of(sliced, [&](const auto& run) {
                return run.weight != weight || run.italic;
            });
            return measure(std::u16string_view(line).substr(begin, end - begin), family, size, weight,
                {.width = html ? SvgTextWidth::advance : SvgTextWidth::bounds,
                 .styles = styled ? std::span<const SvgTextRunStyle>(sliced) : std::span<const SvgTextRunStyle>{}});
        };
        auto append_line = [&](std::size_t begin, std::size_t end, const SvgTextMetrics& metrics) {
            FlowchartLabelLine display;
            for (std::size_t i = 0; i < runs.size(); ++i) {
                const auto& run = styles[i];
                const auto start = std::max(run.offset, begin);
                const auto stop = std::min(run.offset + run.length, end);
                if (start < stop)
                    display.push_back({line.substr(start, stop - start), runs[i].strong, runs[i].emphasis});
            }
            out.lines.push_back(line.substr(begin, end - begin));
            out.runs.push_back(std::move(display));
            out.width = std::max(out.width, metrics.width);
            glyph_height = std::max(glyph_height, metrics.height);
            out.baseline = std::max(out.baseline, metrics.baseline);
        };
        auto natural = measure_slice(0, line.size());
        if (!natural) return std::unexpected(natural.error());
        if (wrapping <= 0 || natural->width <= wrapping || line.find(u' ') == line.npos) {
            append_line(0, line.size(), *natural);
            continue;
        }
        auto begin = std::size_t{0};
        auto previous = line.find(u' ');
        for (auto end = line.find(u' ', previous + 1);; end = line.find(u' ', end + 1)) {
            if (end == line.npos) end = line.size();
            auto candidate = measure_slice(begin, end);
            if (!candidate) return std::unexpected(candidate.error());
            if (candidate->width > wrapping) {
                auto fitting = measure_slice(begin, previous);
                if (!fitting) return std::unexpected(fitting.error());
                append_line(begin, previous, *fitting);
                begin = previous + 1;
            }
            previous = end;
            if (end == line.size()) {
                auto last = measure_slice(begin, end);
                if (!last) return std::unexpected(last.error());
                append_line(begin, end, *last);
                break;
            }
        }
        if (html) out.width = std::max(out.width, wrapping);
    }
    // The HTML label's height follows its 1.5 line-height. Width is measured
    // by the platform and rounded to the browser's 1/64 px layout grid.
    out.glyph_width = out.width;
    out.glyph_top = size - out.baseline;
    out.font_height = glyph_height;
    out.width = std::max(minimum_width, std::round(out.width * 64) / 64 + 2 * padding);
    out.height = html ? out.lines.size() * out.line_step :
        glyph_height + (out.lines.size() - 1) * out.line_step + 2 * padding;
    return out;
}

void paint_label(std::string& svg, const Label& label, FlowchartPoint center,
                 std::string_view color, std::string_view css_class, std::string_view background = {},
                 SvgLabelPosition position = SvgLabelPosition::centered, const Label* svg_background = nullptr) {
    if (label.lines.empty()) return;
    const auto size = label.size;
    const auto weight = label.weight;
    const auto& family = label.family;
    if (!label.html) {
        const bool edge = position == SvgLabelPosition::edge;
        const bool center_bounds = position == SvgLabelPosition::centered_bounds;
        const auto x = center.x - (center_bounds ? label.glyph_width / 2 :
            position == SvgLabelPosition::centered ? label.width / 2 : 0);
        const auto top = center.y - label.height / 2;
        const auto baseline = top + (edge ? label.baseline : size - (center_bounds ? label.glyph_top : 0));
        if (svg_background) {
            const auto y = baseline - size + svg_background->size - svg_background->baseline - 2;
            svg += "<rect class=\"background\" x=\"" + n(x - svg_background->width / 2) +
                "\" y=\"" + n(y) + "\" width=\"" + n(svg_background->width) +
                "\" height=\"" + n(svg_background->height) + "\" fill=\"" + std::string(background) + "\"/>";
        }
        svg += "<text class=\"" + std::string(css_class) + "\" font-family=\"" + attribute(family) +
            "\" font-size=\"" + n(size) + "\" font-weight=\"" + std::to_string(weight) +
            "\" fill=\"" + std::string(color) + "\"" + (edge ? " text-anchor=\"middle\"" : "") + ">";
        for (std::size_t i = 0; i < label.runs.size(); ++i) {
            svg += "<tspan x=\"" + n(x) + "\" y=\"" + n(baseline + i * label.line_step) + "\">";
            for (const auto& run : label.runs[i]) {
                svg += "<tspan font-weight=\"" + std::to_string(run.strong ? 700u : 400u) +
                    "\" font-style=\"" + std::string(run.emphasis ? "italic" : "normal") + "\">" +
                    esc(run.text) + "</tspan>";
            }
            svg += "</tspan>";
        }
        svg += "</text>";
        return;
    }
    const auto top = center.y - label.height / 2;
    const auto left = center.x - label.width / 2;
    const auto baseline = top + label.baseline + (label.line_step - label.font_height) / 2;
    if (!background.empty()) {
        svg += "<rect class=\"background\" x=\"" + n(left) + "\" y=\"" + n(top) +
            "\" width=\"" + n(label.width) + "\" height=\"" + n(label.height) +
            "\" fill=\"" + std::string(background) + "\"/>";
    }
    svg += "<text xml:space=\"preserve\" class=\"" + std::string(css_class) +
        "\" font-family=\"" + attribute(family) + "\" font-size=\"" + n(size) +
        "\" font-weight=\"" + std::to_string(weight) + "\" fill=\"" + std::string(color) +
        "\" text-anchor=\"middle\" data-label-box=\"" + n(left) + " " + n(top) + " " +
        n(label.width) + " " + n(label.height) + "\">";
    const auto strong = weight < 350 ? 400u : weight < 550 ? 700u : 900u;
    for (std::size_t i = 0; i < label.runs.size(); ++i) {
        svg += "<tspan x=\"" + n(center.x) + "\" y=\"" + n(baseline + i * label.line_step) + "\">";
        for (const auto& run : label.runs[i]) {
            svg += "<tspan font-weight=\"" + std::to_string(run.strong ? strong : weight) +
                "\" font-style=\"" + std::string(run.emphasis ? "italic" : "normal") + "\">" +
                esc(run.text) + "</tspan>";
        }
        svg += "</tspan>";
    }
    svg += "</text>";
}

struct JsonTable {
    struct Row { Label key, value; double height; };
    std::vector<Row> rows;
    std::vector<RoughPolygonResult> rough_cells;
    std::optional<RoughBounds> rough_bounds;
    double inner_width = 0, inner_height = 0, key_width = 0;
};

std::expected<JsonTable, std::string> measure_json(const UsecaseNode& node,
    const Label& title, const UsecaseRenderTheme& theme, const UsecaseTextMeasure& measure,
    const RoughPolygonOptions* rough, bool html) {
    JsonTable table;
    double value_width = 0;
    table.inner_height = title.height + 8;
    for (const auto& row : node.json_rows) {
        auto key = measure_label(measure, row.key, theme.font_family, theme.diagram_font_size, 0, 0, false, 400, 120, html);
        if (!key) return std::unexpected(key.error());
        auto value = measure_label(measure, row.value, theme.font_family, theme.diagram_font_size, 0, 0, false, 400, 120, html);
        if (!value) return std::unexpected(value.error());
        const double height = std::max(key->height, value->height) + 8;
        table.key_width = std::max(table.key_width, key->width);
        value_width = std::max(value_width, value->width);
        table.inner_height += height;
        table.rows.push_back({std::move(*key), std::move(*value), height});
    }
    table.key_width += 16;
    table.inner_width = std::max(title.width + 16, table.key_width + value_width + 16);
    if (rough) {
        const double left = -table.inner_width / 2, top = -table.inner_height / 2;
        const double title_height = title.height + 8;
        const auto add_cell = [&](double x, double y, double width, double height, double offset = 0) {
            table.rough_cells.push_back(sketch_rectangle(x, y, width, height, *rough));
            auto bounds = table.rough_cells.back().bounds;
            if (bounds) {
                bounds->top += offset;
                bounds->bottom += offset;
            }
            include_rough(table.rough_bounds, bounds);
        };
        add_cell(left - 1, top - 1, table.inner_width + 2, table.inner_height + 2);
        add_cell(left, top, table.inner_width, title_height);
        double y = top + title_height;
        for (const auto& row : table.rows) {
            add_cell(left + table.key_width, 0, table.inner_width - table.key_width, row.height, y);
            add_cell(left, 0, table.key_width, row.height, y);
            y += row.height;
        }
    }
    return table;
}

void paint_json(std::string& svg, const Label& title, const JsonTable& table,
    const UsecaseRenderTheme& theme, std::string_view style, std::string_view shadow) {
    const double left = -table.inner_width / 2, top = -table.inner_height / 2;
    const double title_height = title.height + 8, value_width = table.inner_width - table.key_width;
    const auto paint = shape_paint(theme.background, theme.node_border, 1) + std::string(style);
    std::size_t cell = 0;
    const auto rect = [&](double x, double y, double width, double height) {
        if (!table.rough_cells.empty()) {
            svg += paint_rough(table.rough_cells[cell++], {theme.background, theme.rough_border}, style);
            return;
        }
        svg += "<rect x=\"" + n(x) + "\" y=\"" + n(y) + "\" width=\"" + n(width) +
            "\" height=\"" + n(height) + "\"" + paint + std::string(shadow) + "/>";
    };
    const auto text = [&](const Label& label, double x, double y) {
        paint_label(svg, label, {x, y}, theme.text, "nodeLabel", {}, SvgLabelPosition::centered_bounds);
    };
    rect(left - 1, top - 1, table.inner_width + 2, table.inner_height + 2);
    rect(left, top, table.inner_width, title_height);
    text(title, 0, top + title_height / 2);
    double y = top + title_height;
    for (const auto& row : table.rows) {
        svg += "<g transform=\"translate(0," + n(y) + ")\">";
        rect(left + table.key_width, 0, value_width, row.height);
        rect(left, 0, table.key_width, row.height);
        text(row.key, left + table.key_width / 2, row.height / 2);
        text(row.value, left + table.key_width + value_width / 2, row.height / 2);
        svg += "</g>";
        y += row.height;
    }
}

std::string point(FlowchartPoint p) { return n(p.x) + "," + n(p.y); }

FlowchartPoint mix(FlowchartPoint a, FlowchartPoint b, double fraction) {
    return {std::lerp(a.x, b.x, fraction), std::lerp(a.y, b.y, fraction)};
}

FlowchartPoint ellipse_intersect(const FlowchartPlacedNode& node, FlowchartPoint toward) {
    const double dx = toward.x - node.x, dy = toward.y - node.y;
    const double scale = std::hypot(2 * dx / node.width, 2 * dy / node.height);
    return scale == 0 ? FlowchartPoint{node.x, node.y} :
        FlowchartPoint{node.x + dx / scale, node.y + dy / scale};
}

FlowchartPoint intersect(const FlowchartPlacedNode& node, std::u16string_view shape,
                        FlowchartPoint toward) {
    if (shape == u"ellipse") return ellipse_intersect(node, toward);
    const double dx = toward.x - node.x, dy = toward.y - node.y;
    const double scale = std::max(std::abs(2 * dx / node.width), std::abs(2 * dy / node.height));
    return scale == 0 ? FlowchartPoint{node.x, node.y} :
        FlowchartPoint{node.x + dx / scale, node.y + dy / scale};
}

#include "flowchart_elk_paint.inc"
#include "flowchart_neo.inc"

// D3 curveBasis on Dagre points, with endpoint offsets applied before fitting.
std::string basis(std::span<const FlowchartPoint> points, double* length) {
    EdgePath path(points.front(), length);
    if (points.size() > 2) {
        path.line(mix(points[0], points[1], 1.0 / 6));
        const auto segment = [&](FlowchartPoint a, FlowchartPoint b, FlowchartPoint c) {
            path.cubic(mix(a, b, 1.0 / 3), mix(a, b, 2.0 / 3),
                {(a.x + 4 * b.x + c.x) / 6, (a.y + 4 * b.y + c.y) / 6});
        };
        for (std::size_t i = 2; i < points.size(); ++i) segment(points[i - 2], points[i - 1], points[i]);
        segment(points[points.size() - 2], points.back(), points.back());
    }
    path.line(points.back());
    return std::move(path.svg);
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

std::string marker_id(Marker marker, bool start, bool neo = false) {
    std::string_view name = "point";
    if (marker == Marker::extension) name = "extension";
    else if (marker == Marker::circle) name = "circle";
    else if (marker == Marker::cross) name = "cross";
    return std::string(neo ? "uc-neo-" : "uc-") + std::string(name) + (start ? "-start" : "-end");
}

std::u16string_view marker_name(Marker marker) {
    switch (marker) {
    case Marker::point: return u"arrow_point";
    case Marker::circle: return u"arrow_circle";
    case Marker::cross: return u"arrow_cross";
    default: return u"none";
    }
}

std::string marker_definitions(const UsecaseRenderTheme& theme, bool neo) {
    if (neo) {
        auto out = neo_marker_definitions(theme.line, "uc-neo");
        out += "<defs><marker id=\"uc-neo-extension-end\" refX=\"9\" refY=\"7\" markerWidth=\"20\""
            " markerHeight=\"28\" orient=\"auto\" markerUnits=\"userSpaceOnUse\" viewBox=\"0 0 20 14\">"
            "<polygon points=\"10,1 10,13 18,7\" fill=\"" + theme.background +
            "\" stroke-width=\"2\" stroke=\"" + theme.line + "\"/></marker></defs>";
        return out;
    }
    std::string out = "<defs>";
    for (bool start : {true, false}) {
        out += "<marker id=\"" + marker_id(Marker::point, start) +
            "\" viewBox=\"0 0 10 10\" refX=\"" + (start ? "4.5" : "5") +
            "\" refY=\"5\" markerWidth=\"8\" markerHeight=\"8\" orient=\"auto\""
            " markerUnits=\"userSpaceOnUse\"><path d=\"" +
            (start ? "M 0 5 L 10 10 L 10 0 z" : "M 0 0 L 10 5 L 0 10 z") +
            "\" fill=\"" + theme.line + "\" stroke=\"" + theme.line +
            "\" stroke-width=\"1\" stroke-dasharray=\"1 0\"/></marker>";
        out += "<marker id=\"" + marker_id(Marker::circle, start) +
            "\" viewBox=\"0 0 10 10\" refX=\"" + (start ? "-1" : "11") +
            "\" refY=\"5\" markerWidth=\"11\" markerHeight=\"11\" orient=\"auto\""
            " markerUnits=\"userSpaceOnUse\"><circle cx=\"5\" cy=\"5\" r=\"5\" fill=\"" +
            theme.line + "\" stroke=\"" + theme.line +
            "\" stroke-width=\"1\" stroke-dasharray=\"1 0\"/></marker>";
        out += "<marker id=\"" + marker_id(Marker::cross, start) +
            "\" viewBox=\"0 0 11 11\" refX=\"" + (start ? "-1" : "12") +
            "\" refY=\"5.2\" markerWidth=\"11\" markerHeight=\"11\" orient=\"auto\""
            " markerUnits=\"userSpaceOnUse\"><path d=\"M 1,1 l 9,9 M 10,1 l -9,9\" fill=\"" +
            theme.line + "\" stroke-width=\"2\" stroke=\"" + theme.line +
            "\" stroke-dasharray=\"1 0\"/></marker>";
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

std::u16string_view trim_css(std::u16string_view value) {
    const auto first = value.find_first_not_of(u" \t");
    if (first == std::u16string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(u" \t") - first + 1);
}

std::string paint_style(const UsecaseDocument& document,
    const std::vector<std::u16string>& classes, const std::vector<std::u16string>& styles) {
    std::map<std::u16string_view, std::u16string_view> values;
    const auto apply = [&](const auto& declarations) {
        for (const auto& declaration : declarations) {
            const auto split = declaration.find(u':');
            if (split == std::u16string::npos) continue;
            const auto key = trim_css(std::u16string_view(declaration).substr(0, split));
            const auto value = trim_css(std::u16string_view(declaration).substr(split + 1));
            if (key != u"fill" && key != u"stroke" && key != u"stroke-width" &&
                key != u"stroke-dasharray" && key != u"opacity") continue;
            if (value.find_first_of(u";{}<>\\") != std::u16string_view::npos ||
                value.find(u"url(") != std::u16string_view::npos) continue;
            values[key] = value;
        }
    };
    for (const auto& cls : classes)
        if (const auto found = document.class_styles.find(cls); found != document.class_styles.end())
            apply(found->second);
    apply(styles);
    if (values.empty()) return {};
    std::string out = " style=\"";
    for (const auto& [key, value] : values) out += attribute(key) + ":" + attribute(value) + ";";
    return out + "\"";
}

std::optional<RoughBounds> paint_boundary(std::string& svg, const FlowchartPlacedNode& box, const Label& label,
    const UsecaseNode& node, const UsecaseRenderInput& input, std::size_t slot, std::string_view style,
    std::string_view shadow, const RoughPolygonOptions* rough) {
    const bool package = node.shape == UsecaseShape::package;
    const double width = std::max(box.width, label.width + 20);
    const double tab_height = package ? label.height + 10 : 0;
    const double height = package ? std::max(box.height, tab_height + 40) : box.height;
    const double tab_width = package ? std::min(width, std::max(80.0, label.width + 20)) : 0;
    const double left = box.x - width / 2, top = box.y - height / 2;
    const auto& theme = input.theme;
    const auto colors = palette_paint(theme, {theme.boundary_fill, theme.boundary_border}, slot, true);
    const auto paint = shape_paint(colors.fill, colors.border, 1);
    std::optional<RoughBounds> bounds;
    const auto rect = [&](double y, double w, double h, std::string_view name) {
        if (rough) {
            const auto drawing = sketch_rectangle(left, y, w, h, *rough);
            include_rough(bounds, drawing.bounds);
            svg += "<g class=\"" + std::string(name) + "\">" + paint_rough(drawing, colors, style, 1) + "</g>";
            return;
        }
        svg += "<rect class=\"" + std::string(name) + "\" x=\"" + n(left) +
            "\" y=\"" + n(y) + "\" width=\"" + n(w) + "\" height=\"" + n(h) +
            "\"" + paint + std::string(style) + std::string(shadow) + "/>";
    };
    svg += "<g class=\"cluster system-boundary\" data-usecase-id=\"" + attribute(node.id) + "\">";
    if (package) rect(top, tab_width, tab_height, "boundary-tab");
    rect(top + tab_height, width, height - tab_height, "boundary-body");
    paint_label(svg, label, {package ? left + tab_width / 2 : box.x,
        package ? top + tab_height / 2 : top + input.group_title_top + label.height / 2},
        theme.boundary_text, "nodeLabel");
    svg += "</g>";
    return bounds;
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
    theme.diagram_font_size = font_size(member(vars, u"fontSize"), 16);
    theme.neo_actor_path_width = number(member(vars, u"strokeWidth"), 1);
    theme.actor_weight = font_weight(member(usecase, u"actorFontWeight"));
    theme.usecase_weight = font_weight(member(usecase, u"usecaseFontWeight"));
    theme.text = color(vars, u"primaryTextColor", "#333333");
    theme.svg_text = color(vars, u"textColor", "#333333");
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
    theme.boundary_fill = color(vars, u"usecaseBoundaryBkg", color(vars, u"clusterBkg", "#ffffde"));
    theme.boundary_border = color(vars, u"usecaseBoundaryBorder", color(vars, u"clusterBorder", "#aaaa33"));
    theme.boundary_text = color(vars, u"titleColor", color(vars, u"textColor", "#333333"));
    theme.note_fill = color(vars, u"noteBkgColor", "#fff5ad");
    theme.note_border = color(vars, u"noteBorderColor", "#aaaa33");
    theme.note_text = color(vars, u"noteTextColor", theme.text);
    const auto theme_name = string(member(config, u"theme"), {});
    out.dark = config_truthy(member(vars, u"darkMode"));
    theme.drop_shadow = color(vars, u"dropShadow", {});
    theme.rough_border = color(vars, u"nodeBorder", theme.node_border);
    if (theme_name == u"redux-color" || theme_name == u"redux-dark-color") {
        theme.palette_fill = palette(member(vars, u"bkgColorArray"));
        theme.palette_border = palette(member(vars, u"borderColorArray"));
        out.rotate_colors = string(member(usecase, u"colorScheme"), {}) == u"rotate";
    }
    out.node_spacing = number(member(usecase, u"nodeSpacing"), 50);
    out.rank_spacing = number(member(usecase, u"rankSpacing"), 50);
    out.diagram_padding = number(member(usecase, u"diagramPadding"), 20);
    out.min_node_width = number(member(usecase, u"minNodeWidth"), 120);
    out.wrapping_width = number(member(usecase, u"wrappingWidth"), 120);
    const auto& html = member(config, u"htmlLabels");
    const auto& legacy_html = member(member(config, u"flowchart"), u"htmlLabels");
    out.html_labels = html.kind != ConfigKind::null ? config_truthy(html) :
        legacy_html.kind == ConfigKind::null || config_truthy(legacy_html);
    out.elk = string(member(config, u"layout"), u"elk") == u"elk";
    out.neo = string(member(config, u"look"), u"classic") == u"neo";
    out.hand_drawn = string(member(config, u"look"), u"classic") == u"handDrawn";
    const double seed = number(member(config, u"handDrawnSeed"), 0);
    if (seed >= 0 && seed <= std::numeric_limits<std::uint32_t>::max())
        out.hand_drawn_seed = static_cast<std::uint32_t>(seed);
    const auto& use_max_width = member(usecase, u"useMaxWidth");
    out.use_max_width = use_max_width.kind == ConfigKind::null || config_truthy(use_max_width);
    const auto& margins = member(member(config, u"flowchart"), u"subGraphTitleMargin");
    out.group_title_top = number(member(margins, u"top"), 0);
    out.group_title_bottom = number(member(margins, u"bottom"), 0);
    return out;
}

std::expected<UsecaseSvg, std::string> render_usecase_svg(
    const UsecaseRenderInput& input, const UsecaseTextMeasure& measure) {
    if (!measure) return std::unexpected("Use Case rendering requires platform text measurement");
    try {
        const auto& theme = input.theme;
        const RoughPolygonOptions rough{.seed = input.hand_drawn_seed};
        const auto rough_options = input.hand_drawn ? &rough : nullptr;
        FlowchartSvgTheme neo_theme;
        neo_theme.drop_shadow = theme.drop_shadow;
        neo_theme.dark_shadow = input.dark;
        const auto shadow = input.neo ? neo_shadow(neo_theme) : NeoShadow{};
        const auto shadow_attribute = shadow.attribute();
        FlowchartInput graph;
        graph.direction = input.document.direction;
        graph.node_spacing = input.node_spacing;
        graph.rank_spacing = input.rank_spacing;
        std::vector<FlowchartNodeBox> boxes, nominal_boxes;
        std::vector<RoughPolygonResult> rough_nodes;
        std::vector<FlowchartEdgeLabelSize> edge_sizes;
        std::vector<Label> labels, stereotypes, edge_labels, edge_backgrounds;
        std::vector<JsonTable> json_tables;
        FlowchartMeasurements measured;
        std::unordered_map<std::u16string, std::size_t> node_index;

        // UsecaseDB publishes participants, notes, and system boundaries separately.
        std::vector<const UsecaseNode*> nodes;
        for (auto kind : {UsecaseNodeKind::actor, UsecaseNodeKind::usecase, UsecaseNodeKind::note,
                          UsecaseNodeKind::json, UsecaseNodeKind::boundary}) {
            for (const auto& node : input.document.nodes)
                if (node.kind == kind) nodes.push_back(&node);
        }
        for (const auto* node : nodes) {
            const bool actor = node->kind == UsecaseNodeKind::actor;
            const bool usecase = node->kind == UsecaseNodeKind::usecase;
            const bool note = node->kind == UsecaseNodeKind::note;
            const bool json = node->kind == UsecaseNodeKind::json;
            const bool group = node->kind == UsecaseNodeKind::boundary;
            const bool usecase_font = usecase && (input.html_labels || node->business);
            const auto& family = actor ? theme.actor_font_family :
                usecase_font ? theme.usecase_font_family : theme.font_family;
            const double size = actor ? theme.actor_font_size :
                usecase_font ? theme.font_size : theme.diagram_font_size;
            const auto weight = actor ? theme.actor_weight : usecase_font ? theme.usecase_weight : 400u;
            auto label = measure_label(measure, node->label, family, size,
                usecase || json || note ? input.min_node_width : 0, 0, node->markdown, weight,
                input.wrapping_width, input.html_labels);
            if (!label) return std::unexpected(label.error());
            if (!input.html_labels && (actor || (elliptical(*node) && node->business))) {
                // labelHelper measures before the actor/business caption class
                // changes the SVG font. The already measured caption retains
                // its width, height and origin when that class is applied.
                auto before_class = measure_label(measure, node->label, theme.font_family,
                    theme.diagram_font_size, usecase ? input.min_node_width : 0,
                    0, node->markdown, 400, input.wrapping_width, false);
                if (!before_class) return std::unexpected(before_class.error());
                label->width = before_class->width;
                label->height = before_class->height;
                label->glyph_width = before_class->glyph_width;
                label->glyph_top = before_class->glyph_top;
                label->runs = std::move(before_class->runs);
                label->lines = std::move(before_class->lines);
            }
            auto stereotype = measure_label(measure,
                node->stereotype.empty() ? std::u16string{} : u"«" + node->stereotype + u"»",
                family, size, 0, 0, false, weight, input.wrapping_width, input.html_labels);
            if (!stereotype) return std::unexpected(stereotype.error());
            node_index.emplace(node->id, graph.nodes.size());
            graph.nodes.push_back({.id = node->id, .label = node->label,
                .label_type = node->markdown ? u"markdown" : u"text",
                .shape = group ? u"rect" : u"squareRect",
                .parent = node->parent, .group = group, .padding = 20});
            const double text_width = std::max(label->width, stereotype->width);
            const double text_height = label->height + stereotype->height +
                (stereotype->height > 0 && (actor || node->business) ? 2 : 0);
            JsonTable table;
            if (json) {
                auto measured_table = measure_json(*node, *label, theme, measure, rough_options, input.html_labels);
                if (!measured_table) return std::unexpected(measured_table.error());
                table = std::move(*measured_table);
                boxes.push_back({table.inner_width + 2, table.inner_height + 2});
            } else if (actor) {
                boxes.push_back({std::max(actor_figure_width, text_width) + 2 * actor_padding,
                    actor_figure_height + actor_label_gap + text_height + 2 * actor_padding});
            } else if (usecase || note) {
                const double padding_x = elliptical(*node) ? 40 : note ? 20 : input.neo ? 32 : 40;
                const double padding_y = elliptical(*node) ? 40 : note ? 20 : input.neo ? 24 : 20;
                boxes.push_back({text_width + padding_x, text_height + padding_y});
            } else {
                boxes.push_back({text_width + 20, text_height + 40});
            }
            nominal_boxes.push_back(boxes.back());
            auto& drawing = rough_nodes.emplace_back();
            if (json && table.rough_bounds) {
                drawing.bounds = table.rough_bounds;
                boxes.back() = {drawing.bounds->width(), drawing.bounds->height()};
            }
            if (input.hand_drawn && (note || usecase)) {
                auto& box = boxes.back();
                drawing = elliptical(*node) ? sketch_ellipse({0, 0}, box.width, box.height, rough) :
                    sketch_rectangle(-box.width / 2, -box.height / 2, box.width, box.height, rough);
                if (drawing.bounds) box = {drawing.bounds->width(), drawing.bounds->height()};
            }
            measured.nodes.push_back({.width = label->width, .height = label->height,
                .lines = label->lines});
            labels.push_back(std::move(*label));
            stereotypes.push_back(std::move(*stereotype));
            json_tables.push_back(std::move(table));
        }
        for (const auto& edge : input.document.edges) {
            const auto text = relationship_label(edge);
            graph.edges.push_back({.id = edge.id, .start = edge.from, .end = edge.to, .label = text,
                .min_length = edge.min_length});
            auto label = measure_label(measure, text, theme.font_family, edge_font_size, 0,
                input.html_labels ? 2 : 0, edge.markdown, 400, 200, input.html_labels);
            if (!label) return std::unexpected(label.error());
            // createText sizes the SVG background before the edge group CSS
            // sets the visible text to the smaller relationship font.
            Label background;
            if (!input.html_labels && !text.empty()) {
                auto large = measure_label(measure, text, theme.font_family, theme.diagram_font_size,
                    0, 2, edge.markdown, 400, 200, false);
                if (!large) return std::unexpected(large.error());
                background = std::move(*large);
            }
            const auto& box = input.html_labels ? *label : background;
            edge_sizes.push_back({box.width, box.height});
            measured.edges.push_back({.width = box.width, .height = box.height,
                .lines = label->lines});
            edge_backgrounds.push_back(std::move(background));
            edge_labels.push_back(std::move(*label));
        }
        FlowchartLayout layout;
        if (!nodes.empty()) {
            FlowchartSvgTheme options;
            options.group_title_top = input.group_title_top;
            options.group_title_bottom = input.group_title_bottom;
            if (input.elk) {
                auto scene = layout_measured_elk_scene(graph, boxes, edge_sizes, measured, {});
                if (!scene) return std::unexpected(scene.error());
                layout = std::move(scene->layout);
            } else if (std::ranges::any_of(graph.nodes, &FlowchartNode::group)) {
                auto scene = layout_measured_flowchart(graph, boxes, edge_sizes, measured, options);
                if (!scene) return std::unexpected(scene.error());
                layout = std::move(scene->layout);
            } else {
                auto result = layout_flowchart(graph, boxes, edge_sizes);
                if (!result) return std::unexpected(result.error());
                layout = std::move(*result);
            }
        }
        if (!input.elk) {
            for (auto& node : layout.nodes) {
                node.x += dagre_margin;
                node.y += dagre_margin;
            }
            for (auto& edge : layout.edges) {
                for (auto& p : edge.points) {
                    p.x += dagre_margin;
                    p.y += dagre_margin;
                }
                if (edge.label) {
                    edge.label->x += dagre_margin;
                    edge.label->y += dagre_margin;
                }
            }
        }

        const double padding = input.diagram_padding;
        double left = std::numeric_limits<double>::infinity(), top = left, right = -left, bottom = -left;
        const auto include_box = [&](double x, double y, double width, double height) {
            left = std::min(left, x - width / 2);
            right = std::max(right, x + width / 2);
            top = std::min(top, y - height / 2);
            bottom = std::max(bottom, y + height / 2);
        };
        const auto include_drawing = [&](const RoughBounds& bounds, double x = 0, double y = 0) {
            include_box(x + (bounds.left + bounds.right) / 2,
                y + (bounds.top + bounds.bottom) / 2, bounds.width(), bounds.height());
        };
        for (std::size_t i = 0; i < layout.nodes.size(); ++i) {
            const auto& box = layout.nodes[i];
            const auto& node = *nodes[i];
            double width = box.width, height = box.height;
            if (rough_nodes[i].bounds) {
                include_drawing(*rough_nodes[i].bounds, box.x, box.y);
                continue;
            }
            if (node.kind == UsecaseNodeKind::boundary) {
                if (input.hand_drawn) continue;
                width = std::max(width, labels[i].width + 20);
                if (node.shape == UsecaseShape::package) height = std::max(height, labels[i].height + 50);
            }
            include_box(box.x, box.y, width, height);
            if (!input.html_labels && node.kind == UsecaseNodeKind::usecase && !node.business)
                include_box(box.x + labels[i].glyph_width / 2, box.y, labels[i].glyph_width, labels[i].height);
        }
        for (std::size_t i = 0; i < layout.edges.size(); ++i) {
            const auto& edge = layout.edges[i];
            if (!input.hand_drawn)
                for (auto p : edge.points) include_box(p.x, p.y, 0, 0);
            if (edge.label) {
                const auto& label = edge_labels[i];
                if (input.html_labels) include_box(edge.label->x, edge.label->y, label.width, label.height);
                else {
                    const auto& background = edge_backgrounds[i];
                    const auto top = edge.label->y - label.height / 2 + label.baseline - label.size +
                        background.size - background.baseline - 2;
                    include_box(edge.label->x, top + background.height / 2, background.width, background.height);
                }
            }
        }
        std::string svg;
        if (!input.document.acc_title.empty()) svg += "<title>" + esc(input.document.acc_title) + "</title>";
        if (!input.document.acc_description.empty()) svg += "<desc>" + esc(input.document.acc_description) + "</desc>";
        svg += marker_definitions(theme, input.neo);
        svg += neo_look_definitions(neo_theme, shadow);
        svg += "<g class=\"clusters\">";
        std::size_t boundary_slot = 0;
        for (std::size_t i = 0; i < layout.nodes.size(); ++i) {
            if (nodes[i]->kind != UsecaseNodeKind::boundary) continue;
            const auto bounds = paint_boundary(svg, layout.nodes[i], labels[i], *nodes[i], input, boundary_slot++,
                paint_style(input.document, nodes[i]->classes, nodes[i]->styles), shadow_attribute, rough_options);
            if (bounds) include_drawing(*bounds);
        }
        svg += "</g><g class=\"edgePaths\">";
        for (std::size_t i = 0; i < layout.edges.size(); ++i) {
            auto points = layout.edges[i].points;
            if (points.size() < 2) continue;
            const auto& edge = input.document.edges[i];
            const auto a = node_index.at(edge.from), b = node_index.at(edge.to);
            if (input.elk) {
                clip_elk_node_endpoints(points, layout.nodes[a], layout.nodes[b],
                    elliptical(*nodes[a]) ? u"ellipse" : u"rect", elliptical(*nodes[b]) ? u"ellipse" : u"rect");
            } else {
                const auto clip = [&](std::size_t index, FlowchartPoint toward) {
                    auto node = layout.nodes[index];
                    node.width = nominal_boxes[index].width;
                    node.height = nominal_boxes[index].height;
                    return ellipse_intersect(node, toward);
                };
                if (elliptical(*nodes[a])) points.front() = clip(a, points[1]);
                if (elliptical(*nodes[b])) points.back() = clip(b, points[points.size() - 2]);
            }
            if (points.size() < 2) continue;
            const auto markers = edge_markers(edge);
            if (markers.start == Marker::point) shorten(points.front(), points[1], 4);
            if (markers.end == Marker::point || markers.end == Marker::extension)
                shorten(points.back(), points[points.size() - 2], markers.end == Marker::point ? 4 : 17.25);
            const bool include = edge.kind == UsecaseRelationship::include;
            const bool extend = edge.kind == UsecaseRelationship::extend;
            const bool note = edge.kind == UsecaseRelationship::note;
            const bool dashed = include || extend || note;
            double length = 0;
            auto path = input.elk ? rounded_elk_path(points, 5, &length) : basis(points, &length);
            if (input.hand_drawn) {
                auto drawing = sketch_svg_path(path,
                    {.seed = input.hand_drawn_seed, .roughness = .3, .fill = RoughFill::none});
                if (drawing.bounds) include_drawing(*drawing.bounds);
                path = std::move(drawing.stroke_path);
            }
            svg += "<path data-et=\"edge\" data-usecase-id=\"" + attribute(edge.id) +
                "\" class=\"relationship\" data-look=\"" + (input.neo ? "neo" : input.hand_drawn ? "handDrawn" : "classic") +
                "\" fill=\"none\" stroke=\"" +
                (include ? theme.include_line : extend ? theme.extend_line : theme.line) +
                "\" d=\"" + path + "\"";
            if (input.neo && edge.animation.empty()) {
                const FlowchartEdge dash_edge{.stroke = dashed ? u"dotted" : u"normal",
                    .arrow_start = std::u16string(marker_name(markers.start)),
                    .arrow_end = std::u16string(marker_name(markers.end))};
                svg += " stroke-dasharray=\"" + neo_edge_dash(dash_edge, length) + "\" stroke-dashoffset=\"0\"";
            } else svg += dashed ? " stroke-dasharray=\"3\"" : " stroke-dasharray=\"0\"";
            svg += paint_style(input.document, edge.classes, edge.styles);
            if (markers.start != Marker::none)
                svg += " marker-start=\"url(#" + marker_id(markers.start, true, input.neo) + ")\"";
            if (markers.end != Marker::none)
                svg += " marker-end=\"url(#" + marker_id(markers.end, false, input.neo) + ")\"";
            svg += "/>";
        }
        svg += "</g><g class=\"edgeLabels\">";
        for (std::size_t i = 0; i < layout.edges.size(); ++i) {
            const auto& edge = layout.edges[i];
            if (edge.label) paint_label(svg, edge_labels[i], *edge.label,
                theme.text, "edgeLabel", theme.label_background, SvgLabelPosition::edge,
                input.html_labels ? nullptr : &edge_backgrounds[i]);
        }
        svg += "</g><g class=\"nodes\">";
        for (std::size_t i = 0; i < layout.nodes.size(); ++i) {
            const auto& box = layout.nodes[i];
            const auto& node = *nodes[i];
            if (node.kind == UsecaseNodeKind::boundary) continue;
            const bool actor = node.kind == UsecaseNodeKind::actor;
            const bool note = node.kind == UsecaseNodeKind::note;
            const auto& label = labels[i];
            const auto& stereotype = stereotypes[i];
            const auto style = paint_style(input.document, node.classes, node.styles);
            const std::string_view node_kind = actor ? "actor" : note ? "note" :
                node.kind == UsecaseNodeKind::json ? "json" : "usecase";
            svg += "<g class=\"" + std::string(input.hand_drawn ? "rough-node" : "node") +
                "\" data-usecase-kind=\"" + std::string(node_kind) +
                "\" data-usecase-id=\"" + attribute(nodes[i]->id) +
                "\" transform=\"translate(" + n(box.x) + "," + n(box.y) + ")\">";
            if (node.kind == UsecaseNodeKind::json) {
                paint_json(svg, label, json_tables[i], theme, style, shadow_attribute);
            } else if (actor) {
                const double figure_top = -box.height / 2 + actor_padding;
                svg += "<rect x=\"" + n(-box.width / 2) + "\" y=\"" + n(-box.height / 2) +
                    "\" width=\"" + n(box.width) + "\" height=\"" + n(box.height) +
                    "\" opacity=\"0\" aria-hidden=\"true\"/>";
                const auto paint = input.hand_drawn ? NodePaint{theme.background, theme.rough_border} :
                    palette_paint(theme, {theme.actor_fill, theme.actor_border}, i, input.rotate_colors);
                svg += paint_actor(node, paint, figure_top + actor_figure_height / 2, style,
                    shadow_attribute, input.neo ? theme.neo_actor_path_width : 0, rough_options);
                const double top = figure_top + actor_figure_height + actor_label_gap;
                paint_label(svg, stereotype, {0, top + stereotype.height / 2},
                    theme.actor_text, "usecase-stereotype", {}, SvgLabelPosition::centered_bounds);
                paint_label(svg, label, {0, top + (stereotype.height > 0 ? stereotype.height + 2 : 0) + label.height / 2},
                    theme.actor_text, "nodeLabel", {}, SvgLabelPosition::centered_bounds);
            } else {
                const double x = -box.width / 2, y = -box.height / 2;
                const auto colors = palette_paint(theme,
                    {note ? theme.note_fill : theme.node_fill, note ? theme.note_border : theme.node_border},
                    i, input.rotate_colors && !note && (!input.neo || elliptical(node)));
                const auto paint = shape_paint(colors.fill, colors.border, note ? 1 : 2) + style;
                if (input.hand_drawn) {
                    svg += paint_rough(rough_nodes[i],
                        note ? NodePaint{theme.note_fill, theme.note_border} :
                               NodePaint{theme.background, theme.rough_border}, style);
                }
                if (elliptical(node)) {
                    if (!input.hand_drawn)
                        svg += "<ellipse rx=\"" + n(-x) + "\" ry=\"" + n(-y) + "\"" + paint + "/>";
                    if (node.business) {
                        const double start = std::max(label.width, stereotype.width) / 2 + 2;
                        const double end = -x - 2;
                        const auto at = [&](double p) {
                            return -y * std::sqrt(std::max(0.0, 1 - p * p / (x * x)));
                        };
                        svg += "<path class=\"usecase-business-marker\" d=\"M" + n(start) + "," + n(at(start)) +
                            "L" + n(end) + "," + n(-at(end)) + "\"" +
                            shape_paint("none", colors.border, 2) + "/>";
                    }
                } else if (note && !input.hand_drawn) {
                    svg += "<path d=\"M" + n(x) + "," + n(y) + "H" + n(-x) + "V" +
                        n(-y) + "H" + n(x) + "Z\"" + paint + shadow_attribute + "/>";
                } else if (!input.hand_drawn) {
                    svg += "<rect x=\"" + n(x) + "\" y=\"" + n(y) + "\" width=\"" +
                        n(box.width) + "\" height=\"" + n(box.height) + "\"" + paint + shadow_attribute + "/>";
                }
                const double gap = stereotype.height > 0 && node.business ? 2 : 0;
                paint_label(svg, stereotype, {0, -(label.height + gap) / 2},
                    theme.text, "usecase-stereotype", {}, SvgLabelPosition::centered_bounds);
                const auto& color = input.html_labels ? (note ? theme.note_text : theme.text) :
                    note ? theme.note_fill : node.business ? theme.text : theme.svg_text;
                paint_label(svg, label, {0, (stereotype.height + gap) / 2}, color, "nodeLabel", {},
                    note || node.business ? SvgLabelPosition::centered_bounds : SvgLabelPosition::start);
            }
            svg += "</g>";
        }
        svg += "</g>";
        if (left > right) left = top = right = bottom = 0;
        if (!input.document.title.empty()) {
            const auto metrics = measure(input.document.title, theme.font_family, theme.diagram_font_size,
                400, {.width = SvgTextWidth::advance});
            if (!metrics) return std::unexpected(metrics.error());
            const double center = (left + right) / 2;
            include_box(center, metrics->height / 2 - metrics->baseline, metrics->width, metrics->height);
            svg += "<text class=\"usecaseDiagramTitleText\" text-anchor=\"middle\" x=\"" +
                n(center) + "\" y=\"0\" fill=\"" + theme.boundary_text + "\" font-family=\"" +
                attribute(theme.font_family) + "\" font-size=\"" + n(theme.diagram_font_size) +
                "\">" + esc(input.document.title) + "</text>";
        }
        UsecaseSvg result;
        result.width = right - left + 2 * padding;
        result.height = bottom - top + 2 * padding;
        result.svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" +
            n(left - padding) + " " + n(top - padding) + " " + n(result.width) + " " +
            n(result.height) + "\" aria-roledescription=\"usecase\"";
        if (input.use_max_width) result.svg += " width=\"100%\" style=\"max-width:" + n(result.width) + "px\"";
        else result.svg += " width=\"" + n(result.width) + "\" height=\"" + n(result.height) + "\"";
        result.svg += ">" + svg + "</svg>";
        return result;
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}

}
