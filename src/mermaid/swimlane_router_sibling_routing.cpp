// Based on Mermaid 12.2.0 swimlanes/direction/siblingSharedFaceRouting.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"
#include "swimlane_router_phases.inc"

namespace folia::mermaid::swimlane_routing_detail {

// siblingSharedFaceRouting.ts: rescue clear collinear paths with ports
// separated enough to leave room for antiparallel siblings' labels.
void straighten_sibling_detours(std::span<const SwimlaneRoutingNode> nodes,
                                std::span<SwimlaneRoutingEdge> edges) {
    for (Index i = 0; i < edges.size(); ++i) {
        auto& edge = edges[i];
        if (edge.start >= nodes.size() || edge.end >= nodes.size() ||
            !real_node(nodes[edge.start]) || !real_node(nodes[edge.end]) ||
            edge.placed.points.size() != 4)
            continue;
        const auto current = tracks_for(i, edge.placed.points, eps);
        if (current.size() != 3 || bends(current) != 2) continue;
        const auto& src = nodes[edge.start].box;
        const auto& dst = nodes[edge.end].box;
        const auto v = same(src.x, dst.x);
        if (v == same(src.y, dst.y)) continue;
        const auto positive = v ? dst.y > src.y : dst.x > src.x;
        const auto start = v ? (positive ? Side::bottom : Side::top)
                             : (positive ? Side::right : Side::left);
        const auto end = v ? (positive ? Side::top : Side::bottom)
                           : (positive ? Side::left : Side::right);
        if (line_hits_real_node(port(src, start, 0), port(dst, end, 0), nodes, edge.start, edge.end, -1))
            continue;
        double shift = 4;
        for (const auto& sibling : edges) {
            const auto same_pair = (sibling.start == edge.start && sibling.end == edge.end) ||
                                   (sibling.start == edge.end && sibling.end == edge.start);
            if (!same_pair || !sibling.label_node || *sibling.label_node >= nodes.size()) continue;
            const auto& label = nodes[*sibling.label_node].box;
            shift = std::max(shift, (v ? label.width : label.height) / 2 + 3);
        }
        for (const auto delta : {0., shift, -shift}) {
            if (std::abs(delta) >= std::min(v ? src.width : src.height,
                                            v ? dst.width : dst.height) / 2)
                continue;
            const auto a = port(src, start, delta), b = port(dst, end, delta);
            if (line_hits_real_node(a, b, nodes, edge.start, edge.end, -1)) continue;
            const TrackSegment candidate{i, 0, a, b, !v};
            bool conflict = false;
            for (Index j = 0; j < edges.size() && !conflict; ++j) {
                if (j == i) continue;
                for (const auto& other : tracks_for(j, edges[j].placed.points, eps)) {
                    if (crosses_at_segment(candidate, other, eps) ||
                        shared_axis_length(candidate, other, eps) > eps) {
                        conflict = true;
                        break;
                    }
                }
            }
            if (!conflict) {
                edge.placed.points = {a, b};
                break;
            }
        }
    }
}

} // namespace folia::mermaid::swimlane_routing_detail
