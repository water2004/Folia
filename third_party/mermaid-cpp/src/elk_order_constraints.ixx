// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_order_constraints;
import std;
export import folia.mermaid.elk_north_south;

export namespace folia::mermaid {
struct ElkOrderConstraints {
    std::vector<bool> normal;
    std::vector<std::vector<std::size_t>> units,successors;
};
[[nodiscard]] ElkOrderConstraints elk_make_order_constraints(
    const ElkLayeredGraph& graph,const ElkNorthSouthPreparation& prepared);
// Original ForsterConstraintResolver: called after barycenter sorting, with
// filled barycenters. Layout units keep their original initialization order.
void elk_resolve_order_constraints(const ElkOrderConstraints& constraints,
    std::vector<std::size_t>& layer,std::span<std::optional<double>> barycenters);
// Original additional in-layer crossings of N/S proxy connections, including
// long-edge segments traversing the layout unit. Adjacent-layer count is separate.
[[nodiscard]] std::size_t elk_count_north_south_crossings(const ElkLayeredGraph& graph,
    const ElkNorthSouthPreparation& prepared,std::span<const std::size_t> layer);
}
