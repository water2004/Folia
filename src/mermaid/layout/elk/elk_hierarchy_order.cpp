// SPDX-License-Identifier: EPL-2.0
// Port of the E/W branch of ELK 0.9.3 LayerSweepTypeDecider and
// LayerSweepCrossingMinimizer's parent/dummy ordering transfer.
module folia.mermaid.elk_hierarchy_order;
import std;

namespace folia::mermaid {
bool elk_hierarchy_uses_bottom_up(const ElkLayeredGraph& child,
    const ElkLayeredGraph& parent, std::size_t parent_node,
    std::span<const ElkHierarchyPortLink> links, bool parent_fixed_order) {
    const auto& group = parent.nodes[parent_node];
    if (parent_fixed_order || group.fixed_port_positions) return true;
    const auto side_count = [&](ElkPortSide side) {
        return std::ranges::count_if(group.ports, [&](auto p) { return parent.ports[p].side == side; });
    };
    if (side_count(ElkPortSide::east) < 2 && side_count(ElkPortSide::west) < 2) return true;
    struct Influence { double hierarchical = 0, random = 0, connected = 0; };
    std::vector<Influence> info(child.nodes.size());
    double random_paths = 0, hierarchical_paths = 0;
    for (const auto& layer : child.layers) for (const auto n : layer) {
        const auto& node = child.nodes[n];
        auto& current = info[n];
        if (node.kind == ElkLayerNodeKind::external_port) {
            current.hierarchical = 1;
            const auto link = std::ranges::find(links, n, &ElkHierarchyPortLink::dummy_node);
            if (parent.ports[link->parent_port].side == ElkPortSide::east)
                hierarchical_paths += current.connected;
        } else {
            const auto connected = [&](ElkPortSide side) {
                return std::ranges::any_of(node.ports, [&](auto p) {
                    const auto& port = child.ports[p];
                    return port.side == side && (!port.incoming.empty() || !port.outgoing.empty());
                });
            };
            if (!connected(ElkPortSide::west)) current.random = 1;
            else if (!connected(ElkPortSide::east)) random_paths += current.connected;
        }
        for (const auto p : node.ports) for (const auto e : child.ports[p].outgoing) {
            random_paths += current.random;
            hierarchical_paths += current.hierarchical;
            auto& target = info[child.ports[child.edges[e].target_port].node];
            target.hierarchical += current.hierarchical;
            target.random += current.random;
            target.connected += current.connected + 1;
        }
    }
    const auto total = random_paths + hierarchical_paths;
    // Original normalizes an empty path set to positive infinity.
    return total == 0 || (random_paths - hierarchical_paths) / total >= 0.1;
}

bool elk_hierarchy_enter(ElkLayeredGraph& child, const ElkLayeredGraph& parent,
    std::size_t parent_node, std::span<const ElkHierarchyPortLink> links, bool forward) {
    auto& layer = forward ? child.layers.front() : child.layers.back();
    if (child.nodes[layer.front()].kind != ElkLayerNodeKind::external_port) return false;
    const auto side = forward ? ElkPortSide::west : ElkPortSide::east;
    std::vector<std::size_t> sorted;
    for (const auto p : parent.nodes[parent_node].ports) if (parent.ports[p].side == side) {
        const auto link = std::ranges::find(links, p, &ElkHierarchyPortLink::parent_port);
        if (link != links.end()) sorted.push_back(link->dummy_node);
    }
    if (forward) std::ranges::reverse(sorted);
    layer = std::move(sorted);
    return true;
}

void elk_hierarchy_leave(const ElkLayeredGraph& child, ElkLayeredGraph& parent,
    std::size_t parent_node, std::span<const ElkHierarchyPortLink> links, bool forward) {
    const auto& layer = forward ? child.layers.back() : child.layers.front();
    if (child.nodes[forward ? layer.front() : layer.back()].kind != ElkLayerNodeKind::external_port) return;
    const auto side = forward ? ElkPortSide::east : ElkPortSide::west;
    std::size_t offset = 0;
    for (auto& p : parent.nodes[parent_node].ports) if (parent.ports[p].side == side) {
        if (std::ranges::find(links, p, &ElkHierarchyPortLink::parent_port) == links.end()) continue;
        const auto n = layer[forward ? offset : layer.size() - 1 - offset];
        p = std::ranges::find(links, n, &ElkHierarchyPortLink::dummy_node)->parent_port;
        ++offset;
    }
}
}
