// Based on Mermaid 12.2.0 swimlanes/direction/endpointClip.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"
#include "swimlane_router_phases.inc"

namespace folia::mermaid::swimlane_routing_detail {

namespace {

void clip_start(std::vector<Point>& p, Rect r) {
    const auto interior = inflate(r, -0.5);
    const auto it = std::ranges::find_if(p, [&](Point q) { return !inside(q, interior); });
    if (it == p.begin() || it == p.end()) return;
    const auto outside = *it, inside = *std::prev(it);
    Point entry;
    if (same(outside.y, inside.y, geometry_eps))
        entry = {outside.x < r.left ? r.left : r.right, outside.y};
    else if (same(outside.x, inside.x, geometry_eps))
        entry = {outside.x, outside.y < r.top ? r.top : r.bottom};
    else
        entry = {std::clamp(outside.x, r.left, r.right), std::clamp(outside.y, r.top, r.bottom)};
    const auto pos = p.erase(p.begin(), it);
    p.insert(pos, entry);
}

std::optional<Side> terminal_side(Point p, Point adjacent, Rect r) {
    if (same(p.y, adjacent.y, geometry_eps) && within(p.y, r.top, r.bottom)) {
        if (same(p.x, r.left, geometry_eps)) return Side::left;
        if (same(p.x, r.right, geometry_eps)) return Side::right;
    }
    if (same(p.x, adjacent.x, geometry_eps) && within(p.x, r.left, r.right)) {
        if (same(p.y, r.top, geometry_eps)) return Side::top;
        if (same(p.y, r.bottom, geometry_eps)) return Side::bottom;
    }
    return std::nullopt;
}
std::pair<double, double> clearance(Rect r, bool horizontal) {
    auto lo = horizontal ? r.top : r.left, hi = horizontal ? r.bottom : r.right;
    if (hi - lo < 8) return {(lo + hi) / 2, (lo + hi) / 2};
    return {lo + 4, hi - 4};
}
void clear_straight_corners(std::vector<Point>& points, Rect src, Rect dst) {
    if (points.size() != 2) return;
    const auto a = points.front(), b = points.back();
    const auto h = same(a.y, b.y, geometry_eps);
    if (!h && !same(a.x, b.x, geometry_eps)) return;
    double lo = -inf, hi = inf;
    const auto update = [&](Point endpoint, Point adjacent, Rect r) {
        const auto side = terminal_side(endpoint, adjacent, r);
        if (!side || vertical(*side) == h) return;
        const auto [l, u] = clearance(r, h);
        lo = std::max(lo, l);
        hi = std::min(hi, u);
    };
    update(a, b, src);
    update(b, a, dst);
    if (lo > hi) return;
    const auto value = std::clamp(h ? a.y : a.x, lo, hi);
    for (auto& p : points) (h ? p.y : p.x) = value;
}

void snap_start(std::vector<Point>& points, Rect r, bool destination) {
    if (points.size() < 2) return;
    auto& p = points.front();
    const auto q = points[1];
    if (same(p.y, q.y, geometry_eps) && within(p.y, r.top, r.bottom)) {
        auto to_left = std::abs(p.x - r.left) <= std::abs(p.x - r.right);
        if (destination && (q.x < r.left - geometry_eps || q.x > r.right + geometry_eps))
            to_left = q.x < r.left;
        p = {to_left ? r.left : r.right, q.y};
    } else if (same(p.x, q.x, geometry_eps) && within(p.x, r.left, r.right)) {
        auto to_top = std::abs(p.y - r.top) <= std::abs(p.y - r.bottom);
        if (destination && (q.y < r.top - geometry_eps || q.y > r.bottom + geometry_eps))
            to_top = q.y < r.top;
        p = {q.x, to_top ? r.top : r.bottom};
    }
    if (points.size() < 3) return;
    const auto next = points[2];
    const auto vertical_stub = within(p.y, r.top, r.bottom) && within(q.y, r.top, r.bottom) &&
        same(q.y, next.y, geometry_eps) &&
        ((same(p.x, r.left, geometry_eps) && same(q.x, r.left, geometry_eps) &&
          next.x < r.left - geometry_eps) ||
         (same(p.x, r.right, geometry_eps) && same(q.x, r.right, geometry_eps) &&
          next.x > r.right + geometry_eps));
    const auto horizontal_stub = within(p.x, r.left, r.right) && within(q.x, r.left, r.right) &&
        same(q.x, next.x, geometry_eps) &&
        ((same(p.y, r.top, geometry_eps) && same(q.y, r.top, geometry_eps) &&
          next.y < r.top - geometry_eps) ||
         (same(p.y, r.bottom, geometry_eps) && same(q.y, r.bottom, geometry_eps) &&
          next.y > r.bottom + geometry_eps));
    if (vertical_stub || horizontal_stub) points.erase(points.begin());
}

void clear_corner(std::vector<Point>& points, Rect r) {
    if (points.size() < 2) return;
    const auto p = points.front();
    const auto adjacent = std::ranges::find_if(points, [&](Point q) {
        return !same(p, q, geometry_eps);
    });
    if (adjacent == points.end()) return;
    const auto side = terminal_side(p, *adjacent, r);
    if (!side) return;
    const auto h = !vertical(*side);
    const auto [lo, hi] = clearance(r, h);
    const auto old = h ? p.y : p.x;
    const auto next = std::clamp(old, lo, hi);
    if (same(old, next, geometry_eps)) return;
    for (auto& q : points) {
        auto& coordinate = h ? q.y : q.x;
        if (!same(coordinate, old, geometry_eps)) break;
        coordinate = next;
    }
}

} // namespace

void clip_endpoints(std::span<const SwimlaneRoutingNode> nodes,
                    std::span<SwimlaneRoutingEdge> edges) {
    for (auto& e : edges) {
        auto& points = e.placed.points;
        if (points.size() < 2 || e.start >= nodes.size() || e.end >= nodes.size()) continue;
        const auto src = rect(nodes[e.start].box), dst = rect(nodes[e.end].box);
        clip_start(points, src);
        std::ranges::reverse(points);
        clip_start(points, dst);
        std::ranges::reverse(points);
        points = simplify(std::move(points));
        clear_straight_corners(points, src, dst);
        points = simplify(std::move(points));
    }
}

void prepare_endpoints(std::span<const SwimlaneRoutingNode> nodes,
                       std::span<SwimlaneRoutingEdge> edges) {
    for (auto& e : edges) {
        auto& points = e.placed.points;
        if (points.size() < 2 || e.start >= nodes.size() || e.end >= nodes.size()) continue;
        const auto src = rect(nodes[e.start].box), dst = rect(nodes[e.end].box);
        points = distinct_points(points);
        snap_start(points, src, false);
        std::ranges::reverse(points);
        snap_start(points, dst, true);
        std::ranges::reverse(points);
        if (points.size() == 2) {
            clear_straight_corners(points, src, dst);
        } else {
            clear_corner(points, src);
            std::ranges::reverse(points);
            clear_corner(points, dst);
            std::ranges::reverse(points);
        }
        if (points.size() >= 3) {
            const auto first = points.front(), last = points.back();
            points.insert(points.begin(), first);
            points.push_back(last);
        }
    }
}

} // namespace folia::mermaid::swimlane_routing_detail
