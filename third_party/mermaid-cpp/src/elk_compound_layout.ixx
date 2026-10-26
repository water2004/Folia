// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_compound_layout;
import std;
export import folia.mermaid.elk_compound_restore;
export import folia.mermaid.elk_direction;

export namespace folia::mermaid {
struct ElkCompoundLayoutInput {
    ElkCompoundInput hierarchy;
    std::vector<ElkMeasuredNode> nodes;
    std::vector<ElkInlineCenterLabel> center_labels;
    std::vector<ElkEndLabel> end_labels;
    // Optional entries indexed by original node; non-groups are ignored.
    std::vector<ElkGroupLayoutBox> group_boxes;
    // Optional per-group layered options, indexed by original node. Root
    // options are supplied to elk_layout_compound; empty entries inherit them.
    std::vector<std::optional<ElkFlatLayoutOptions>> group_options;
};
struct ElkCompoundLayoutResult : ElkFlatLayoutResult {
    std::vector<std::optional<ElkNodeBox>> group_titles;
};
// INCLUDE_CHILDREN layered pipeline. All nested graphs inherit root direction,
// as in the original ElkGraphImporter. Group measured dimensions are
// minimum dimensions; children determine the final size. Output coordinates
// are absolute in the root graph, retaining original node/edge/label indices.
// Per-group spacing and padding remain independent of the inherited direction.
[[nodiscard]] std::expected<ElkCompoundLayoutResult,std::string> elk_layout_compound(
    const ElkCompoundLayoutInput& input,const ElkFlatLayoutOptions& options={});
}
