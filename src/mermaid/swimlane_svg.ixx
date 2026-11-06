// Mermaid 12.2.0 Swimlane adapter. Swimlane reuses Flowchart's parser/DB and
// supplies a dedicated layout engine; see ../LICENSE.
export module folia.mermaid.swimlane_svg;
import std;
export import folia.mermaid.flowchart_svg;
import folia.mermaid.frontend;
import folia.mermaid.svg_text_measure;

export namespace folia::mermaid {
struct SwimlaneSvgTheme {
    std::u16string font_family = u"Arial";
    double font_size = 16;
    std::string background = "#ffffff", text = "#333333";
    std::string line = "#333333", node_fill = "#ECECFF";
    std::string node_border = "#9370DB", lane_fill = "#ffffde";
    std::string lane_border = "#aaaa33", lane_text = "#333333";
    std::string edge_label_background = "#e8e8e8";
    std::string arrowhead = "#333333";
    double stroke_width = 1;
};
struct SwimlaneRenderInput { FlowchartInput document; SwimlaneSvgTheme theme; };
struct SwimlaneSvg { std::string svg; double width = 0, height = 0; };
std::expected<SwimlaneRenderInput, std::string> prepare_swimlane_render(const PreparedDiagram&);
std::expected<SwimlaneSvg, std::string> render_swimlane_svg(
    const SwimlaneRenderInput&, const SvgFamilyTextMeasure&);
}
