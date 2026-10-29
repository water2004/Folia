// SPDX-License-Identifier: EPL-2.0
// Port of ELK 0.9.3 DepthFirstCycleBreaker; see ELK-LICENSE and source-lock.json.
export module folia.mermaid.elk_cycle;
import std;

export namespace folia::mermaid {
// Indices retain the layered graph's node, port and edge insertion order.
// Ports belonging to a node are visited in their order in this array.
struct ElkCyclePort { std::size_t node = 0; };
struct ElkCycleEdge { std::size_t source_port = 0, target_port = 0; };
struct ElkCycleGraph {
    std::size_t node_count = 0;
    std::vector<ElkCyclePort> ports;
    std::vector<ElkCycleEdge> edges;
};

// Returns edge indices in the original reversal order, without mutating input.
// This is the selection phase only: port adaptation, label reversal and the
// post-routing edge restorer belong to the later layered-graph integration.
[[nodiscard]] std::expected<std::vector<std::size_t>, std::string>
elk_depth_first_feedback_edges(const ElkCycleGraph& graph);
}
