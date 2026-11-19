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
struct MindmapNodeBounds {double x=0,y=0,width=0,height=0;};
struct MindmapMeasurements {
    std::vector<FlowchartTextBox> nodes;
    // Optional externally measured whole-node SVG bounds. CoSE consumes these
    // exact boxes; otherwise the native shape geometry supplies them.
    std::vector<MindmapNodeBounds> node_bounds;
};
struct MindmapSvg {std::string svg;double width=0,height=0;};
std::expected<MindmapRenderInput,std::string> prepare_mindmap_render(
    std::u16string_view,const ConfigSession&,FrontendLimits limits={});
std::expected<MindmapSvg,std::string> render_mindmap_svg(
    const MindmapRenderInput&,const MindmapMeasurements&);
}
