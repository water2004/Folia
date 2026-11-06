// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 orthogonal routing; see ../ELK-LICENSE.
export module folia.mermaid.elk_routing;
import std;
export import folia.mermaid.elk_long_edges;
export import folia.mermaid.elk_random;

export namespace folia::mermaid {
struct ElkHorizontalMargin { double left = 0, right = 0; };
struct ElkRoutingPoint { double x = 0, y = 0; };
struct ElkRoutingSpacing {
    double node_node_between_layers = 20, edge_node_between_layers = 10,
        edge_edge_between_layers = 10;
};
struct ElkOrthogonalRoutes {
    std::vector<double> node_x, layer_width;
    // Includes the empty gap before the first and after the last layer.
    std::vector<std::size_t> gap_slots;
    std::vector<std::vector<ElkRoutingPoint>> bends;
    double width = 0;
};
// Consumes measured, ordered and vertically placed adjacent east/west edges,
// with one edge per port. Long edges have already been split into dummy nodes.
// node_y and margins use stable graph node indices; port.y includes its anchor.
// The seed overload is a standalone convenience. A composed pipeline passes
// the same RNG reference already consumed by crossing minimization.
[[nodiscard]] std::expected<ElkOrthogonalRoutes, std::string> elk_route_flat_edges(
    const ElkLayeredGraph& graph, std::span<const double> node_y,
    std::span<const ElkHorizontalMargin> margins,
    ElkRoutingSpacing spacing = {}, std::int64_t seed = 1);
[[nodiscard]] std::expected<ElkOrthogonalRoutes, std::string> elk_route_flat_edges(
    const ElkLayeredGraph& graph, std::span<const double> node_y,
    std::span<const ElkHorizontalMargin> margins, ElkRandom& random,
    ElkRoutingSpacing spacing = {});
}
