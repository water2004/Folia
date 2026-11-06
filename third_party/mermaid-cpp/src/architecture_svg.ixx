// Original Architecture SVG adapter. Mermaid MIT; ../LICENSE.
export module folia.mermaid.architecture_svg;
export import folia.mermaid.fcose_architecture;
export import folia.mermaid.flowchart_svg;
import std;
export namespace folia::mermaid {
struct ArchitectureRenderInput {
    ArchitectureDocument document;FlowchartSvgTheme theme;
    std::u16string edge_color,arrow_color,group_color,text_color;
    double edge_width=3,group_width=2;
};
struct ArchitectureMeasurements {
    std::vector<FlowchartTextBox> services,icon_text;
    ArchitectureCyMeasurements cy_labels;
    double svg_ascent=0,svg_middle=0;
    std::function<std::expected<FlowchartTextBox,std::string>(std::u16string_view,double)> measure_label;
};
struct ArchitectureSvg {std::string svg;double width=0,height=0;};
std::expected<ArchitectureRenderInput,std::string> prepare_architecture_render(std::u16string_view,const ConfigSession&,FrontendLimits={});
std::expected<ArchitectureSvg,std::string> render_architecture_svg(const ArchitectureRenderInput&,const ArchitectureMeasurements&);
}
