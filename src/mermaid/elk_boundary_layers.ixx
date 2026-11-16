// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_boundary_layers;
import std;
export import folia.mermaid.elk_layering;

export namespace folia::mermaid {
enum class ElkExternalLayerRole { none, first_separate, last_separate };
// EdgeAndLayerConstraintEdgeReverser runs before DFS. Reversals retain their
// original edge ids and order so later path restoration can undo both stages.
[[nodiscard]] std::expected<std::vector<std::size_t>,std::string> elk_boundary_feedback_edges(
    const ElkCycleGraph& source,std::span<const ElkExternalLayerRole> roles);
// LayerConstraintPreprocessor/NetworkSimplex/LayerConstraintPostprocessor for
// direction-normalized implicit compound boundaries. Ordinary nodes retain
// their input order and all output ids refer to the original graph.
[[nodiscard]] std::expected<ElkLayeringResult,std::string> elk_layer_boundary_graph(
    const ElkCycleGraph& source,std::span<const ElkExternalLayerRole> roles);
}
