// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 flat edge restoration; see ../ELK-LICENSE.
export module folia.mermaid.elk_restore;
import std;
export import folia.mermaid.elk_routing;

export namespace folia::mermaid {
struct ElkRestoredRoute {
    std::size_t original_edge = 0, source_node = 0, target_node = 0;
    std::size_t source_port = 0, target_port = 0;
    ElkRoutingPoint start, end;
    std::vector<ElkRoutingPoint> bends;
};
// Project original edge paths from the completed raw routing graph, applying
// LongEdgeJoiner and ReversedEdgeRestorer semantics without invalidating its
// stable node/port indices. No label dummies, collector ports or junctions.
// The original default unnecessaryBendpoints=false is retained.
[[nodiscard]] std::expected<std::vector<ElkRestoredRoute>,std::string> elk_restore_flat_routes(
    const ElkLayeredGraph& graph, const ElkOrthogonalRoutes& routes,
    std::span<const double> node_y);
}
