// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_north_south;
import std;
export import folia.mermaid.elk_routing;

export namespace folia::mermaid {
struct ElkNorthSouthDummy {
    std::size_t node=0,origin_node=0,origin_port=0,proxy_port=0;
    ElkPortSide side=ElkPortSide::undefined;
};
struct ElkNorthSouthPreparation {
    std::vector<ElkNorthSouthDummy> dummies;
    std::vector<std::pair<std::size_t,std::size_t>> successors;
    std::vector<std::vector<std::size_t>> barycenter_associates;
    std::vector<std::optional<std::size_t>> layout_units;
};
// Original fixed-side normal-node preprocessor, for the adapter's one-edge
// implicit ports. The graph is layered, with final N/S group-port sides.
// Self loops and shared ports belong to separate processors.
[[nodiscard]] std::expected<ElkNorthSouthPreparation,std::string> elk_prepare_north_south_ports(ElkLayeredGraph& graph);
// After routing, before original-edge/compound restoration. Adds the original
// orthogonal turn at each proxy and restores original endpoint port identities.
void elk_restore_north_south_ports(ElkLayeredGraph& graph,
    const ElkNorthSouthPreparation& prepared,ElkOrthogonalRoutes& routing,
    std::span<const double> node_y);
}
