// Based on Mermaid 12.2.0 swimlanes/direction/detourSimplification.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"
#include "swimlane_router_phases.inc"

namespace folia::mermaid::swimlane_routing_detail {

namespace {

Side nearest_side(Point p, Rect r) {
    const std::array distances{std::abs(p.y - r.top), std::abs(p.y - r.bottom),
                               std::abs(p.x - r.left), std::abs(p.x - r.right)};
    return cardinal_sides[std::ranges::min_element(distances) - distances.begin()];
}

} // namespace

// detourSimplification.ts: try unclaimed face pairs and outer channels for
// paths with at least four bends, preserving obstacle clearance.
void simplify_detoured_edges(std::span<const SwimlaneRoutingNode> nodes,
                             std::span<SwimlaneRoutingEdge> edges) {
    std::array outside{inf, -inf, inf, -inf};
    for (const auto& n : nodes) {
        if (!real_node(n)) continue;
        const auto r = rect(n.box);
        outside[0] = std::min(outside[0], r.top - anchor_offset);
        outside[1] = std::max(outside[1], r.bottom + anchor_offset);
        outside[2] = std::min(outside[2], r.left - anchor_offset);
        outside[3] = std::max(outside[3], r.right + anchor_offset);
    }
    struct Claim { Index edge; Side side; };
    std::vector<std::vector<Claim>> claims(nodes.size());
    const auto materialized = [&](Index i) { return i < nodes.size() && real_node(nodes[i]); };
    for (Index i = 0; i < edges.size(); ++i) {
        const auto& edge = edges[i];
        const auto& p = edge.placed.points;
        if (p.empty()) continue;
        if (materialized(edge.start))
            claims[edge.start].push_back({i, nearest_side(p.front(), rect(nodes[edge.start].box))});
        if (materialized(edge.end))
            claims[edge.end].push_back({i, nearest_side(p.back(), rect(nodes[edge.end].box))});
    }
    const auto claimed = [&](Index node, Side side, Index edge) {
        return std::ranges::any_of(claims[node], [&](Claim claim) {
            return claim.side == side && claim.edge != edge;
        });
    };
    const auto conflicts = [&](std::span<const TrackSegment> path, Index i, bool include_incident) {
        Index count = 0;
        for (Index j = 0; j < edges.size(); ++j) {
            if (i == j || (!include_incident && incident_edges(edges[i], edges[j]))) continue;
            for (const auto& a : path)
                for (const auto& b : tracks_for(j, edges[j].placed.points))
                    if (crosses_at_segment(a, b, geometry_eps) ||
                        shared_axis_length(a, b, geometry_eps) >= 8)
                        ++count;
        }
        return count;
    };
    for (Index i = 0; i < edges.size(); ++i) {
        auto& edge = edges[i];
        if (!materialized(edge.start) || !materialized(edge.end)) continue;
        const auto current = tracks_for(i, edge.placed.points);
        auto best_bends = bends(current);
        if (best_bends < 4) continue;
        const auto current_crossings = conflicts(current, i, true);
        const auto current_non_incident = conflicts(current, i, false);
        auto best_crossings = current_crossings;
        std::vector<Point> best;
        const auto consider = [&](std::vector<Point> candidate) {
            if (candidate.empty()) return;
            for (Index j = 1; j < candidate.size(); ++j)
                if (line_hits_real_node(candidate[j - 1], candidate[j], nodes, edge.start, edge.end, -1))
                    return;
            const auto segments = tracks_for(i, candidate);
            const auto candidate_bends = bends(segments);
            if (current_crossings > 0) {
                const auto crossings = conflicts(segments, i, true);
                if (crossings > best_crossings ||
                    (crossings == best_crossings && candidate_bends >= best_bends))
                    return;
                best_crossings = crossings;
            } else if (candidate_bends >= best_bends ||
                       conflicts(segments, i, false) > current_non_incident) {
                return;
            }
            best_bends = candidate_bends;
            best = std::move(candidate);
        };
        const auto& src = nodes[edge.start].box;
        const auto& dst = nodes[edge.end].box;
        for (const auto start : cardinal_sides) {
            if (claimed(edge.start, start, i)) continue;
            const auto a = port(src, start, 0);
            for (const auto end : cardinal_sides) {
                if (claimed(edge.end, end, i)) continue;
                const auto b = port(dst, end, 0);
                consider(port_path(a, start, b, end, anchor_offset));
                if (start == end) consider(same_side_path(a, start, b, outside[side_index(start)]));
            }
        }
        if (best.empty()) continue;
        edge.placed.points = std::move(best);
        const auto refresh = [&](Index node, Point p) {
            std::erase_if(claims[node], [&](Claim c) { return c.edge == i; });
            claims[node].push_back({i, nearest_side(p, rect(nodes[node].box))});
        };
        refresh(edge.start, edge.placed.points.front());
        refresh(edge.end, edge.placed.points.back());
    }
}

} // namespace folia::mermaid::swimlane_routing_detail
