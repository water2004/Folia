// SPDX-License-Identifier: EPL-2.0
// Port of ELK 0.9.3 DepthFirstCycleBreaker; see ELK-LICENSE and source-lock.json.
module folia.mermaid.elk_cycle;
import std;

namespace folia::mermaid {
std::expected<std::vector<std::size_t>, std::string>
elk_depth_first_feedback_edges(const ElkCycleGraph& graph) {
    std::vector<std::vector<std::size_t>> node_ports(graph.node_count);
    std::vector<std::vector<std::size_t>> outgoing(graph.ports.size());
    std::vector<bool> has_incoming(graph.node_count, false);
    for (std::size_t p = 0; p < graph.ports.size(); ++p) {
        if (graph.ports[p].node >= graph.node_count)
            return std::unexpected("ELK cycle port has no node");
        node_ports[graph.ports[p].node].push_back(p);
    }
    for (std::size_t e = 0; e < graph.edges.size(); ++e) {
        const auto& edge = graph.edges[e];
        if (edge.source_port >= graph.ports.size() || edge.target_port >= graph.ports.size())
            return std::unexpected("ELK cycle edge has no port");
        outgoing[edge.source_port].push_back(e);
        has_incoming[graph.ports[edge.target_port].node] = true;
    }
    std::vector<bool> visited(graph.node_count, false), active(graph.node_count, false);
    std::vector<std::size_t> reversed;
    const auto dfs = [&](this auto&& visit, std::size_t node) -> void {
        if (visited[node]) return;
        visited[node] = active[node] = true;
        for (const auto port : node_ports[node]) {
            for (const auto edge : outgoing[port]) {
                const auto target = graph.ports[graph.edges[edge].target_port].node;
                if (target == node) continue;
                if (active[target]) reversed.push_back(edge);
                else visit(target);
            }
        }
        active[node] = false;
    };
    // Original ELK visits source nodes before the remaining node-model order.
    for (std::size_t node = 0; node < graph.node_count; ++node)
        if (!has_incoming[node]) dfs(node);
    for (std::size_t node = 0; node < graph.node_count; ++node)
        if (!visited[node]) dfs(node);
    return reversed;
}
}
