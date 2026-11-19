// Typed mathematical labels for Mermaid's HTML-label rendering path.
export module folia.mermaid.math_label;
import std;

export namespace folia::mermaid {
// A self-contained SVG root and its resolved CSS-pixel dimensions. Math
// providers own TeX parsing and glyph rendering; the compositor never edits SVG.
struct MathFragment {
    std::string svg;
    double width = 0, height = 0, baseline = 0;
};
using MathLabelRender = std::function<std::expected<MathFragment, std::string>(
    std::u16string_view tex, double font_size)>;
struct MathLabelTextMetrics {
    double width = 0, height = 0, baseline = 0;
};
using MathLabelTextMeasure = std::function<std::expected<MathLabelTextMetrics, std::string>(
    std::u16string_view text, unsigned weight, bool italic)>;
enum class MathLabelParsePolicy { create_text, sequence };
struct MathLabelOptions {
    double font_size = 16, line_height = 24;
    unsigned font_weight = 400;
    bool font_italic = false;
    MathLabelParsePolicy parse_policy = MathLabelParsePolicy::create_text;
};
struct MathLabelAtom {
    std::variant<std::u16string, MathFragment> content;
    double x = 0, y = 0, width = 0, height = 0, baseline = 0;
    unsigned weight = 400;
    bool italic = false;
};
struct MathLabelLine {
    std::vector<MathLabelAtom> atoms;
    double width = 0, height = 0, y = 0;
};
struct MathLabel {
    std::vector<MathLabelLine> lines;
    double width = 0, height = 0, font_size = 16;
};
// Family renderers can request a typed label without changing their ordinary
// font metric callback. nullopt preserves the existing plain-text path.
using MathLabelMeasure = std::function<std::expected<std::optional<MathLabel>, std::string>(
    std::u16string_view text, std::u16string_view font_family, double font_size,
    unsigned weight, bool italic)>;
// Matches Mermaid's non-greedy $$...$$ detector, including empty expressions.
// A formula never spans a real newline. A preceding backslash is not an escape.
bool has_math_label(std::u16string_view text);
std::expected<MathLabel, std::string> measure_math_label(
    std::u16string_view text, const MathLabelOptions&,
    const MathLabelTextMeasure&, const MathLabelRender&);
struct MathLabelSvgStyle {
    std::u16string_view font_family;
    std::string_view color;
    std::string_view css_class;
    // A cluster may apply its direct font style after fixing the layout box.
    std::optional<double> container_width;
};
// Paint at the top-left of the measured box. Lines and each line's atoms are
// centered independently, matching Mermaid's flex-row mathematical labels.
std::expected<std::string, std::string> render_math_label_svg(
    const MathLabel&, double left, double top, const MathLabelSvgStyle&);
}
