export module folia.mermaid.pie_geometry;
import std;

export namespace folia::mermaid {

struct PieGeometryConfig {
    double donut_hole = 0;
    double text_position = 0.75;
};

struct PieGeometryError {
    // Input value index, or values.size() for a configuration/aggregate error.
    std::size_t index{};
    std::string message;
};

struct PieSectorGeometry {
    double value{};
    double start_angle{};
    double end_angle{};
    std::string path;
    double label_x{};
    double label_y{};
};

// Mermaid Pie's D3 geometry only: pie().sort(null), radius 185, no padding or
// rounded corners, and the percentage label arc's centroid. Values retain
// input order; zero/negative values receive zero angle as in D3. Filtering
// sections below 1%, percentage text, colors and legend layout belong to the
// Mermaid renderer and are deliberately not performed here.
// donut_hole outside (0, 0.9] becomes zero, matching pieRenderer.ts. Non-finite
// input or arithmetic is rejected instead of emitting invalid SVG geometry.
// Paths use D3's default three decimal places; centroid coordinates use the
// platform's binary64 trigonometry and may differ from V8 by a few ulps.
std::expected<std::vector<PieSectorGeometry>, PieGeometryError> layout_pie_geometry(
    std::span<double const> values, PieGeometryConfig const& config = {});

}
