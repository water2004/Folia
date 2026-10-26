// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 Brandes & Koepf placement; see ../ELK-LICENSE.
export module folia.mermaid.elk_placement;
import std;
export import folia.mermaid.elk_long_edges;

export namespace folia::mermaid {
struct ElkAlignedBlockLayout {
    bool right = true, down = true;
    std::vector<std::size_t> root, align;
    std::vector<double> inner_shift, block_size;
    std::vector<bool> only_dummies;
};
struct ElkBlockAlignment {
    std::vector<bool> marked_edges;
    // Original BKNodePlacer iteration order: right/down, right/up, left/down, left/up.
    std::array<ElkAlignedBlockLayout, 4> layouts;
};
// Consumes ordered flat layers with adjacent east/west edges. Port y is the
// attachment position (local port position plus anchor). Node dimensions and
// margins must already be measured. This stage does not assign final coordinates.
[[nodiscard]] std::expected<ElkBlockAlignment, std::string> elk_align_flat_blocks(
    const ElkLayeredGraph& graph);
struct ElkPlacementSpacing {
    double node_node = 20, edge_node = 10, edge_edge = 10;
};
// Original BK compaction with IMPROVE_STRAIGHTNESS, followed by balanced
// selection (or the original narrowest feasible layout for favorStraightEdges).
[[nodiscard]] std::expected<std::vector<double>, std::string> elk_place_flat_nodes(
    const ElkLayeredGraph& graph, ElkPlacementSpacing spacing = {}, bool favor_straight_edges = true);
}
