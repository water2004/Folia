// Deterministic geometric equivalent of Mermaid 12.2.0 ER RoughJS rectangles.
// RoughJS emits collinear cubic strokes with run-to-run control-point jitter
// when roughness is zero and seed is omitted. Preserve their path geometry.
export module folia.mermaid.rough_rectangle;
import std;

export namespace folia::mermaid {
struct RoughPoint {
    double x{};
    double y{};
};

struct RoughRectanglePaths {
    std::string fill_path;
    std::string stroke_path;
};

// Mermaid ER uses RoughJS rectangle(..., {roughness: 0, fillStyle: 'solid'}).
// The polygon overload preserves the source vertex order and emits the same
// solid fill path plus two deterministic, collinear cubic strokes per edge.
RoughRectanglePaths rough_polygon_paths(std::span<RoughPoint const> points);

// Rectangle convenience wrapper around the polygon primitive.
RoughRectanglePaths rough_rectangle_paths(double x, double y,
    double width, double height);
}
