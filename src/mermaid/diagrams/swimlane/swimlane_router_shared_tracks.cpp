// Based on Mermaid 12.2.0 swimlanes/direction/sharedTrackNudging.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"
#include "swimlane_router_phases.inc"

namespace folia::mermaid::swimlane_routing_detail {

namespace {

bool crowded_track(const TrackSegment& a, const TrackSegment& b) {
    if (a.horizontal != b.horizontal) return false;
    if (a.horizontal)
        return overlap_length(a.a.x, a.b.x, b.a.x, b.b.x) >= 8 && same(a.a.y, b.a.y, 7);
    return overlap_length(a.a.y, a.b.y, b.a.y, b.b.y) >= 8 && same(a.a.x, b.a.x, 7);
}

} // namespace

// sharedTrackNudging.ts: keep terminal stubs pinned, attempting a bounded
// interior track shift or a source detour when the direct shift is unsafe.
void nudge_shared_tracks(std::span<const SwimlaneRoutingNode> nodes,
                         std::span<SwimlaneRoutingEdge> edges) {
    std::vector<std::pair<Index, Rect>> node_rects;
    for (Index i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        if (!node.group && node.box.width > 0 && node.box.height > 0)
            node_rects.emplace_back(i, inflate(rect(node.box), 2));
    }
    for (Index iteration = 0; iteration < 12; ++iteration) {
        std::vector<std::vector<Point>> points;
        std::vector<TrackSegment> all;
        for (Index i = 0; i < edges.size(); ++i) {
            points.push_back(distinct_points(edges[i].placed.points));
            const auto segments = tracks_for(i, points.back());
            all.insert(all.end(), segments.begin(), segments.end());
        }
        const auto safe = [&](Index i, const std::vector<Point>& candidate) {
            const auto segs = tracks_for(i, candidate);
            if (segs.size() + 1 != candidate.size()) return false;
            const auto& edge = edges[i];
            for (const auto& seg : segs) {
                for (const auto& [id, bounds] : node_rects) {
                    const auto ignored = nodes[id].edge_label ? edge.label_node == id
                                                              : edge.start == id || edge.end == id;
                    if (!ignored && overlaps(segment_bounds(seg.a, seg.b), bounds)) return false;
                }
                for (const auto& other : all)
                    if (other.edge != i && (crowded_track(seg, other) || strictly_crosses(seg, other)))
                        return false;
            }
            return true;
        };
        const auto shift_track = [&](const TrackSegment& seg, double delta) {
            auto candidate = points[seg.edge];
            auto& a = candidate[seg.index];
            auto& b = candidate[seg.index + 1];
            (seg.horizontal ? a.y : a.x) += delta;
            (seg.horizontal ? b.y : b.x) += delta;
            return candidate;
        };
        const auto detour = [&](const TrackSegment& seg, double delta) -> std::vector<Point> {
            const auto& original = points[seg.edge];
            const auto& edge = edges[seg.edge];
            if (original.size() != 4 || seg.index != 1 ||
                edge.start >= nodes.size() || edge.end >= nodes.size())
                return {};
            const auto& source = nodes[edge.start].box;
            const auto& target = nodes[edge.end].box;
            if (source.width <= 0 || source.height <= 0 ||
                target.width <= 0 || target.height <= 0)
                return {};
            const auto r = rect(source);
            std::vector<Point> path;
            if (seg.horizontal) {
                const auto right = target.x >= source.x;
                const auto port = right ? r.right : r.left;
                const auto stub = port + (right ? 20 : -20);
                if (right ? seg.b.x <= stub + geometry_eps : seg.b.x >= stub - geometry_eps)
                    return {};
                const auto rail = seg.a.y + delta;
                path = {{port, source.y}, {stub, source.y}, {stub, rail}, {seg.b.x, rail}};
            } else {
                const auto below = target.y >= source.y;
                const auto port = below ? r.bottom : r.top;
                const auto stub = port + (below ? 20 : -20);
                if (below ? seg.b.y <= stub + geometry_eps : seg.b.y >= stub - geometry_eps)
                    return {};
                const auto rail = seg.a.x + delta;
                path = {{source.x, port}, {source.x, stub}, {rail, stub}, {rail, seg.b.y}};
            }
            path.push_back(original.back());
            return distinct_points(path);
        };
        const auto try_segment = [&](const TrackSegment& seg) {
            const auto count = points[seg.edge].size();
            if (count < 4 || seg.index < 1 || seg.index > count - 3) return false;
            for (const auto delta : {-7., 7., -14., 14., -21., 21.}) {
                auto candidate = shift_track(seg, delta);
                if (!safe(seg.edge, candidate)) candidate = detour(seg, delta);
                if (candidate.empty() || !safe(seg.edge, candidate)) continue;
                edges[seg.edge].placed.points = std::move(candidate);
                return true;
            }
            return false;
        };
        bool fixed = false;
        for (Index i = 0; i < all.size() && !fixed; ++i)
            for (Index j = i + 1; j < all.size() && !fixed; ++j)
                if (all[i].edge != all[j].edge && crowded_track(all[i], all[j]))
                    fixed = try_segment(all[i]) || try_segment(all[j]);
        if (!fixed) return;
    }
}

} // namespace folia::mermaid::swimlane_routing_detail
