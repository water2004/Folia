// Based on Mermaid 12.2.0 swimlanes/direction/labelAnchoring.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"
#include "swimlane_router_phases.inc"

namespace folia::mermaid::swimlane_routing_detail {

namespace {

Rect marker_bounds(Point tip, Point inner) {
    if (std::abs(tip.y - inner.y) <= geometry_eps) {
        const auto x = tip.x + sign(inner.x - tip.x) * 10;
        return {std::min(tip.x, x), std::max(tip.x, x), tip.y - 7, tip.y + 7};
    }
    if (std::abs(tip.x - inner.x) <= geometry_eps) {
        const auto y = tip.y + sign(inner.y - tip.y) * 10;
        return {tip.x - 7, tip.x + 7, std::min(tip.y, y), std::max(tip.y, y)};
    }
    return segment_bounds(tip, inner);
}

} // namespace

void anchor_labels(std::span<SwimlaneRoutingNode> nodes, std::span<SwimlaneRoutingEdge> edges) {
    std::vector<std::pair<Index, Rect>> lanes, content, foreign_edges, placed_labels;
    for (Index i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        if (node.box.width <= 0 || node.box.height <= 0) continue;
        if (node.group && !node.parent)
            lanes.emplace_back(i, rect(node.box));
        else if (!node.group && !node.edge_label)
            content.emplace_back(i, rect(node.box));
    }
    for (Index i = 0; i < edges.size(); ++i) {
        const auto& p = edges[i].placed.points;
        for (Index j = 1; j < p.size(); ++j)
            foreign_edges.emplace_back(i, segment_bounds(p[j - 1], p[j]));
    }
    for (Index i = 0; i < edges.size(); ++i) {
        auto& e = edges[i];
        if (!e.label_node || *e.label_node >= nodes.size()) continue;
        auto& label = nodes[*e.label_node];
        const auto w = label.box.width, h = label.box.height;
        const auto& p = e.placed.points;
        if (p.size() < 2 || w <= 0 || h <= 0) continue;
        const auto label_rect = [&](Point q) {
            return Rect{q.x - w / 2, q.x + w / 2, q.y - h / 2, q.y + h / 2};
        };
        struct Candidate { Index index; bool horizontal; double length; };
        std::vector<Candidate> segments;
        std::vector<Point> distinct;
        for (Index j = 0; j < p.size(); ++j) {
            if (distinct.empty() || !same(distinct.back(), p[j], geometry_eps))
                distinct.push_back(p[j]);
            if (!j) continue;
            const auto dx = std::abs(p[j].x - p[j - 1].x), dy = std::abs(p[j].y - p[j - 1].y);
            if ((dx < geometry_eps) != (dy < geometry_eps))
                segments.push_back({j - 1, dx >= geometry_eps, dx + dy});
        }
        if (segments.empty() || distinct.size() < 2) continue;
        const auto start_marker = marker_bounds(distinct[0], distinct[1]);
        const auto end_marker = marker_bounds(distinct.back(), distinct[distinct.size() - 2]);
        const auto rank = [&](const Candidate& s) {
            const auto axis = s.horizontal == (w >= h);
            const auto fits = s.length >= (s.horizontal ? w : h) + 2;
            return std::tuple{axis, fits, s.length};
        };
        const auto first_visible = segments.front().index, last_visible = segments.back().index;
        const auto visible_count = segments.size();
        std::stable_sort(segments.begin(), segments.end(), [&](const Candidate& a, const Candidate& b) {
            return rank(a) > rank(b);
        });
        const auto place = [&](Point q) -> std::optional<std::pair<Index, Point>> {
            const auto r = label_rect(q);
            for (const auto& [id, lane] : lanes)
                if (contains(lane, r)) return std::pair{id, q};
            for (const auto& [id, lane] : lanes) {
                if (!within(q.x, lane.left, lane.right) || !within(q.y, lane.top, lane.bottom)) continue;
                const auto l = lane.left + w / 2 + 1, r = lane.right - w / 2 - 1;
                const auto t = lane.top + h / 2 + 1, b = lane.bottom - h / 2 - 1;
                if (l > r || t > b) return std::nullopt;
                const Point clamped{std::clamp(q.x, l, r), std::clamp(q.y, t, b)};
                const auto box = label_rect(clamped);
                if (within(q.x, box.left, box.right) && within(q.y, box.top, box.bottom))
                    return std::pair{id, clamped};
                return std::nullopt;
            }
            return std::nullopt;
        };
        const auto allowed = [&](Rect r, bool allow_foreign_edge) {
            if (overlaps(r, start_marker) || overlaps(r, end_marker)) return false;
            for (const auto& [id, other] : placed_labels)
                if (id != *e.label_node && overlaps(r, other)) return false;
            r = inflate(r, 3);
            for (const auto& [id, other] : content)
                if (id != *e.label_node && overlaps(r, other)) return false;
            if (!allow_foreign_edge)
                for (const auto& [id, other] : foreign_edges)
                    if (id != i && (e.id.empty() || edges[id].id != e.id) && overlaps(r, other))
                        return false;
            return true;
        };
        const auto choose = [&](std::span<const double> candidates, bool middle, bool endpoint_clearance,
                                bool allow_foreign_edge) -> std::optional<std::pair<Index, Point>> {
            for (const auto& s : segments) {
                if (middle && (s.index == 0 || s.index >= visible_count - 1)) continue;
                const auto a = p[s.index], b = p[s.index + 1];
                for (const auto t : candidates) {
                    const auto required = (s.horizontal ? w : h) / 2 + 12;
                    if (endpoint_clearance &&
                        ((s.index == first_visible && s.length * t + geometry_eps < required) ||
                         (s.index == last_visible && s.length * (1 - t) + geometry_eps < required)))
                        continue;
                    const auto placement = place({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t});
                    if (placement && allowed(label_rect(placement->second), allow_foreign_edge))
                        return placement;
                }
            }
            return std::nullopt;
        };
        constexpr std::array along{0.5, 0.25, 0.75, 0.05, 0.95, 0.15, 0.85, 0.1, 0.9};
        constexpr std::array middle{0.5};
        auto chosen = visible_count >= 3 ? choose(along, true, true, false) : std::nullopt;
        if (!chosen) chosen = choose(along, false, true, false);
        if (!chosen) chosen = choose(middle, false, true, false);
        if (!chosen) chosen = choose(middle, false, false, false);
        if (!chosen) chosen = choose(middle, false, false, true);
        if (chosen) {
            label.box.x = chosen->second.x;
            label.box.y = chosen->second.y;
            label.parent = chosen->first;
            placed_labels.emplace_back(*e.label_node, label_rect(chosen->second));
        }
        e.placed.label = center(label.box);
    }
}

} // namespace folia::mermaid::swimlane_routing_detail
