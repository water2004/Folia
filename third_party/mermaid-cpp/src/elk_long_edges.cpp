// SPDX-License-Identifier: EPL-2.0
// Port of ELK 0.9.3 LongEdgeSplitter; see ../ELK-LICENSE.
module folia.mermaid.elk_long_edges;
import std;

namespace folia::mermaid {
std::expected<ElkLayeredGraph, std::string> make_elk_layered_graph(
    const ElkCycleGraph& source, const ElkLayeringResult& layering) {
    if (layering.layers.size() != source.node_count)
        return std::unexpected("ELK layer assignment does not match nodes");
    ElkLayeredGraph graph;
    graph.layers = layering.layer_nodes;
    graph.nodes.resize(source.node_count);
    std::vector<bool> assigned(source.node_count, false);
    for (std::size_t layer = 0; layer < graph.layers.size(); ++layer) for (const auto node : graph.layers[layer]) {
        if (node >= source.node_count || assigned[node] || layering.layers[node] != static_cast<int>(layer))
            return std::unexpected("ELK layer order does not match assignment");
        assigned[node] = true;
        graph.nodes[node].original_node = node;
        graph.nodes[node].layer = layer;
    }
    if (std::ranges::find(assigned, false) != assigned.end())
        return std::unexpected("ELK node is missing from layer order");
    for (const auto& port : source.ports) {
        if (port.node >= source.node_count) return std::unexpected("ELK port has no node");
        graph.nodes[port.node].ports.push_back(graph.ports.size());
        graph.ports.push_back({.node=port.node});
    }
    for (const auto& edge : source.edges) {
        if (edge.source_port >= graph.ports.size() || edge.target_port >= graph.ports.size())
            return std::unexpected("ELK edge has no port");
        const auto index = graph.edges.size();
        graph.edges.push_back({.original_edge=index,.source_port=edge.source_port,.target_port=edge.target_port});
        graph.ports[edge.source_port].outgoing.push_back(index);
        graph.ports[edge.target_port].incoming.push_back(index);
    }
    for (const auto index : layering.reversed_edges) {
        if (index >= graph.edges.size()) return std::unexpected("ELK reversed edge does not exist");
        auto& edge = graph.edges[index];
        std::erase(graph.ports[edge.source_port].outgoing, index);
        std::erase(graph.ports[edge.target_port].incoming, index);
        std::swap(edge.source_port, edge.target_port); edge.reversed = !edge.reversed;
        graph.ports[edge.source_port].outgoing.push_back(index);
        graph.ports[edge.target_port].incoming.push_back(index);
    }
    return graph;
}

void elk_split_long_edges(ElkLayeredGraph& graph) {
    // Original processor visits each current layer, appending dummies to the
    // next layer. A later iteration splits the new segment again if necessary.
    for (std::size_t layer = 0; layer + 1 < graph.layers.size(); ++layer) {
        if (graph.layers.size() <= 2) break;
        for (const auto node : graph.layers[layer]) {
            // A split can reallocate nodes/ports/edges; retain indices, not refs.
            const auto ports = graph.nodes[node].ports;
            for (const auto port : ports) {
                const auto outgoing = graph.ports[port].outgoing;
                for (const auto index : outgoing) {
                    const auto target = graph.edges[index].target_port;
                    const auto target_layer = graph.nodes[graph.ports[target].node].layer;
                    if (target_layer == layer || target_layer == layer + 1) continue;
                    const auto dummy = graph.nodes.size(), input = graph.ports.size(), output = input + 1;
                    const auto next_edge = graph.edges.size();
                    const auto thickness = std::max(0.0, graph.edges[index].thickness);
                    graph.edges[index].thickness = thickness;
                    const auto source_node = graph.ports[graph.edges[index].source_port].node;
                    const auto target_node = graph.ports[target].node;
                    const auto inherited = graph.nodes[source_node].kind != ElkLayerNodeKind::normal
                        ? std::optional{source_node} : graph.nodes[target_node].kind == ElkLayerNodeKind::label
                        ? std::optional{target_node} : std::nullopt;
                    const auto long_source = inherited ? graph.nodes[*inherited].long_edge_source
                        : std::optional{graph.edges[index].source_port};
                    const auto long_target = inherited ? graph.nodes[*inherited].long_edge_target : std::optional{target};
                    const auto has_label = inherited && (graph.nodes[*inherited].kind == ElkLayerNodeKind::label
                        || graph.nodes[*inherited].long_edge_has_label);
                    graph.nodes.push_back({.kind=ElkLayerNodeKind::long_edge,.layer=layer+1,.ports={input,output},
                        .height=thickness,.fixed_port_positions=true,.origin_edge=index,
                        .long_edge_source=long_source,.long_edge_target=long_target,.long_edge_has_label=has_label});
                    graph.layers[layer+1].push_back(dummy);
                    const auto port_y = std::floor(thickness / 2);
                    graph.ports.push_back({.node=dummy,.side=ElkPortSide::west,.y=port_y,.incoming={index}});
                    graph.ports.push_back({.node=dummy,.side=ElkPortSide::east,.y=port_y,.outgoing={next_edge}});
                    std::erase(graph.ports[target].incoming, index);
                    graph.edges[index].target_port = input;
                    auto segment = graph.edges[index];
                    segment.source_port = output; segment.target_port = target;
                    graph.edges.push_back(segment);
                    graph.ports[target].incoming.push_back(next_edge);
                }
            }
        }
    }
}
}
