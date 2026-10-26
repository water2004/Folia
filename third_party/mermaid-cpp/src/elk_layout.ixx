// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_layout;
import std;
export import folia.mermaid.elk_end_labels;
export import folia.mermaid.elk_placement;
export import folia.mermaid.elk_ports;

export namespace folia::mermaid {
enum class ElkLayoutDirection { right, left, down, up };
struct ElkMeasuredNode { double width=0,height=0; bool center_ports=false; };
struct ElkFlatEdge { std::size_t source=0,target=0; };
struct ElkFlatLayoutInput {
    std::vector<ElkMeasuredNode> nodes;
    std::vector<ElkFlatEdge> edges;
    std::vector<ElkInlineCenterLabel> center_labels;
    std::vector<ElkEndLabel> end_labels;
};
struct ElkFlatLayoutOptions {
    ElkLayoutDirection direction=ElkLayoutDirection::right;
    ElkPlacementSpacing placement;
    ElkRoutingSpacing routing;
    ElkPointPortOptions ports;
    double edge_label_spacing=2,label_label_spacing=0,padding=12;
    std::int64_t seed=1;
    bool unnecessary_bendpoints=true;
};
struct ElkNodeBox { double x=0,y=0,width=0,height=0; };
struct ElkFlatLayoutResult {
    std::vector<ElkNodeBox> nodes;
    std::vector<ElkRestoredRoute> routes;
    std::vector<ElkLabelPosition> center_labels,end_labels;
    double width=0,height=0;
};
// Connected ordinary flat graph, measured node and edge-label sizes. Runs the
// original layered phases with a shared RNG, balanced BK and direction export.
// Compound boundary ports and self loops require their own original processors.
[[nodiscard]] std::expected<ElkFlatLayoutResult,std::string> elk_layout_flat(
    const ElkFlatLayoutInput& input,const ElkFlatLayoutOptions& options={});
}
