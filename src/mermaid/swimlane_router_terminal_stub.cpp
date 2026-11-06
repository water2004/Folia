// Based on Mermaid 12.2.0 swimlanes/direction/terminalStub.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"
#include "swimlane_router_phases.inc"

namespace folia::mermaid::swimlane_routing_detail {

// terminalStub.ts: replace a short destination stub with a face-center
// approach when both changed segments remain clear.
void collapse_short_terminal_stubs(std::span<SwimlaneRoutingNode> nodes,
                                   std::span<SwimlaneRoutingEdge> edges) {
    for (Index i = 0; i < edges.size(); ++i) {
        auto& edge = edges[i];
        const auto p = distinct_points(edge.placed.points);
        if (p.size() < 4 || edge.end >= nodes.size()) continue;
        const auto& dst = nodes[edge.end].box;
        if (dst.width <= 0 || dst.height <= 0) continue;
        const auto end = p.back(), corner = p[p.size() - 2], previous = p[p.size() - 3];
        const auto length = std::hypot(end.x - corner.x, end.y - corner.y);
        if (length >= 10 || length < geometry_eps) continue;
        const auto segs = tracks_for(i, std::span<const Point>{p}.last(3));
        if (segs.size() != 2 || segs[0].horizontal == segs[1].horizontal) continue;
        const auto r = rect(dst);
        Point new_previous, new_end;
        if (segs[0].horizontal) {
            new_previous = {previous.x, dst.y};
            new_end = {corner.x > previous.x ? r.right : r.left, dst.y};
        } else {
            new_previous = {dst.x, previous.y};
            new_end = {dst.x, corner.y < previous.y ? r.bottom : r.top};
        }
        if (line_hits_real_node(new_previous, new_end, nodes, edge.end, nodes.size(), 2))
            continue;
        const auto final_segment = segment_bounds(new_previous, new_end);
        if (std::ranges::any_of(nodes, [&](const SwimlaneRoutingNode& n) {
                return !n.group && n.edge_label && n.box.width > 0 && n.box.height > 0 &&
                    overlaps(final_segment, inflate(rect(n.box), 2));
            }))
            continue;
        if (edge.start < nodes.size() && nodes[edge.start].box.width > 0 &&
            nodes[edge.start].box.height > 0 &&
            inside(new_previous, inflate(rect(nodes[edge.start].box), -2)))
            continue;
        const auto own = tracks_for(i, p);
        const auto key = [](Point point) {
            return std::pair{std::round(point.x * 1000), std::round(point.y * 1000)};
        };
        const auto crosses_foreign = [&](Point a, Point b) {
            const std::array line{a, b};
            const auto tested = tracks_for(i, line);
            if (tested.empty()) return false;
            for (Index j = 0; j < edges.size(); ++j) {
                if (j == i) continue;
                for (const auto& other : tracks_for(j, edges[j].placed.points)) {
                    if (!strictly_crosses(tested.front(), other)) continue;
                    if (std::ranges::none_of(own, [&](const TrackSegment& s) {
                            return key(s.a) == key(other.a) && key(s.b) == key(other.b);
                        }))
                        return true;
                }
            }
            return false;
        };
        const auto before = p[p.size() - 4];
        if (crosses_foreign(new_previous, new_end) ||
            line_hits_real_node(before, new_previous, nodes, edge.start, edge.end, 2) ||
            crosses_foreign(before, new_previous))
            continue;
        auto& points = edge.placed.points;
        points.assign(p.begin(), std::prev(p.end(), 3));
        points.push_back(new_previous);
        points.push_back(new_end);
        if (!edge.label_node || *edge.label_node >= nodes.size()) continue;
        auto& label = nodes[*edge.label_node].box;
        if (label.width <= 0 || label.height <= 0) continue;
        double best = -1;
        for (const auto& s : tracks_for(i, points)) {
            const auto length = std::hypot(s.b.x - s.a.x, s.b.y - s.a.y);
            if (length > best && length >= (s.horizontal ? label.width : label.height) + 2) {
                best = length;
                label.x = (s.a.x + s.b.x) / 2;
                label.y = (s.a.y + s.b.y) / 2;
            }
        }
    }
}

} // namespace folia::mermaid::swimlane_routing_detail
