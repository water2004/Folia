// Original classic Mindmap shape geometry, driven by measured label boxes.
export module folia.mermaid.mindmap_shapes;
export import folia.mermaid.mindmap;
import std;

export namespace folia::mermaid {
struct MindmapLabelSize {double width=0,height=0;};
struct MindmapShape {
    std::string path;
    double translate_x=0,translate_y=0;
    // Entire node bounds in local coordinates, including any asymmetric arcs.
    double min_x=0,min_y=0,width=0,height=0;
};
std::expected<MindmapShape,std::string> measure_mindmap_shape(
    const MindmapNode&,MindmapLabelSize,double rounded_radius=5);
}
