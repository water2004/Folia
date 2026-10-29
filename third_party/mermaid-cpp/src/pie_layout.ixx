// Derived from Mermaid 12.2.0 pieRenderer.ts. MIT; see ../LICENSE.
export module folia.mermaid.pie_layout;
import std;
export import folia.mermaid.pie;
export import folia.mermaid.pie_geometry;

export namespace folia::mermaid {

enum class PieLegendPosition { right, left, top, bottom, center };
struct PieLayoutConfig {
    PieGeometryConfig geometry;
    PieLegendPosition legend_position = PieLegendPosition::right;
};

// Widths are SVG user units for the final legend strings and title, in the
// resolved rendering font. Supply one width per populated (deduplicated)
// section in insertion order. The host must measure text; there is no estimate
// or implicit fallback. Font measurement itself belongs to the platform layer.
struct PieMeasurements {
    std::vector<double> legend_widths;
    double title_width{};
};

struct PieLegendLayout {
    std::u16string text;
    double x{}, y{};
};
struct PieSliceLayout {
    std::size_t section_index{};
    PieSectorGeometry geometry;
    std::string percentage;
};
struct PieLayout {
    PieDocument document;
    double viewbox_x{}, width{}, height{};
    // Translation of the inner Pie group; the outer group is at (225,225).
    double pie_x{}, pie_y{};
    std::vector<PieLegendLayout> legends;
    std::vector<PieSliceLayout> slices;
};

// The exact display strings to measure after populate_pie().
std::expected<std::vector<std::u16string>, PieError> pie_legend_text(PieDocument const&);
std::expected<PieLayout, PieError> layout_pie(
    PieDocument const&, PieMeasurements const&, PieLayoutConfig const& = {});
}
