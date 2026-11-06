// Based on Mermaid 12.2.0 swimlanes/direction/geometry.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"

namespace folia::mermaid::swimlane_routing_detail {

namespace {

std::vector<Point> simplify_vertices(std::vector<Point> work) {
    const auto between = [](double p, double a, double b) {
        return p > std::min(a, b) + geometry_eps && p < std::max(a, b) - geometry_eps;
    };
    for (Index iteration = 0; iteration < 32 && work.size() >= 3; ++iteration) {
        std::vector<Point> next;
        for (Index i = 0; i < work.size(); ++i) {
            if (!next.empty() && i + 1 < work.size()) {
                const auto p = next.back(), q = work[i], r = work[i + 1];
                if (same(p, r, geometry_eps)) {
                    ++i;
                    continue;
                }
                if ((same(p.x, q.x, geometry_eps) && same(q.x, r.x, geometry_eps) &&
                     between(q.y, p.y, r.y)) ||
                    (same(p.y, q.y, geometry_eps) && same(q.y, r.y, geometry_eps) &&
                     between(q.x, p.x, r.x)))
                    continue;
            }
            next.push_back(work[i]);
        }
        if (next.size() == work.size()) break;
        work = std::move(next);
    }
    return work;
}

} // namespace

Point port(const FlowchartPlacedNode& b, Side side, double offset) {
    const auto r = rect(b);
    switch (side) {
        case Side::top: return {b.x + offset, r.top};
        case Side::bottom: return {b.x + offset, r.bottom};
        case Side::left: return {r.left, b.y + offset};
        case Side::right: return {r.right, b.y + offset};
    }
    std::unreachable();
}

std::vector<Point> simplify(std::vector<Point> points) {
    if (points.empty()) return points;
    std::vector<Point> work{points.front()};
    for (Index i = 1; i < points.size(); ++i) {
        const auto p = work.back(), q = points[i];
        if (!same(p.x, q.x, geometry_eps) && !same(p.y, q.y, geometry_eps)) {
            const auto v = work.size() >= 2 && same(work[work.size() - 2].x, p.x, geometry_eps);
            work.push_back(v ? Point{p.x, q.y} : Point{q.x, p.y});
        }
        if (!same(work.back(), q, geometry_eps)) work.push_back(q);
    }
    return simplify_vertices(std::move(work));
}

Rect inflate(Rect r, double amount) {
    return {r.left - amount, r.right + amount, r.top - amount, r.bottom + amount};
}

bool within(double v, double lo, double hi) {
    return v >= lo - geometry_eps && v <= hi + geometry_eps;
}

bool overlaps(Rect a, Rect b) {
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}
bool contains(Rect a, Rect b) {
    return a.left <= b.left && a.right >= b.right && a.top <= b.top && a.bottom >= b.bottom;
}
Rect segment_bounds(Point a, Point b) {
    return {std::min(a.x, b.x), std::max(a.x, b.x), std::min(a.y, b.y), std::max(a.y, b.y)};
}
double overlap_length(double a, double b, double c, double d) {
    return std::max(0.0, std::min(std::max(a, b), std::max(c, d)) -
                              std::max(std::min(a, b), std::min(c, d)));
}

std::vector<Point> distinct_points(std::span<const Point> points) {
    std::vector<Point> result;
    for (const auto p : points)
        if (result.empty() || !same(result.back(), p, geometry_eps)) result.push_back(p);
    return result;
}
std::vector<Point> simplify_candidate(std::span<const Point> points) {
    return simplify_vertices(distinct_points(points));
}
std::vector<TrackSegment> tracks_for(Index edge, std::span<const Point> points,
                                    double tolerance) {
    std::vector<TrackSegment> result;
    for (Index i = 1; i < points.size(); ++i) {
        const auto a = points[i - 1], b = points[i];
        const auto h = same(a.y, b.y, tolerance), v = same(a.x, b.x, tolerance);
        if (h && std::abs(a.x - b.x) > tolerance) result.push_back({edge, i - 1, a, b, true});
        else if (v && std::abs(a.y - b.y) > tolerance) result.push_back({edge, i - 1, a, b, false});
    }
    return result;
}
bool strictly_crosses(const TrackSegment& a, const TrackSegment& b) {
    if (a.horizontal == b.horizontal) return false;
    const auto& h = a.horizontal ? a : b;
    const auto& v = a.horizontal ? b : a;
    const auto x = v.a.x, y = h.a.y;
    return x > std::min(h.a.x, h.b.x) + geometry_eps &&
           x < std::max(h.a.x, h.b.x) - geometry_eps &&
           y > std::min(v.a.y, v.b.y) + geometry_eps &&
           y < std::max(v.a.y, v.b.y) - geometry_eps;
}

bool crosses_at_segment(const TrackSegment& a, const TrackSegment& b, double tolerance) {
    if (a.horizontal == b.horizontal) return false;
    const auto& h = a.horizontal ? a : b;
    const auto& v = a.horizontal ? b : a;
    const auto hr = segment_bounds(h.a, h.b), vr = segment_bounds(v.a, v.b);
    const Point hit{v.a.x, h.a.y};
    return hit.x >= hr.left && hit.x <= hr.right && hit.y >= vr.top && hit.y <= vr.bottom &&
        !((same(hit, a.a, tolerance) || same(hit, a.b, tolerance)) &&
          (same(hit, b.a, tolerance) || same(hit, b.b, tolerance)));
}

double shared_axis_length(const TrackSegment& a, const TrackSegment& b, double tolerance) {
    if (a.horizontal != b.horizontal) return 0;
    if (a.horizontal)
        return same(a.a.y, b.a.y, tolerance) ? overlap_length(a.a.x, a.b.x, b.a.x, b.b.x) : 0;
    return same(a.a.x, b.a.x, tolerance) ? overlap_length(a.a.y, a.b.y, b.a.y, b.b.y) : 0;
}

bool real_node(const SwimlaneRoutingNode& node) {
    return !node.group && !node.edge_label && node.box.width > 0 && node.box.height > 0;
}

bool line_hits_real_node(Point a, Point b, std::span<const SwimlaneRoutingNode> nodes,
                         Index exclude_start, Index exclude_end, double margin) {
    const auto bounds = segment_bounds(a, b);
    for (Index i = 0; i < nodes.size(); ++i) {
        if (i != exclude_start && i != exclude_end && real_node(nodes[i]) &&
            overlaps(bounds, inflate(rect(nodes[i].box), margin)))
            return true;
    }
    return false;
}

std::vector<Point> same_side_path(Point a, Side side, Point b, double track) {
    if (vertical(side)) return {a, {a.x, track}, {b.x, track}, b};
    return {a, {track, a.y}, {track, b.y}, b};
}

std::vector<Point> port_path(Point a, Side start, Point b, Side end, double anchor) {
    const auto face_towards = [](Side side, Point a, Point b) {
        switch (side) {
            case Side::top: return b.y < a.y;
            case Side::bottom: return b.y > a.y;
            case Side::left: return b.x < a.x;
            case Side::right: return b.x > a.x;
        }
        std::unreachable();
    };
    if (vertical(start) != vertical(end)) {
        if (!face_towards(start, a, b) || !face_towards(end, b, a)) return {};
        return {a, vertical(start) ? Point{a.x, b.y} : Point{b.x, a.y}, b};
    }
    const auto aligned = vertical(start) ? same(a.x, b.x, geometry_eps)
                                         : same(a.y, b.y, geometry_eps);
    const auto aa = vertical(start) ? a.y : a.x;
    const auto bb = vertical(start) ? b.y : b.x;
    if (start == end) {
        if (aligned) return {};
        const auto negative = start == Side::top || start == Side::left;
        return same_side_path(a, start, b, negative ? std::min(aa, bb) - anchor
                                                   : std::max(aa, bb) + anchor);
    }
    if (!face_towards(start, a, b) || !face_towards(end, b, a)) return {};
    return aligned ? std::vector<Point>{a, b} : same_side_path(a, start, b, (aa + bb) / 2);
}

bool incident_edges(const SwimlaneRoutingEdge& a, const SwimlaneRoutingEdge& b) {
    return a.start == b.start || a.start == b.end || a.end == b.start || a.end == b.end;
}

Index bends(std::span<const TrackSegment> segments) {
    Index count = 0;
    for (Index i = 1; i < segments.size(); ++i)
        if (segments[i].horizontal != segments[i - 1].horizontal) ++count;
    return count;
}

} // namespace folia::mermaid::swimlane_routing_detail
