// Mermaid 12.2.0 pieRenderer.ts / pieStyles.ts. MIT; see ../LICENSE.
export module folia.mermaid.pie_svg;
export import folia.mermaid.pie_layout;
import std;

export namespace folia::mermaid {
// Fully resolved theme input. Shared Mermaid theme/config resolution is a
// separate boundary. Paints are literal CSS colors, never URLs or CSS rules.
struct PieSvgTheme {
    std::array<std::string, 12> colors;
    std::string stroke_color, outer_stroke_color;
    std::string title_color, section_color, legend_color;
    std::u16string font_family;
    double stroke_width = 2, outer_stroke_width = 2, opacity = 0.7;
    double title_size = 25, section_size = 17, legend_size = 17;
};
struct PieSvgConfig {
    PieLayoutConfig layout;
    bool use_max_width = true;
    // "hover" selects the official hover rule; otherwise match a section label.
    std::u16string highlight_slice;
};
struct PieSvg { std::string svg; double width{}, height{}; };

// Rebuilds geometry from document + measured widths; never trusts cached paths.
// UTF-16/XML and finite dimensions are checked before returning any SVG.
// Measurement must use the same resolved family and title/legend sizes as theme.
// This family-level renderer does not perform MermaidAPI preprocessing or its
// accessibility wrapper. Text goes through XML escaping, never HTML insertion.
std::expected<PieSvg, PieError> render_pie_svg(PieDocument const&,
    PieMeasurements const&, PieSvgTheme const&, PieSvgConfig const& = {});
}
