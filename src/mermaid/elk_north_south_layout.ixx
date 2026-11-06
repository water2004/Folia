// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_north_south_layout;
import std;
export import folia.mermaid.elk_north_south;
export import folia.mermaid.elk_layout;

export namespace folia::mermaid {
struct ElkNorthSouthLayoutResult {
    std::vector<double> node_y;
    ElkOrthogonalRoutes routing;
    std::vector<ElkRestoredRoute> routes;
    double height=0,offset_y=0;
};
// Consumes the actual ordered proxy graph after nested groups have supplied
// fixed node dimensions and point-port positions. Includes the port extents
// in node margins, BK placement, original routing and N/S endpoint restoration.
[[nodiscard]] std::expected<ElkNorthSouthLayoutResult,std::string> elk_layout_north_south_graph(
    ElkLayeredGraph& graph,const ElkNorthSouthPreparation& prepared,
    ElkRandom& random,const ElkFlatLayoutOptions& options={});
}
