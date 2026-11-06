// Mermaid 12.2.0 swimlanes layout. MIT; see THIRD_PARTY_NOTICES.md.
export module folia.mermaid.swimlane_layout;
import std;
export import folia.mermaid.dagre_rank;

export namespace folia::mermaid {

struct SwimlaneLayout {
    // Extents of the layout in the original, possibly signed coordinate space.
    double width = 0, height = 0;
    // Input order, with the implicit lane appended if one was synthesized.
    std::vector<FlowchartPlacedNode> nodes;
    std::vector<std::optional<FlowchartPlacedNode>> title_boxes;
    std::vector<FlowchartPlacedEdge> edges;
};

// Non-group nodes require measured shape boxes. Edge label boxes are supplied
// in source edge order and can be zero for edges without labels.
std::expected<SwimlaneLayout, std::string> layout_swimlanes(
    const FlowchartInput& input, std::span<const FlowchartNodeBox> nodes,
    std::span<const FlowchartEdgeLabelSize> labels);

}
