// Seeded polygon and normalized SVG paths from RoughJS 4.6.6.
export module folia.mermaid.rough_polygon;
import std;
export import folia.mermaid.rough_rectangle;

export namespace folia::mermaid {
enum class RoughFill { none, solid, hachure };
enum class RoughVerb { move, line, quadratic, cubic, close };
struct RoughCommand {
    RoughVerb verb;
    RoughPoint end{}, first_control{}, second_control{};
};
struct RoughPolygonOptions {
    std::uint32_t seed = 0;
    double roughness = 0.7;
    double hachure_gap = 1.5;
    double hachure_angle = -41;
    RoughFill fill = RoughFill::hachure;
    bool stroke = true;
};
struct RoughBounds {
    double left, top, right, bottom;
    double width() const { return right - left; }
    double height() const { return bottom - top; }
};
struct RoughPolygonResult {
    std::string fill_path, stroke_path;
    std::optional<RoughBounds> bounds;
};

RoughPolygonResult sketch_polygon(std::span<const RoughPoint> points, const RoughPolygonOptions& = {});
RoughPolygonResult sketch_rectangle(double x, double y, double width, double height,
    const RoughPolygonOptions& = {});
RoughPolygonResult sketch_ellipse(RoughPoint center, double width, double height,
    const RoughPolygonOptions& = {});
// Commands are absolute. Normalize other SVG path commands before calling.
RoughPolygonResult sketch_path(std::span<const RoughCommand> commands, const RoughPolygonOptions& = {});
// Accepts absolute SVG M/L/H/V/Q/C/A/Z and the usual coordinate separators.
RoughPolygonResult sketch_svg_path(std::string_view path, const RoughPolygonOptions& = {});
}
