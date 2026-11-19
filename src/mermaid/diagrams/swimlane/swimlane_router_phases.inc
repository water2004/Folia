// Private pass entry points. The ordered pipeline lives in swimlane_router_post_processing.cpp.
#pragma once

namespace folia::mermaid::swimlane_routing_detail {

void clip_endpoints(std::span<const SwimlaneRoutingNode> nodes,
                    std::span<SwimlaneRoutingEdge> edges);
void prepare_endpoints(std::span<const SwimlaneRoutingNode> nodes,
                       std::span<SwimlaneRoutingEdge> edges);
void simplify_detoured_edges(std::span<const SwimlaneRoutingNode> nodes,
                             std::span<SwimlaneRoutingEdge> edges);
void straighten_sibling_detours(std::span<const SwimlaneRoutingNode> nodes,
                                std::span<SwimlaneRoutingEdge> edges);
void swap_source_ports(std::span<const SwimlaneRoutingNode> nodes,
                       std::span<SwimlaneRoutingEdge> edges);
void nudge_shared_tracks(std::span<const SwimlaneRoutingNode> nodes,
                         std::span<SwimlaneRoutingEdge> edges);
void separate_terminal_lanes(std::span<const SwimlaneRoutingNode> nodes,
                             std::span<SwimlaneRoutingEdge> edges);
void collapse_rectangular_doglegs(std::span<const SwimlaneRoutingNode> nodes,
                                  std::span<SwimlaneRoutingEdge> edges);
void lift_obstacle_hugging_rails(std::span<const SwimlaneRoutingNode> nodes,
                                 std::span<SwimlaneRoutingEdge> edges);
void swap_destination_tails(std::span<const SwimlaneRoutingNode> nodes,
                            std::span<SwimlaneRoutingEdge> edges);
void resolve_materialized_crossings(std::span<const SwimlaneRoutingNode> nodes,
                                    std::span<SwimlaneRoutingEdge> edges);
void reassign_external_rails(std::span<const SwimlaneRoutingNode> nodes,
                             std::span<SwimlaneRoutingEdge> edges);
void shortcut_orthogonal_jogs(std::span<const SwimlaneRoutingNode> nodes,
                              std::span<SwimlaneRoutingEdge> edges);
void collapse_short_terminal_stubs(std::span<SwimlaneRoutingNode> nodes,
                                   std::span<SwimlaneRoutingEdge> edges);
void anchor_labels(std::span<SwimlaneRoutingNode> nodes,
                   std::span<SwimlaneRoutingEdge> edges);

} // namespace folia::mermaid::swimlane_routing_detail
