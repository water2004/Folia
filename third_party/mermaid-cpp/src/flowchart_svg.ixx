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
    double title_top_margin=25;
    std::string title_color="#333333";
    std::string cluster_fill="#ffffde",cluster_border="#aaaa33",cluster_text="#333333";
    bool html_label_spacing=true;
    std::string fill="#ECECFF",border="#9370DB",text="#333333",line="#333333",label_background="#e8e8e8";
};
struct FlowchartLabelFont {
    std::u16string family;
    double size=16;
    unsigned weight=400;
    bool italic=false;
};
// Resolve class/direct typography before native measurement. The platform
// resolves the installed family and retains it in the measured text box.
std::expected<FlowchartLabelFont,std::string> flowchart_label_font(
    const std::vector<std::u16string>& classes,const std::vector<std::u16string>& direct,
    const FlowchartSvgTheme&);
struct FlowchartTextBox {
    // Real measured display box and first baseline, in SVG user units.
    // Lines are the exact display strings measured by the platform adapter.
    double width=0,height=0,baseline=0;
    double line_step=0;
    bool svg_label=false;
    std::vector<std::u16string> lines;
    std::vector<FlowchartLabelLine> runs;
    std::optional<FlowchartLabelFont> font;
};
struct FlowchartMeasurements {
    std::vector<FlowchartTextBox> nodes,edges;
    // Plain diagram title measured at the original fixed 18px size. Its
    // baseline/height are native font metrics, independent of HTML labels.
    std::optional<FlowchartTextBox> title;
};
struct FlowchartSvg {std::string svg;double width=0,height=0;};
struct FlowchartRenderInput {FlowchartInput document;FlowchartSvgTheme theme;};
// Shared unified-Dagre container geometry for measured family-specific nodes.
struct FlowchartScene {
    FlowchartLayout layout;
    double min_x=0,min_y=0,max_x=0,max_y=0,diff=0;
};
std::expected<FlowchartScene,std::string> layout_measured_flowchart(
    const FlowchartInput&,std::span<const FlowchartNodeBox>,
    std::span<const FlowchartEdgeLabelSize>,const FlowchartMeasurements&,const FlowchartSvgTheme&);
// Measured SVG stage: recursive plain-title groups; square/rounded/diamond/circle,
// stadium/subroutine/cylinder, double-circle and classic polygon nodes;
// quoted/entity plain labels with explicit/measured wrapped lines,
// basis/linear paths, plain/point/circle/cross arrow ends and
// normal/thick/dotted/invisible edges and merged self loops. Class/direct fill,
// stroke, text color, line width and dashes retain the original precedence.
// Unsupported
// adapters fail explicitly. Group titles use the same real measurement boundary.
std::expected<FlowchartSvg,std::string> render_flowchart_svg(
    const FlowchartRenderInput&,const FlowchartMeasurements&);
}
