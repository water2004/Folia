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
};
struct ElkCompoundLayoutResult : ElkFlatLayoutResult {
    std::vector<std::optional<ElkNodeBox>> group_titles;
};
// Same-direction compound pipeline (RIGHT/LEFT/DOWN/UP). Group measured dimensions are
// minimum dimensions; children determine the final size. Output coordinates
// are absolute in the root graph, retaining original node/edge/label indices.
// Mixed directions need the original north/south hierarchy port processors.
[[nodiscard]] std::expected<ElkCompoundLayoutResult,std::string> elk_layout_compound(
    const ElkCompoundLayoutInput& input,const ElkFlatLayoutOptions& options={});
}
