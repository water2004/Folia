// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 flat layered-graph processors; see ../ELK-LICENSE.
export module folia.mermaid.elk_layering;
import std;
export import folia.mermaid.elk_cycle;

export namespace folia::mermaid {
struct ElkLayeringResult {
    std::vector<int> layers; // Original node order; normalized independently per component.
    std::vector<std::size_t> reversed_edges;
    std::vector<std::vector<std::size_t>> layer_nodes; // Original per-layer insertion order.
};
// Ordinary ports only, without compound nodes or explicit layer constraints.
// Uses original DEPTH_FIRST cycle removal and NETWORK_SIMPLEX layering with
// default thoroughness 7. No placement, routing or public renderer is implied.
// Omitted shortness priorities use ELK's default 0 (effective weight 1).
[[nodiscard]] std::expected<ElkLayeringResult, std::string> elk_layer_flat_graph(
    const ElkCycleGraph& graph, std::span<const int> shortness = {});
}
