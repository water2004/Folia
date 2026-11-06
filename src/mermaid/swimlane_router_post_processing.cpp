// Based on Mermaid 12.2.0 swimlanes/postProcessing.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"
#include "swimlane_router_phases.inc"

namespace folia::mermaid {

void finish_swimlane_routes(std::span<SwimlaneRoutingNode> nodes,
                            std::span<SwimlaneRoutingEdge> edges) {
    using namespace swimlane_routing_detail;
    for (auto& e : edges)
        if (e.placed.points.size() >= 2)
            e.placed.points = simplify(std::move(e.placed.points));
    simplify_detoured_edges(nodes, edges);
    straighten_sibling_detours(nodes, edges);
    swap_source_ports(nodes, edges);
    anchor_labels(nodes, edges);
    clip_endpoints(nodes, edges);
    collapse_short_terminal_stubs(nodes, edges);
    nudge_shared_tracks(nodes, edges);
    separate_terminal_lanes(nodes, edges);
    collapse_rectangular_doglegs(nodes, edges);
    lift_obstacle_hugging_rails(nodes, edges);
    swap_destination_tails(nodes, edges);
    // Keep the browser handoff's duplicate vertices between passes, where
    // label placement uses the original index of each visible segment.
    const auto finalize = [&] {
        resolve_materialized_crossings(nodes, edges);
        separate_terminal_lanes(nodes, edges);
        reassign_external_rails(nodes, edges);
        shortcut_orthogonal_jogs(nodes, edges);
        anchor_labels(nodes, edges);
        prepare_endpoints(nodes, edges);
        lift_obstacle_hugging_rails(nodes, edges);
        anchor_labels(nodes, edges);
        prepare_endpoints(nodes, edges);
    };
    finalize();
    nudge_shared_tracks(nodes, edges);
    finalize();
    // The native SVG adapter consumes the materialized endpoints directly.
    for (auto& edge : edges)
        edge.placed.points = distinct_points(edge.placed.points);
}

} // namespace folia::mermaid
