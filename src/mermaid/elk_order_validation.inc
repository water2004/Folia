// SPDX-License-Identifier: EPL-2.0
// Shared prepared-graph boundary for the original crossing minimizers.
std::expected<void, std::string> supported(const ElkLayeredGraph& g,const ElkNorthSouthPreparation* north_south=nullptr) {
    for (const auto& p : g.ports) {
        if (north_south&&(p.side==ElkPortSide::north||p.side==ElkPortSide::south)&&p.incoming.empty()&&p.outgoing.empty())continue;
        if (p.side != ElkPortSide::east && p.side != ElkPortSide::west)
            return std::unexpected("ELK flat ordering requires east/west port preprocessing");
        if (p.incoming.size() + p.outgoing.size() > 1)
            return std::unexpected("ELK shared-port hyperedge crossing count is not implemented");
    }
    for (const auto& e : g.edges) {
        const auto& s = g.ports[e.source_port]; const auto& t = g.ports[e.target_port];
        const auto left = g.nodes[s.node].layer, right = g.nodes[t.node].layer;
        const bool forward = right == left + 1 && s.side == ElkPortSide::east && t.side == ElkPortSide::west;
        const bool in_layer = left == right && s.node != t.node && s.side == t.side;
        if (!forward && !in_layer)
            return std::unexpected("ELK ordering requires preprocessed adjacent or same-side in-layer edges");
    }
    return {};
}
