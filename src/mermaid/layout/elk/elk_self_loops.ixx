// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 self-loop processors for the existing implicit one-edge ports.
export module folia.mermaid.elk_self_loops;
import std;
export import folia.mermaid.elk_end_labels;
export import folia.mermaid.elk_ports;

export namespace folia::mermaid {
enum class ElkSelfLoopDistribution { north, north_south, equally };
enum class ElkSelfLoopOrdering { stacked, reverse_stacked, sequenced };
struct ElkSelfLoopNodeOptions {
    ElkSelfLoopDistribution distribution=ElkSelfLoopDistribution::north;
    ElkSelfLoopOrdering ordering=ElkSelfLoopOrdering::stacked;
};
struct ElkSelfLoopLabel {
    std::size_t id=0;
    double width=0,height=0;
    bool terminal=false;
};
struct ElkSelfLoop {
    std::size_t node=0,original_edge=0,source_port=0,target_port=0;
    bool hidden_ports=true;
    std::vector<ElkSelfLoopLabel> labels;
};
struct ElkSelfLoopPreparation {
    ElkCycleGraph graph;
    std::vector<ElkInlineCenterLabel> center_labels;
    std::vector<ElkEndLabel> end_labels;
    std::vector<std::size_t> edge_origins,port_origins;
    std::vector<std::optional<std::size_t>> port_indices;
    std::vector<ElkSelfLoop> loops;
    std::vector<ElkSelfLoopNodeOptions> node_options;
};
// Detach loops before cycle breaking. A compound node with imported boundary
// ports retains its port list, as in SelfLoopPreProcessor.hidePorts.
[[nodiscard]] std::expected<ElkSelfLoopPreparation,std::string> elk_prepare_self_loops(
    const ElkCycleGraph&,std::span<const ElkInlineCenterLabel>,std::span<const ElkEndLabel>,
    std::span<const std::size_t> retained_port_nodes={});
// Restore clockwise port order after crossing minimization, before the shared
// point-port placement. Loop ports remain disconnected from ordinary routing.
void elk_restore_self_loop_ports(ElkSelfLoopPreparation&,ElkLayeredGraph&,
    std::span<const ElkSelfLoopNodeOptions> node_options={});
struct ElkSelfLoopSpacing {
    double node_loop=10,edge_edge=10,edge_label=2,label_label=0;
    ElkLabelDirection label_direction=ElkLabelDirection::horizontal;
};
struct ElkSelfLoopLabelPosition {
    std::size_t node=0;
    bool terminal=false;
    ElkLabelPosition label;
};
struct ElkSelfLoopGeometry {
    // Coordinates are relative to the owning node until postprocessing.
    std::vector<ElkRestoredRoute> routes;
    std::vector<ElkSelfLoopLabelPosition> labels;
};
// RoutingDirector, LabelPlacer, RoutingSlotAssigner and OrthogonalSelfLoopRouter.
// Reserve node margins before BK placement, including all loop labels.
[[nodiscard]] ElkSelfLoopGeometry elk_route_self_loops(
    const ElkSelfLoopPreparation&,ElkLayeredGraph&,std::span<ElkHorizontalMargin>,
    ElkRandom&,const ElkSelfLoopSpacing& = {});
// Restore original identities for ordinary routes/labels, then translate and
// append detached loops after ordinary routing. Keeps caller output ordered.
void elk_restore_self_loops(const ElkSelfLoopPreparation&,const ElkSelfLoopGeometry&,
    std::span<const double> node_x,std::span<const double> node_y,
    std::vector<ElkRestoredRoute>&,std::vector<ElkLabelPosition>& centers,
    std::vector<ElkLabelPosition>& ends);
}
