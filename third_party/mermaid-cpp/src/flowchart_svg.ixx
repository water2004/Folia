// Plain-label Flowchart SVG adapter, Mermaid 12.2.0. MIT, ../LICENSE.
export module folia.mermaid.flowchart_svg;
import std;
export import folia.mermaid.dagre_rank;
export import folia.mermaid.flowchart_label;

export namespace folia::mermaid {
struct FlowchartSvgTheme {
    std::u16string font_family=u"Arial";
    double font_size=16,minimum_label_width=120,wrapping_width=120,padding=8,radius=5;
    double group_title_top=0,group_title_bottom=0;
    std::string cluster_fill="#ffffde",cluster_border="#aaaa33";
    bool html_label_spacing=true;
    std::string fill="#ECECFF",border="#9370DB",text="#333333",line="#333333",label_background="#e8e8e8";
};
struct FlowchartTextBox {
    // Real measured display box and first baseline, in SVG user units.
    // Lines are the exact display strings measured by the platform adapter.
    double width=0,height=0,baseline=0;
    double line_step=0;
    std::vector<std::u16string> lines;
};
struct FlowchartMeasurements {
    std::vector<FlowchartTextBox> nodes,edges;
};
struct FlowchartSvg {std::string svg;double width=0,height=0;};
struct FlowchartRenderInput {FlowchartInput document;FlowchartSvgTheme theme;};
// Measured SVG stage: recursive plain-title groups; square/rounded/diamond/circle,
// stadium/subroutine/cylinder, double-circle and classic polygon nodes;
// quoted/entity plain labels with explicit/measured wrapped lines,
// basis/linear paths, plain/point/circle/cross arrow ends and
// normal/thick/dotted/invisible edges and merged self loops. Unsupported
// adapters fail explicitly. Group titles use the same real measurement boundary.
std::expected<FlowchartSvg,std::string> render_flowchart_svg(
    const FlowchartRenderInput&,const FlowchartMeasurements&);
}
