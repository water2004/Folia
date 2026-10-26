// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 flat layer-sweep ordering; see ../ELK-LICENSE.
export module folia.mermaid.elk_order;
import std;
export import folia.mermaid.elk_long_edges;

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
}
