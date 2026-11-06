// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_end_labels;
import std;
export import folia.mermaid.elk_labels;

export namespace folia::mermaid {
struct ElkEndLabel {
    std::size_t id = 0, original_edge = 0;
    bool head = false;
    double width = 0, height = 0;
};
struct ElkEndLabelCell {
    std::size_t node = 0, port = 0;
    bool above = false, west = false;
    double x = 0, y = 0, width = 0, height = 0;
    std::vector<ElkEndLabel> labels;
};
// Original SMART_DOWN and EndLabelPreprocessor for one edge per east/west
// port. Consumes sorted ports with final local positions, before BK placement.
// Mutates node vertical margins and the supplied horizontal margins together.
[[nodiscard]] std::expected<std::vector<ElkEndLabelCell>,std::string> elk_prepare_end_labels(
    ElkLayeredGraph& graph, std::span<const ElkEndLabel> labels,
    std::span<ElkHorizontalMargin> margins, bool vertical_layout = false,
    double edge_label_spacing = 2, double label_label_spacing = 0,
    bool default_above = false, bool select_sides = true);
// Original EndLabelPostprocessor; called with final node positions after route
// restoration. Original endpoint identities survive feedback-edge reversal.
[[nodiscard]] std::vector<ElkLabelPosition> elk_restore_end_labels(
    std::span<const ElkEndLabelCell> cells, std::span<const double> node_x,
    std::span<const double> node_y, bool vertical_layout = false,
    double label_label_spacing = 0);
}
