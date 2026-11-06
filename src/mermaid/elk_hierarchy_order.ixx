// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 LayerSweepTypeDecider and hierarchical order transfer.
export module folia.mermaid.elk_hierarchy_order;
import std;
export import folia.mermaid.elk_long_edges;

export namespace folia::mermaid {
struct ElkHierarchyPortLink {
    std::size_t dummy_node = 0, parent_port = 0;
};
// Prepared adjacent-layer E/W graphs; links identify every external dummy's
// original parent port. Parent port order is clockwise, layer order top-down.
// The parent_fixed_order argument represents FIXED_ORDER as well as FIXED_POS.
[[nodiscard]] bool elk_hierarchy_uses_bottom_up(
    const ElkLayeredGraph& child, const ElkLayeredGraph& parent,
    std::size_t parent_node, std::span<const ElkHierarchyPortLink> links,
    bool parent_fixed_order = false);
// Before entering the child during a forward/backward hierarchical sweep.
// Returns false when the near layer is normal and needs random first ordering.
[[nodiscard]] bool elk_hierarchy_enter(
    ElkLayeredGraph& child, const ElkLayeredGraph& parent,
    std::size_t parent_node, std::span<const ElkHierarchyPortLink> links,
    bool forward);
// After the child sweep, transfer the far external layer to its parent ports.
void elk_hierarchy_leave(
    const ElkLayeredGraph& child, ElkLayeredGraph& parent,
    std::size_t parent_node, std::span<const ElkHierarchyPortLink> links,
    bool forward);
}
