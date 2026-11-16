// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 ComponentsProcessor and SimpleRowGraphPlacer.
export module folia.mermaid.elk_components;
import std;
export import folia.mermaid.elk_compound_layout;

export namespace folia::mermaid {
struct ElkWeakComponent {
    // Nodes follow the original port-ordered DFS; edges retain input order.
    std::vector<std::size_t> nodes, edges;
};
struct ElkComponentSize {
    double width = 0, height = 0;
};
struct ElkComponentPacking {
    std::vector<std::size_t> order;
    // Indexed by the input component, independently of the packing order.
    std::vector<ElkRoutingPoint> offsets;
    double width = 0, height = 0;
};

[[nodiscard]] std::expected<std::vector<ElkWeakComponent>, std::string>
elk_split_components(std::size_t node_count, std::span<const ElkFlatEdge> edges);
// Component sizes exclude graph padding. Default priorities are equal, so
// packing uses stable ascending area and the upstream float aspect ratio.
[[nodiscard]] std::expected<ElkComponentPacking, std::string>
elk_pack_components(std::span<const ElkComponentSize> sizes, double spacing = 24,
                    double aspect_ratio = static_cast<double>(1.6f));

// Isolated flat scopes use independent layered components before row packing.
// Included hierarchies retain the compound pipeline and its boundary ports.
// Node, edge and label indices in the result always refer to the input graph.
[[nodiscard]] std::expected<ElkCompoundLayoutResult, std::string>
elk_layout_components(const ElkCompoundLayoutInput &input, const ElkFlatLayoutOptions &options = {});
} // namespace folia::mermaid
