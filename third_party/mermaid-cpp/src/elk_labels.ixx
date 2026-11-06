// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 center-label dummy processors; see ../ELK-LICENSE.
export module folia.mermaid.elk_labels;
import std;
export import folia.mermaid.elk_restore;
export import folia.mermaid.elk_boundary_layers;

export namespace folia::mermaid {
enum class ElkLabelDirection { horizontal, down, up };
struct ElkInlineCenterLabel {
    std::size_t id = 0, original_edge = 0;
    double width = 0, height = 0;
};
struct ElkLabelDummy {
    std::size_t node = 0;
    std::vector<ElkInlineCenterLabel> labels;
};
struct ElkLabelPreparation {
    ElkLayeredGraph graph;
    std::vector<ElkLabelDummy> dummies;
};
struct ElkLabelPosition {
    std::size_t id = 0, original_edge = 0;
    double x = 0, y = 0, width = 0, height = 0;
};
// Original DFS -> LabelDummyInserter -> NetworkSimplex order. Measured ordinary
// node sizes are attached to graph afterwards. Each edge has its own ports.
// Labels are the adapter's CENTER/inline labels, in their original label order.
[[nodiscard]] std::expected<ElkLabelPreparation,std::string> elk_prepare_inline_labels(
    const ElkCycleGraph& source, std::span<const ElkInlineCenterLabel> labels,
    ElkLabelDirection direction = ElkLabelDirection::horizontal,
    double edge_label_spacing = 2, double label_label_spacing = 0,
    std::span<const ElkExternalLayerRole> boundary_roles = {});
// After crossing order, before node placement: original default MEDIAN_LAYER.
void elk_switch_label_dummies(ElkLayeredGraph& graph);
// After crossing order, before placement: LabelSideSelector INLINE geometry.
void elk_place_inline_label_ports(ElkLayeredGraph& graph,
    std::span<const ElkLabelDummy> dummies, double edge_label_spacing = 2);
// LabelDummyRemover's label geometry; route joining is elk_restore_flat_routes.
[[nodiscard]] std::vector<ElkLabelPosition> elk_restore_inline_labels(
    const ElkLayeredGraph& graph, std::span<const ElkLabelDummy> dummies,
    std::span<const double> node_x, std::span<const double> node_y,
    ElkLabelDirection direction = ElkLabelDirection::horizontal,
    double label_label_spacing = 0);
}
