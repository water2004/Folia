// Deterministic ER rectangle geometry, matching RoughJS 4.6.6 roughness=0.
module folia.mermaid.rough_rectangle;
import std;
import folia.mermaid.number;

namespace folia::mermaid {
namespace {
std::string number(double value) {
    auto formatted = format_number(value);
    if (!formatted) throw std::runtime_error("Invalid RoughJS polygon coordinate");
    return *formatted;
}
void append_curve(std::string& path, RoughPoint start, RoughPoint end) {
    // RoughJS randomizes a collinear split in [0.2, 0.4] at roughness 0.
    // A fixed one-third split keeps its cubic stroke locus and command style
    // deterministic while retaining the pair of independent subpaths.
    const auto dx = end.x - start.x;
    const auto dy = end.y - start.y;
    const auto c1x = start.x + dx / 3.0;
    const auto c1y = start.y + dy / 3.0;
    const auto c2x = start.x + (2.0 * dx) / 3.0;
    const auto c2y = start.y + (2.0 * dy) / 3.0;
    path += "M" + number(start.x) + " " + number(start.y) +
        " C" + number(c1x) + " " + number(c1y) + ", " +
        number(c2x) + " " + number(c2y) + ", " +
        number(end.x) + " " + number(end.y);
}
}

RoughRectanglePaths rough_polygon_paths(std::span<RoughPoint const> points) {
    if (points.empty()) return {};
    std::string fill = "M" + number(points.front().x) + " " + number(points.front().y);
    for (std::size_t i = 1; i < points.size(); ++i) {
        fill += " L" + number(points[i].x) + " " + number(points[i].y);
    }
    std::string stroke;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto start = points[i];
        const auto end = points[(i + 1) % points.size()];
        if (!stroke.empty()) stroke.push_back(' ');
        append_curve(stroke, start, end);
        stroke.push_back(' ');
        append_curve(stroke, start, end);
    }
    return {std::move(fill), std::move(stroke)};
}

RoughRectanglePaths rough_rectangle_paths(double x, double y,
    double width, double height) {
    const std::array points{
        RoughPoint{x, y}, RoughPoint{x + width, y},
        RoughPoint{x + width, y + height}, RoughPoint{x, y + height}};
    return rough_polygon_paths(points);
}
}
