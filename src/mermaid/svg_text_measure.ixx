export module folia.mermaid.svg_text_measure;
import std;

export namespace folia::mermaid {
// Bounding box and baseline of a single SVG text line in user units.
struct SvgTextMetrics {
    double width{};
    double height{};
    double baseline{};
    double middle_offset{};
};

// Measures text in the current diagram's resolved font family.
using SvgTextMeasure = std::function<std::expected<SvgTextMetrics, std::string>(
    std::u16string_view text, double font_size, unsigned font_weight)>;
enum class SvgTextWidth { bounds, advance };
struct SvgTextRunStyle {
    std::size_t offset{}, length{};
    unsigned weight = 400;
    bool italic = false;
};
struct SvgFamilyTextOptions {
    SvgTextWidth width = SvgTextWidth::bounds;
    std::span<const SvgTextRunStyle> styles;
    bool pair_kerning = false;
};
using SvgFamilyTextMeasure = std::function<std::expected<SvgTextMetrics, std::string>(
    std::u16string_view text, std::u16string_view family, double font_size, unsigned font_weight,
    const SvgFamilyTextOptions&)>;
}
