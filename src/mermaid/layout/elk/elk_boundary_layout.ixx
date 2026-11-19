// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_boundary_layout;
import std;
export import folia.mermaid.elk_boundary_layers;
export import folia.mermaid.elk_compound_ports;

export namespace folia::mermaid {
struct ElkBoundaryLayoutResult {
    ElkLayeredGraph graph;
    std::vector<double> node_y;
    ElkOrthogonalRoutes routing;
    ElkCompoundContent content;
    std::vector<ElkRestoredRoute> routes;
    // Entries correspond to source node indices; ordinary nodes have no port.
    std::vector<std::optional<ElkRoutingPoint>> boundary_ports;
};
// Local child-graph phases with genuine external layers. Parent/child sweep
// coordination is separate; this entry accepts free implicit boundary order.
[[nodiscard]] std::expected<ElkBoundaryLayoutResult,std::string> elk_layout_boundary_graph(
    const ElkCycleGraph& source,std::span<const ElkExternalLayerRole> roles,
    std::span<const ElkMeasuredNode> measured,const ElkFlatLayoutOptions& options={},
    double border_offset=5);
}
