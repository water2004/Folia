// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 flat layer-sweep ordering; see ../ELK-LICENSE.
export module folia.mermaid.elk_order;
import std;
export import folia.mermaid.elk_long_edges;
export import folia.mermaid.elk_random;
export import folia.mermaid.elk_hierarchy_order;

export namespace folia::mermaid {
// Valid layered graph, after long-edge splitting and port-side assignment.
// Normal ports are FIXED_SIDE; fixed_port_positions represents FIXED_POS.
// This boundary accepts east/west ports with at most one edge per port and
// edges to the next layer. Other cases require their original preprocessors.
// Unsupported input is rejected before changing node or port order.
[[nodiscard]] std::expected<void, std::string> elk_sort_flat_ports(ElkLayeredGraph& graph);
// Runs the original seeded BARYCENTER controller, including port distribution,
// alternating sweeps and best-order restoration. Call after port sorting.
[[nodiscard]] std::expected<std::size_t, std::string> elk_order_flat_graph(
    ElkLayeredGraph& graph, std::int64_t seed = 1, int thoroughness = 7);
[[nodiscard]] std::expected<std::size_t, std::string> elk_order_flat_graph(
    ElkLayeredGraph& graph, ElkRandom& random, int thoroughness = 7);
struct ElkHierarchyOrderGraph {
    ElkLayeredGraph* graph = nullptr;
    ElkRandom* random = nullptr;
    std::optional<std::size_t> parent_graph;
    std::size_t parent_node = 0;
    std::vector<ElkHierarchyPortLink> ports;
};
// Root is index zero. Every nested graph has its own current phase RNG.
// Graphs already have separate external layers, split long edges and sorted
// E/W ports. Parent/child links retain original external-port identities.
[[nodiscard]] std::expected<std::size_t, std::string> elk_order_hierarchy(
    std::span<ElkHierarchyOrderGraph> graphs, int thoroughness = 7);
}
