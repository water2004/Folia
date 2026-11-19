// Based on Mermaid 12.2.0 swimlanes/direction/portSwap.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"
#include "swimlane_router_phases.inc"

namespace folia::mermaid::swimlane_routing_detail {

// portSwap.ts: shorten a three-segment route by using the perpendicular
// source face when that preserves clearance from all other geometry.
void swap_source_ports(std::span<const SwimlaneRoutingNode> nodes,
                       std::span<SwimlaneRoutingEdge> edges) {
    std::vector<std::pair<Index, Rect>> node_rects;
    for (Index i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        if (!n.group && !n.edge_label && n.box.width > 0 && n.box.height > 0)
            node_rects.emplace_back(i, inflate(rect(n.box), -1));
    }
    const auto conflicts = [&](Point a, Point b, Index i, Index ignored_node) {
        if (same(a, b)) return false;
        const auto bounds = segment_bounds(a, b);
        for (const auto& [id, r] : node_rects)
            if (id != ignored_node && overlaps(bounds, r)) return true;
        const auto h = same(a.y, b.y);
        for (Index j = 0; j < edges.size(); ++j) {
            if (j == i) continue;
            const auto& points = edges[j].placed.points;
            for (Index k = 1; k < points.size(); ++k) {
                const auto p = points[k - 1], q = points[k];
                if (same(p, q)) continue;
                const auto other_h = same(p.y, q.y);
                if (!other_h && !same(p.x, q.x)) continue;
                if (h == other_h) {
                    if (h ? same(a.y, p.y) && overlap_length(a.x, b.x, p.x, q.x) > eps
                          : same(a.x, p.x) && overlap_length(a.y, b.y, p.y, q.y) > eps)
                        return true;
                    continue;
                }
                const auto other = segment_bounds(p, q);
                const auto horizontal = h ? bounds : other;
                const auto vertical = h ? other : bounds;
                const Point hit{vertical.left, horizontal.top};
                if (hit.x >= horizontal.left && hit.x <= horizontal.right &&
                    hit.y >= vertical.top && hit.y <= vertical.bottom &&
                    !((same(hit, a) || same(hit, b)) && (same(hit, p) || same(hit, q))))
                    return true;
            }
        }
        return false;
    };
    for (Index i = 0; i < edges.size(); ++i) {
        auto& e = edges[i];
        if (e.start >= nodes.size() || e.end >= nodes.size() || e.placed.points.size() != 4)
            continue;
        const auto& source = nodes[e.start];
        const auto& target = nodes[e.end];
        const auto& a = source.box;
        const auto& b = target.box;
        if (source.group || source.edge_label || target.group || target.edge_label ||
            a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0 ||
            same(a.x, b.x) || same(a.y, b.y))
            continue;
        const auto& p = e.placed.points;
        const auto h = [&](Index j) { return same(p[j].y, p[j + 1].y) && !same(p[j].x, p[j + 1].x); };
        const auto v = [&](Index j) { return same(p[j].x, p[j + 1].x) && !same(p[j].y, p[j + 1].y); };
        const auto hvh = h(0) && v(1) && h(2);
        if (!hvh && !(v(0) && h(1) && v(2))) continue;
        const auto r = rect(a);
        const auto end = p.back();
        for (const auto offset : {0., 8., -8., 16., -16.}) {
            const Point start = hvh ? Point{a.x + offset, b.y > a.y ? r.bottom : r.top}
                                    : Point{b.x > a.x ? r.right : r.left, a.y + offset};
            if (hvh ? start.x <= r.left + eps || start.x >= r.right - eps
                    : start.y <= r.top + eps || start.y >= r.bottom - eps)
                continue;
            const Point bend = hvh ? Point{start.x, end.y} : Point{end.x, start.y};
            const auto first_degenerate = same(start, bend), second_degenerate = same(bend, end);
            if ((first_degenerate && second_degenerate) ||
                conflicts(start, bend, i, e.start) || conflicts(bend, end, i, e.end))
                continue;
            e.placed.points = first_degenerate ? std::vector<Point>{bend, end}
                            : second_degenerate ? std::vector<Point>{start, bend}
                                                : std::vector<Point>{start, bend, end};
            break;
        }
    }
}

} // namespace folia::mermaid::swimlane_routing_detail
