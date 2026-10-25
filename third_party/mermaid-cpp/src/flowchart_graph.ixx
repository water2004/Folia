// Mermaid 12.2.0 Dagre graph preparation. MIT; see ../LICENSE.
export module folia.mermaid.flowchart_graph;
import std;
export import folia.mermaid.flowchart;
export import folia.mermaid.dagre_rank;

export namespace folia::mermaid {
struct FlowchartPreparedNode {
    FlowchartNode data;
    // Index into the source nodes; synthetic self-loop nodes have no index.
    std::optional<std::size_t> source_index;
    // Index into this graph's subgraphs, when rendered as an extracted cluster.
    std::optional<std::size_t> subgraph;
};
struct FlowchartPreparedEdge {
    FlowchartEdge data;
    std::u16string name, from_cluster, to_cluster;
    std::size_t source_index = 0;
    // Three layout segments belong to one original logical self edge.
    // Only segment 1 carries the label. -1 is an ordinary edge.
    int loop_segment = -1;
};
struct FlowchartPreparedGraph {
    std::u16string direction = u"TB";
    double node_spacing = 50, rank_spacing = 50;
    // Original Mermaid extraction settings, before recursive measurement's
    // parent rank-spacing + 25 adjustment. Dagre itself remains unchanged.
    double margin_x = 8, margin_y = 8;
    std::vector<FlowchartPreparedNode> nodes;
    std::vector<FlowchartPreparedEdge> edges;
    std::vector<FlowchartPreparedGraph> subgraphs;
};
struct FlowchartGraphPreparation {
    FlowchartInput source;
    FlowchartPreparedGraph graph;
};
// Owned prepareLayoutForDagre/adjustClustersAndEdges/extractor boundary:
// split self-loop layout segments, select cluster endpoint anchors and extract
// independently directed clusters. Source ids/labels/styles stay available for
// measurement and logical-edge painting. No DOM, layout or text measurement.
// Invalid graphs return an error without modifying the caller's source.
std::expected<FlowchartGraphPreparation, std::string> prepare_flowchart_graph(
    const FlowchartInput& source);
// Unified Dagre getSelfLoopPoints/getSelfLoopLabelPosition. The hint is the
// mean position of the two prepared dummy nodes; caller clips shape endpoints.
FlowchartPlacedEdge route_dagre_self_loop(const FlowchartPlacedNode& node,
    FlowchartPoint hint,std::u16string_view direction,FlowchartEdgeLabelSize label,bool has_label);
}
