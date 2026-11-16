// Text shaping belongs to the host; diagram measurement and layout are portable.
export module folia.mermaid.text_measurer;
import std;

export namespace folia::mermaid {
struct MermaidFont {
    // One resolved installed family, shared by measurement and SVG emission.
    std::u16string family;
    float size = 17;
    unsigned weight = 400;
    bool italic = false;
    bool pair_kerning = false;
};
struct MermaidTextMetrics {
    double advance{};
    double ink_width{};
    // Union of the advance interval with native glyph ink bounds. This is not
    // Chromium's raster-hinted SVG box; provider comparisons record that gap.
    double bounds_width{};
    double line_height{};
    double baseline{};
};
struct MermaidTextStyle {
    std::size_t offset = 0, length = 0;
    unsigned weight = 400;
    bool italic = false;
};
struct MermaidSvgFontMetrics { double height{}, baseline{}, middle_offset{}; };
enum class MermaidFontContext { document, svg_label };

// All values are actual shaped font measurements in CSS pixels at 96 DPI.
// A provider must report an error if it cannot resolve or measure a font.
// In particular, callers must never substitute character-count estimates.
class TextMeasurer {
public:
    virtual ~TextMeasurer() = default;
    virtual std::expected<std::u16string, std::string> resolve_family(
        std::u16string_view families,
        MermaidFontContext context = MermaidFontContext::document) const = 0;
    virtual std::expected<MermaidTextMetrics, std::string> measure(
        std::u16string_view text, const MermaidFont& font,
        bool preserve_spaces = false, std::span<const MermaidTextStyle> styles = {},
        bool svg_bounds = false) const = 0;
    virtual std::expected<std::vector<std::u16string>, std::string> wrap(
        std::u16string_view text, const MermaidFont& font, double width,
        std::span<const MermaidTextStyle> styles = {}, bool break_spaces = true) const = 0;
    virtual std::expected<MermaidSvgFontMetrics, std::string> svg_font_metrics(
        const MermaidFont& font) const = 0;
    std::expected<double, std::string> svg_font_height(const MermaidFont& font) const {
        auto metrics = svg_font_metrics(font);
        if (!metrics) return std::unexpected(metrics.error());
        return metrics->height;
    }
};
}
