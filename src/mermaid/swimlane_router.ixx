// Mermaid 12.2.0 Swimlane orthogonal router. MIT; see THIRD_PARTY_NOTICES.md.
export module folia.mermaid.swimlane_router;
import std;
export import folia.mermaid.dagre_rank;

export namespace folia::mermaid {

struct SwimlaneRoutingNode {
    FlowchartPlacedNode box;
    std::optional<std::size_t> parent;
    bool group = false, edge_label = false, diamond = false;
    double padding = 0;
    std::optional<FlowchartPlacedNode> title_box;
    std::u16string direction;
};

struct SwimlaneRoutingEdge {
    std::size_t start = 0, end = 0;
    std::optional<std::size_t> label_node;
    FlowchartPlacedEdge placed;
    std::u16string id;
};

// Receives the canonical TB layout, before the optional direction transform.
// Layout-only edges are omitted; edge-label nodes participate in layout only.
void route_swimlane_edges(std::span<const SwimlaneRoutingNode> nodes,
                         std::span<SwimlaneRoutingEdge> edges,
                         std::u16string_view direction);

// Apply Mermaid's route cleanup and materialize endpoints and label geometry
// after the direction transform.
void finish_swimlane_routes(std::span<SwimlaneRoutingNode> nodes,
                            std::span<SwimlaneRoutingEdge> edges);

void adjust_swimlane_title_bands(std::span<SwimlaneRoutingNode> nodes,
                                 std::span<const SwimlaneRoutingEdge> edges,
                                 std::u16string_view direction);

}
