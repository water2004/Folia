// Plain-label Flowchart SVG adapter, Mermaid 12.2.0. MIT, ../LICENSE.
export module folia.mermaid.flowchart_svg;
import std;
export import folia.mermaid.dagre_rank;

export namespace folia::mermaid {
struct FlowchartSvgTheme {
    std::u16string font_family=u"Arial";
    double font_size=16,minimum_label_width=120,wrapping_width=120,padding=8,radius=5;
    bool html_label_spacing=true;
    std::string fill="#ECECFF",border="#9370DB",text="#333333",line="#333333",label_background="#e8e8e8";
};
struct FlowchartTextBox {
    // Real measured width, line-box height and baseline from its top. The
    // plain HTML-label mode adds the original CSS 1.5 line height separately.
    double width=0,height=0,baseline=0;
};
struct FlowchartMeasurements {
    std::vector<FlowchartTextBox> nodes,edges;
};
struct FlowchartSvg {std::string svg;double width=0,height=0;};
struct FlowchartRenderInput {FlowchartInput document;FlowchartSvgTheme theme;};
// Initial measured SVG stage: ungrouped square/rounded/diamond/circle nodes,
// plain single-line labels, basis/linear paths, plain/point arrow ends and
// normal/thick/dotted/invisible edges. Unsupported adapters fail explicitly.
// The Dagre layout API separately supports compound graphs and self loops.
std::expected<FlowchartSvg,std::string> render_flowchart_svg(
    const FlowchartRenderInput&,const FlowchartMeasurements&);
}
