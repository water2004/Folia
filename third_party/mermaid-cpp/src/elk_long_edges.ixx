// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 layered graph and LongEdgeSplitter; see ../ELK-LICENSE.
export module folia.mermaid.elk_long_edges;
import std;
export import folia.mermaid.elk_layering;

export namespace folia::mermaid {
enum class ElkLayerNodeKind { normal, long_edge, label, external_port, north_south };
enum class ElkPortSide { undefined, north, east, south, west };
struct ElkLayerNode {
    ElkLayerNodeKind kind = ElkLayerNodeKind::normal;
    std::optional<std::size_t> original_node;
    std::size_t layer = 0;
    std::vector<std::size_t> ports;
    double width = 0, height = 0;
    double margin_top = 0, margin_bottom = 0;
    bool fixed_port_positions = false;
    std::optional<std::size_t> origin_edge, long_edge_source, long_edge_target;
    bool long_edge_has_label = false, long_edge_before_label = false;
    bool fixed_port_order = false;
};
struct ElkLayerPort {
    std::size_t node = 0;
    ElkPortSide side = ElkPortSide::undefined;
    double x = 0, y = 0;
    std::vector<std::size_t> incoming, outgoing;
};
struct ElkLayerEdge {
    std::size_t original_edge = 0, source_port = 0, target_port = 0;
    double thickness = 1;
    bool reversed = false;
};
// The working graph for the subsequent crossing/placement stages. Indices are
// stable identities; layers, node.ports and port adjacency carry ordering.
struct ElkLayeredGraph {
    std::vector<ElkLayerNode> nodes;
    std::vector<ElkLayerPort> ports;
    std::vector<ElkLayerEdge> edges;
    std::vector<std::vector<std::size_t>> layers;
};
[[nodiscard]] std::expected<ElkLayeredGraph, std::string> make_elk_layered_graph(
    const ElkCycleGraph& source, const ElkLayeringResult& layering);
// Input is the valid graph built above; callers attach measured node dimensions,
// port sides/positions and edge thickness before this processor. This initial
// flat boundary supports center-label dummies, but not collector ports.
void elk_split_long_edges(ElkLayeredGraph& graph);
}
