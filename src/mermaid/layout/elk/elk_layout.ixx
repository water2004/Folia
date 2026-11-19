// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_layout;
import std;
export import folia.mermaid.elk_end_labels;
export import folia.mermaid.elk_placement;
export import folia.mermaid.elk_ports;
export import folia.mermaid.elk_self_loops;
export import folia.mermaid.elk_greedy_switch;

export namespace folia::mermaid {
enum class ElkLayoutDirection { right, left, down, up };
struct ElkMeasuredNode {
    double width=0,height=0;
    bool center_ports=false;
    ElkSelfLoopNodeOptions self_loops;
};
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
    bool consider_model_order=false;
    double node_self_loop_spacing=10;
    // Original INCLUDE_CHILDREN wrapper semantics. The directional-scope
    // adapter resolves SEPARATE_CHILDREN first, then supplies an independent
    // graph with this false; included hierarchies retain the default.
    bool hierarchical_layout=true;
    // Non-hierarchical GraphConfigurator defaults. Decide before components
    // are split, using the original node count, without label/edge dummies.
    bool two_sided_greedy_switch=true;
    std::size_t greedy_switch_activation_threshold=40;
};
[[nodiscard]] constexpr bool elk_uses_greedy_switch(
    const ElkFlatLayoutOptions& options,std::size_t original_node_count) {
    return !options.hierarchical_layout&&options.two_sided_greedy_switch&&
        (options.greedy_switch_activation_threshold==0||
            original_node_count<options.greedy_switch_activation_threshold);
}
struct ElkNodeBox { double x=0,y=0,width=0,height=0; };
struct ElkFlatLayoutResult {
    std::vector<ElkNodeBox> nodes;
    std::vector<ElkRestoredRoute> routes;
    std::vector<ElkLabelPosition> center_labels,end_labels;
    double width=0,height=0;
};
// Connected ordinary flat graph, measured node and edge-label sizes. Runs the
// original layered phases with a shared RNG, balanced BK and direction export.
// Self loops are detached and restored by the original loop processors.
// Compound boundary ports belong to the compound pipeline.
[[nodiscard]] std::expected<ElkFlatLayoutResult,std::string> elk_layout_flat(
    const ElkFlatLayoutInput& input,const ElkFlatLayoutOptions& options={});
}
