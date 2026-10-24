// Mermaid 12.2.0 sequenceRenderer.ts / svgDraw.js, MIT.
export module folia.mermaid.sequence_svg;
import std;
export import folia.mermaid.sequence;

export namespace folia::mermaid {
struct SequenceSvgTheme {
    std::u16string font_family = u"Arial";
    double font_size = 16;
    unsigned font_weight = 400;
    std::string actor_fill, actor_border, actor_text, actor_line, signal, signal_text;
};
struct SequenceSvgConfig {
    double width = 150, height = 65, actor_margin = 50, box_margin = 10;
    double wrap_padding = 10, margin_x = 50, margin_y = 10, bottom_margin = 1;
    bool right_angles = false;
};
struct SequenceRenderInput { SequenceInput document; SequenceSvgTheme theme; SequenceSvgConfig config; };
struct SequenceTextBox {
    // Original calculateTextDimensions rounds real SVG boxes to whole units.
    double width = 0, height = 0;
    std::u16string display;
};
struct SequenceMeasurements { std::vector<SequenceTextBox> actors, messages; };
struct SequenceSvg { std::string svg; double width = 0, height = 0; };
// First measured SVG boundary: classic rectangular participants, ordinary
// single-line messages/self calls and solid/dotted/open/bidirectional arrows.
std::expected<SequenceSvg,std::string> render_sequence_svg(
    const SequenceRenderInput&, const SequenceMeasurements&);
}
