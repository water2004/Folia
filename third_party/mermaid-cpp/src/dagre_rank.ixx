// Dagre 7.0.14 rank pipeline. MIT; see ../DAGRE-LICENSE.
export module folia.mermaid.dagre_rank;
import std;
export import folia.mermaid.flowchart;

export namespace folia::mermaid {
struct FlowchartEdgeLabelSize { double width = 0, height = 0; };
struct FlowchartNodeRank {
    std::u16string id;
    std::optional<int> rank, min_rank, max_rank;
};
struct FlowchartEdgeRank {
    bool reversed = false, self = false;
    std::optional<int> label_rank;
};
struct FlowchartRanks {
    // Original node/edge order; group nodes have a rank interval instead of rank.
    std::vector<FlowchartNodeRank> nodes;
    std::vector<FlowchartEdgeRank> edges;
    double rank_spacing = 25;
};
// The first Dagre layout stage: self-edge removal, DFS cycle reversal, compound
// nesting constraints, network simplex, label proxies and normalized ranks.
// Edge label boxes are supplied by the caller in input edge order, including
// zero boxes for absent labels. No guessed text widths or text measurement.
// This consumes an already adapted graph: links to nonempty groups need the
// separate Mermaid cluster-endpoint adapter and currently fail explicitly.
// Subgraph direction/extraction, node ordering, coordinates and SVG are later
// stages. The source graph is never mutated; failure returns no partial result.
std::expected<FlowchartRanks, std::string> rank_flowchart(
    const FlowchartInput& input, std::span<const FlowchartEdgeLabelSize> labels);
}
