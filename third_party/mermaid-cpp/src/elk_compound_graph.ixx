// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_compound_graph;
import std;
export import folia.mermaid.elk_compound_ports;

export namespace folia::mermaid {
struct ElkCompoundInputNode {
    std::optional<std::size_t> parent;
    bool group=false;
    ElkLayoutDirection direction=ElkLayoutDirection::right;
};
struct ElkCompoundInput {
    std::vector<ElkCompoundInputNode> nodes;
    std::vector<ElkFlatEdge> edges;
    ElkLayoutDirection direction=ElkLayoutDirection::right;
    double edge_spacing=10;
};
struct ElkCompoundNode {
    std::size_t graph=0;
    std::optional<std::size_t> original,nested_graph,boundary_port;
    std::optional<ElkCompoundPortDummy> boundary;
    std::vector<std::size_t> ports;
};
struct ElkCompoundPort { std::size_t node=0; ElkPortSide side=ElkPortSide::undefined; };
struct ElkCompoundSegment {
    std::size_t original_edge=0,graph=0,source_port=0,target_port=0;
};
struct ElkCompoundSubgraph {
    std::optional<std::size_t> parent_node;
    ElkLayoutDirection direction=ElkLayoutDirection::right;
    std::vector<std::size_t> nodes,segments;
};
struct ElkCompoundEdgeChain {
    std::vector<std::size_t> segments; // Original source-to-target traversal.
    std::size_t center_segment=0,tail_segment=0,head_segment=0;
};
struct ElkCompoundGraph {
    std::vector<ElkCompoundNode> nodes;
    std::vector<ElkCompoundPort> ports;
    std::vector<ElkCompoundSegment> segments;
    std::vector<ElkCompoundSubgraph> graphs;
    std::vector<ElkCompoundEdgeChain> edges;
};
// Original CompoundGraphPreprocessor for implicit one-edge ports. Creates
// genuine boundary dummies and preserves graph/node/port insertion order.
// Chain segment ownership determines CENTER/TAIL/HEAD label distribution.
[[nodiscard]] std::expected<ElkCompoundGraph,std::string> elk_prepare_compound_graph(
    const ElkCompoundInput& input);
}
