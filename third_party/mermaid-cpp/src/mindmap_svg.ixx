// Mermaid 12.2.0 Mindmap measured SVG projection. MIT; see ../LICENSE.
export module folia.mermaid.mindmap_svg;
export import folia.mermaid.mindmap_shapes;
export import folia.mermaid.cose_tree;
export import folia.mermaid.flowchart_svg;
import std;

export namespace folia::mermaid {
struct MindmapRenderInput {
    MindmapInput document;
    // createText uses the same typography/wrapping contract as Flowchart.
    FlowchartSvgTheme theme;
    std::u16string root_fill,root_text;
    std::vector<std::u16string> fills,texts,underlines;
};
struct MindmapMeasurements {std::vector<FlowchartTextBox> nodes;};
struct MindmapSvg {std::string svg;double width=0,height=0;};
std::expected<MindmapRenderInput,std::string> prepare_mindmap_render(
    std::u16string_view,const ConfigSession&,FrontendLimits limits={});
std::expected<MindmapSvg,std::string> render_mindmap_svg(
    const MindmapRenderInput&,const MindmapMeasurements&);
}
