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
    std::string note_fill, note_border, note_text, activation_fill, activation_border;
    std::string label_fill, label_border, label_text, loop_text;
    std::string rect_fill;
};
struct SequenceSvgConfig {
    double width = 150, height = 65, actor_margin = 50, box_margin = 10;
    double wrap_padding = 10, margin_x = 50, margin_y = 10, bottom_margin = 1;
    double note_margin = 10, activation_width = 10;
    double box_text_margin = 5, label_box_width = 50, label_box_height = 20;
    std::u16string note_align = u"center";
    bool right_angles = false;
};
struct SequenceRenderInput { SequenceInput document; SequenceSvgTheme theme; SequenceSvgConfig config; };
struct SequenceTextBox {
    // Original calculateTextDimensions rounds real SVG boxes to whole units.
    double width = 0, height = 0;
    std::u16string display;
};
struct SequenceMeasurements { std::vector<SequenceTextBox> actors, messages; };
// Region events have no participant endpoints and use bracketed display labels.
bool sequence_region_start(SequenceKind);
bool sequence_region_end(SequenceKind);
bool sequence_region_section(SequenceKind);
struct SequenceSvg { std::string svg; double width = 0, height = 0; };
// Classic participants/human actors, plain notes, ordered activations and
// single-line messages/self calls with solid/dotted/open/bidirectional arrows.
std::expected<SequenceSvg,std::string> render_sequence_svg(
    const SequenceRenderInput&, const SequenceMeasurements&);
}
