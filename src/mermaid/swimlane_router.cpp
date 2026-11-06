// Based on Mermaid 12.2.0 swimlanes/orthogonalRouter/router.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"

namespace folia::mermaid {
namespace {
using namespace swimlane_routing_detail;

constexpr double node_padding = 8;
constexpr double pipe_margin = 15;
constexpr double routing_margin = 25;
constexpr double track_spacing = 10;

Side choose_side(Point from, Point to) {
    const auto dx = to.x - from.x, dy = to.y - from.y;
    if (std::abs(dy) > eps && std::abs(dy) * 3 >= std::abs(dx))
        return dy > 0 ? Side::bottom : Side::top;
    if (std::abs(dx) > eps)
        return dx > 0 ? Side::right : Side::left;
    return Side::bottom;
}

Point clamp_boundary(Point p, const FlowchartPlacedNode& b) {
    const auto r = rect(b);
    if (b.width <= 0 || b.height <= 0 || p.x < r.left || p.x > r.right ||
        p.y < r.top || p.y > r.bottom)
        return p;
    const auto d = std::min({p.x - r.left, r.right - p.x, p.y - r.top, r.bottom - p.y});
    if (d == p.x - r.left) return {r.left, p.y};
    if (d == r.right - p.x) return {r.right, p.y};
    if (d == p.y - r.top) return {p.x, r.top};
    return {p.x, r.bottom};
}

struct End {
    Side side = Side::bottom;
    std::optional<double> offset;
};
struct Ports {
    std::array<End, 2> end;
    double dx = 0, dy = 0;
};
struct Face {
    std::array<std::vector<Index>, 2> role;
    Index size() const { return role[0].size() + role[1].size(); }
};
struct Obstacle { Index node; Rect bounds; double visual_half_width; };
struct Pipe {
    bool vertical;
    double coord;
    std::vector<std::vector<Index>> tracks;
};
struct Segment {
    Index edge, pipe, track = 0;
    double from, to;
};
struct FastSegment { Index edge; bool vertical; double coord, from, to; };

class Router {
public:
    Router(std::span<const SwimlaneRoutingNode> nodes, std::span<SwimlaneRoutingEdge> edges,
           bool lr)
        : nodes(nodes), edges(edges), lr(lr), lanes(nodes.size()), incident(nodes.size()),
          faces(nodes.size()), ports(edges.size()), indices(edges.size()) {
        for (Index i = 0; i < nodes.size(); ++i) {
            auto top = i;
            for (Index step = 0; step < nodes.size() && nodes[top].parent &&
                                 *nodes[top].parent < nodes.size(); ++step)
                top = *nodes[top].parent;
            if (nodes[top].group && !nodes[top].parent) lanes[i] = top;
            const auto& n = nodes[i];
            if (!n.group && !n.edge_label)
                obstacles.push_back({i, rect(n.box, node_padding),
                    (lr ? n.box.height : n.box.width) / 2 + node_padding});
        }
    }

    void run() {
        std::vector<Index> order;
        for (Index i = 0; i < edges.size(); ++i) {
            edges[i].placed = {};
            if (edges[i].start < nodes.size() && edges[i].end < nodes.size() &&
                edges[i].start != edges[i].end)
                order.push_back(i);
        }
        assign_ports(order);
        const auto cross_lane = [&](Index i) {
            const auto a = lanes[edges[i].start], b = lanes[edges[i].end];
            return a && b && a != b;
        };
        // The 1px tie band is intentionally retained. Insertion sorting keeps
        // this tolerance comparison out of C++'s strict-order sort contract.
        const auto before = [&](Index a, Index b) {
            if (cross_lane(a) != cross_lane(b)) return cross_lane(a);
            const auto delta = edge_distance(a) - edge_distance(b);
            return std::abs(delta) > 1 ? delta < 0 : a < b;
        };
        stable_insert_sort(order, before);
        for (const auto i : order) initial_route(i);
        reduce_crossings();
        rebuild();
    }

private:
    template<class T, class Before>
    static void stable_insert_sort(std::vector<T>& values, Before before) {
        for (Index i = 1; i < values.size(); ++i) {
            const auto value = values[i];
            auto j = i;
            while (j && before(value, values[j - 1])) {
                values[j] = values[j - 1];
                --j;
            }
            values[j] = value;
        }
    }

    double edge_distance(Index i) const { return std::abs(ports[i].dx) + std::abs(ports[i].dy); }

    void assign_ports(std::span<const Index> order) {
        using SideKey = std::pair<Index, Side>;
        std::vector<std::pair<SideKey, std::vector<Index>>> source_groups;
        std::map<SideKey, Index> source_group_index;
        std::map<SideKey, Index> load;
        for (const auto i : order) {
            const auto& e = edges[i];
            ++incident[e.start];
            ++incident[e.end];
            const auto a = center(nodes[e.start].box), b = center(nodes[e.end].box);
            auto& p = ports[i];
            p.dx = b.x - a.x;
            p.dy = b.y - a.y;
            p.end[0].side = choose_side(a, b);
            p.end[1].side = choose_side(b, a);
            const auto key = SideKey{e.start, p.end[0].side};
            auto [it, fresh] = source_group_index.try_emplace(key, source_groups.size());
            if (fresh) source_groups.emplace_back(key, std::vector<Index>{});
            source_groups[it->second].second.push_back(i);
            ++load[key];
            ++load[{e.end, p.end[1].side}];
        }
        const auto strength = [&](Index i) {
            const auto& p = ports[i];
            const auto x = std::abs(p.dx), y = std::abs(p.dy);
            return vertical(p.end[0].side) ? (x == 0 ? inf : y / x) : (y == 0 ? inf : x / y);
        };
        const auto secondary = [&](Index i) {
            const auto& p = ports[i];
            return vertical(p.end[0].side) ? (p.dx >= 0 ? Side::right : Side::left)
                                          : (p.dy >= 0 ? Side::bottom : Side::top);
        };
        for (auto& [key, group] : source_groups) {
            stable_insert_sort(group, [&](Index a, Index b) {
                const auto sa = strength(a), sb = strength(b);
                return std::abs(sa - sb) > 1e-9 ? sa > sb : a < b;
            });
            for (Index j = 1; j < group.size(); ++j) {
                const auto i = group[j];
                const auto candidate = SideKey{key.first, secondary(i)};
                if (load[candidate] < load[key]) {
                    --load[key];
                    ++load[candidate];
                    ports[i].end[0].side = candidate.second;
                }
            }
        }
        std::set<SideKey> incoming;
        for (const auto i : order) incoming.emplace(edges[i].end, ports[i].end[1].side);
        for (const auto i : order) {
            const auto a = edges[i].start;
            auto& side = ports[i].end[0].side;
            if (!nodes[a].diamond || !incoming.contains({a, side})) continue;
            const auto candidate = secondary(i);
            if (incoming.contains({a, candidate}) || load[{a, candidate}] > 0) continue;
            if (load[{a, side}]) --load[{a, side}];
            ++load[{a, candidate}];
            side = candidate;
        }
        for (const auto i : order) {
            const auto& e = edges[i];
            faces[e.start][side_index(ports[i].end[0].side)].role[0].push_back(i);
            faces[e.end][side_index(ports[i].end[1].side)].role[1].push_back(i);
        }
        for (Index n = 0; n < faces.size(); ++n) {
            for (Index s = 0; s < 4; ++s) {
                const auto v = vertical(static_cast<Side>(s));
                for (Index role = 0; role < 2; ++role) {
                    auto& group = faces[n][s].role[role];
                    if (group.size() < 2) continue;
                    const auto opposite = [&](Index i) {
                        const auto& b = nodes[role ? edges[i].start : edges[i].end].box;
                        return v ? b.x : b.y;
                    };
                    std::stable_sort(group.begin(), group.end(), [&](Index a, Index b) {
                        return opposite(a) < opposite(b);
                    });
                    const auto side_length = v ? nodes[n].box.width : nodes[n].box.height;
                    const auto effective = side_length * (nodes[n].diamond ? 0.3 : 1);
                    const auto spacing = std::clamp(effective / (group.size() + 1), 8.0, 20.0);
                    for (Index j = 0; j < group.size(); ++j)
                        ports[group[j]].end[role].offset = (j - (group.size() - 1) / 2.0) * spacing;
                }
            }
        }
    }

    bool blocked(Point a, Point b, const SwimlaneRoutingEdge& e) const {
        const auto [min_x, max_x] = std::minmax(a.x, b.x);
        const auto [min_y, max_y] = std::minmax(a.y, b.y);
        return std::ranges::any_of(obstacles, [&](const Obstacle& obs) {
            if (obs.node == e.start || obs.node == e.end) return false;
            const auto r = obs.bounds;
            return std::abs(a.x - b.x) > eps
                ? r.top < a.y && r.bottom > a.y && r.right > min_x && r.left < max_x
                : r.left < a.x && r.right > a.x && r.bottom > min_y && r.top < max_y;
        });
    }

    Index pipe_at(bool v, double coord) {
        const auto found = std::ranges::find_if(pipes, [&](const Pipe& pipe) {
            return pipe.vertical == v && same(pipe.coord, coord, 1);
        });
        if (found != pipes.end()) return static_cast<Index>(found - pipes.begin());
        pipes.push_back({v, coord, {}});
        return pipes.size() - 1;
    }

    double crossing_penalty(Index i, Point a, Point b) const {
        const auto horizontal = same(a.y, b.y);
        if (!horizontal && !same(a.x, b.x)) return 0;
        const auto range = horizontal ? std::minmax(a.x, b.x) : std::minmax(a.y, b.y);
        const auto coord = horizontal ? a.y : a.x;
        double penalty = 0;
        for (const auto& seg : segments) {
            const auto& pipe = pipes[seg.pipe];
            if (seg.edge != i && pipe.vertical == horizontal &&
                pipe.coord >= range.first - eps && pipe.coord <= range.second + eps &&
                seg.from - eps <= coord && seg.to + eps >= coord)
                penalty += 1000;
        }
        for (const auto& seg : fast_segments)
            if (seg.edge != i && seg.vertical == horizontal &&
                seg.coord >= range.first - eps && seg.coord <= range.second + eps &&
                seg.from - eps <= coord && seg.to + eps >= coord)
                penalty += 1000;
        return penalty;
    }

    struct Anchor { Point point; std::vector<Point> waypoints; };

    Anchor make_anchor(Index i, Point p, Side side, bool source) const {
        const auto& e = edges[i];
        const auto& node = nodes[source ? e.start : e.end].box;
        const auto& opposite = nodes[source ? e.end : e.start].box;
        const auto v = vertical(side);
        const auto positive = v ? p.y > node.y : p.x > node.x;
        Anchor result{p, {}};
        (v ? result.point.y : result.point.x) += positive ? anchor_offset : -anchor_offset;
        const auto found = std::ranges::find_if(obstacles, [&](const Obstacle& obs) {
            return obs.node != e.start && obs.node != e.end && inside(result.point, obs.bounds);
        });
        if (found == obstacles.end()) return result;
        const auto r = found->bounds;
        result.point = {
            (v ? opposite.x >= p.x : positive) ? r.right + pipe_margin : r.left - pipe_margin,
            (v ? positive : opposite.y >= p.y) ? r.bottom + pipe_margin : r.top - pipe_margin,
        };
        if (v && source) {
            const auto gap = positive ? std::min(r.top - 2, p.y + anchor_offset)
                                      : std::max(r.bottom + 2, p.y - anchor_offset);
            result.waypoints = {{p.x, gap}, {result.point.x, gap}, result.point};
        } else if (source) {
            const auto gap = positive ? std::min(r.left - 2, p.x + anchor_offset)
                                      : std::max(r.right + 2, p.x - anchor_offset);
            result.waypoints = {{gap, p.y}, {gap, result.point.y}, result.point};
        } else if (v) {
            result.waypoints = {result.point, {p.x, result.point.y}};
        } else {
            result.waypoints = {result.point, {result.point.x, p.y}};
        }
        return result;
    }

    std::vector<Point> search(Index i, Point start, Point end);
    void simplify_detour(Index i, std::vector<Point>& points) const;
    void initial_route(Index i);
    void reduce_crossings();
    void rebuild();

    std::span<const SwimlaneRoutingNode> nodes;
    std::span<SwimlaneRoutingEdge> edges;
    bool lr;
    std::vector<std::optional<Index>> lanes;
    std::vector<Index> incident;
    std::vector<std::array<Face, 4>> faces;
    std::vector<Ports> ports;
    std::vector<Obstacle> obstacles;
    std::vector<Pipe> pipes;
    std::vector<Segment> segments;
    std::vector<FastSegment> fast_segments;
    std::vector<std::vector<Index>> indices;
};

std::vector<Point> Router::search(Index i, Point start, Point end) {
    const auto& e = edges[i];
    const auto [left, right] = std::minmax(start.x, end.x);
    const auto [top, bottom] = std::minmax(start.y, end.y);
    auto bounds = Rect{left - 50, right + 50, top - 50, bottom + 50};
    for (const auto& obs : obstacles) {
        const auto r = obs.bounds;
        if (r.left < right && r.right > left && r.top < bottom && r.bottom > top) {
            bounds.left = std::min(bounds.left, r.left - routing_margin);
            bounds.right = std::max(bounds.right, r.right + routing_margin);
            bounds.top = std::min(bounds.top, r.top - routing_margin);
            bounds.bottom = std::max(bounds.bottom, r.bottom + routing_margin);
        }
    }
    for (const auto& obs : obstacles) {
        const auto r = obs.bounds;
        if (r.right < bounds.left || r.left > bounds.right ||
            r.bottom < bounds.top || r.top > bounds.bottom)
            continue;
        pipe_at(false, r.top - pipe_margin);
        pipe_at(false, r.bottom + pipe_margin);
        pipe_at(true, r.left - pipe_margin);
        pipe_at(true, r.right + pipe_margin);
    }
    pipe_at(false, start.y);
    pipe_at(false, end.y);

    const auto hv = Point{end.x, start.y}, vh = Point{start.x, end.y};
    if (!blocked(start, hv, e) && !blocked(hv, end, e)) {
        if (same(start.x, end.x) || same(start.y, end.y)) return {start, end};
        return {start, hv, end};
    }
    if (!blocked(start, vh, e) && !blocked(vh, end, e)) {
        if (same(start.x, end.x)) return {start, end};
        return {start, vh, end};
    }

    std::vector<double> xs, ys;
    for (const auto& p : pipes) {
        if (p.vertical && p.coord >= bounds.left && p.coord <= bounds.right)
            xs.push_back(p.coord);
        if (!p.vertical && p.coord >= bounds.top && p.coord <= bounds.bottom)
            ys.push_back(p.coord);
    }
    std::ranges::sort(xs);
    std::ranges::sort(ys);

    // Keys match the one-decimal coordinate identity of the visibility grid.
    using Key = std::pair<double, double>;
    const auto key = [](Point p) {
        return Key{std::round(p.x * 10), std::round(p.y * 10)};
    };
    struct State {
        double score = inf;
        std::optional<Point> previous;
        char direction = 'n';
        bool open = false;
    };
    struct Candidate { Point point; double estimate; };
    std::map<Key, State> state;
    std::vector<Candidate> open{{start, std::hypot(end.x - start.x, end.y - start.y)}};
    state[key(start)] = {0, std::nullopt, 'n', true};
    const auto target = key(end);
    while (!open.empty()) {
        std::stable_sort(open.begin(), open.end(), [](const Candidate& a, const Candidate& b) {
            return a.estimate < b.estimate;
        });
        const auto p = open.front().point;
        open.erase(open.begin());
        auto& current = state.at(key(p));
        current.open = false;
        if (key(p) == target) {
            std::vector<Point> result{end};
            for (auto prev = current.previous; prev; prev = state.at(key(*prev)).previous)
                result.push_back(*prev);
            std::ranges::reverse(result);
            return result;
        }
        std::vector<Point> neighbors;
        const auto collect = [&](const std::vector<double>& axis, double value, bool horizontal) {
            const auto it = std::ranges::find_if(axis, [&](double v) { return same(v, value, 1); });
            if (it == axis.end()) return;
            const auto append = [&](double v) {
                neighbors.push_back(horizontal ? Point{v, p.y} : Point{p.x, v});
            };
            if (it != axis.begin()) append(*std::prev(it));
            if (std::next(it) != axis.end()) append(*std::next(it));
        };
        collect(xs, p.x, true);
        collect(ys, p.y, false);
        for (const auto next : neighbors) {
            if (blocked(p, next, e)) continue;
            const auto dx = next.x - p.x, dy = next.y - p.y;
            const auto dx_total = end.x - start.x, dy_total = end.y - start.y;
            double penalty = crossing_penalty(i, p, next);
            if ((dy_total > 10 && dy < -5) || (dy_total < -10 && dy > 5))
                penalty += std::abs(dy) * 100;
            if ((dx_total > 10 && dx < -5) || (dx_total < -10 && dx > 5))
                penalty += std::abs(dx) * 50;
            const auto move = std::abs(dx) > eps ? 'h' : 'v';
            if (current.direction != 'n' && current.direction != move) penalty += 50;
            const auto score = current.score + distance(p, next) + penalty;
            auto& next_state = state[key(next)];
            if (score >= next_state.score) continue;
            const auto estimate = score + distance(next, end);
            if (next_state.open) {
                const auto it = std::ranges::find_if(open, [&](const Candidate& item) {
                    return key(item.point) == key(next);
                });
                if (it != open.end()) it->estimate = estimate;
            } else {
                open.push_back({next, estimate});
            }
            next_state = {score, p, move, true};
        }
    }
    return {start, vh, end};
}

void Router::simplify_detour(Index i, std::vector<Point>& path) const {
    if (path.size() <= 4) return;
    const auto start = path.front(), end = path.back();
    const auto [left, right] = std::minmax(start.x, end.x);
    const auto [top, bottom] = std::minmax(start.y, end.y);
    auto min_x = left, max_x = right;
    for (const auto p : path) {
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
    }
    const auto went_left = min_x < left, went_right = max_x > right;
    if (went_left == went_right) return;
    if (lr) {
        auto visual = went_right ? right : left;
        bool has_obstacle = false;
        for (const auto& obs : obstacles) {
            const auto r = obs.bounds;
            const auto in_x = went_right ? r.left < right && r.right > right
                                         : r.left < left + pipe_margin;
            if (!in_x || r.top >= bottom || r.bottom <= top) continue;
            has_obstacle = true;
            const auto cx = (r.left + r.right) / 2;
            const auto extent = obs.visual_half_width + pipe_margin;
            visual = went_right ? std::max(visual, cx + extent) : std::min(visual, cx - extent);
        }
        if (has_obstacle) (went_right ? max_x : min_x) = visual;
    }
    const auto x = went_right ? max_x : min_x;
    const auto down = end.y > start.y;
    auto obstacle_y = down ? -inf : inf;
    auto detour_y = obstacle_y;
    for (const auto& obs : obstacles) {
        const auto r = obs.bounds;
        if (r.left >= right || r.right <= left || r.top >= bottom || r.bottom <= top) continue;
        obstacle_y = down ? std::max(obstacle_y, r.bottom) : std::min(obstacle_y, r.top);
        if (r.left < x && r.right > x)
            detour_y = down ? std::max(detour_y, r.bottom) : std::min(detour_y, r.top);
    }
    if (lr && std::isfinite(detour_y)) obstacle_y = detour_y;
    const auto proposed_y = obstacle_y + (down ? pipe_margin : -pipe_margin);
    auto y = end.y;
    if (std::isfinite(obstacle_y) && (down ? proposed_y < y - eps : proposed_y > y + eps))
        y = proposed_y;
    const auto first = Point{x, start.y}, second = Point{x, y}, third = Point{end.x, y};
    const auto& e = edges[i];
    if (!blocked(start, first, e) && !blocked(first, second, e) &&
        !blocked(second, third, e) && (y == end.y || !blocked(third, end, e))) {
        path = {start, first, second};
        if (!same(y, end.y)) path.push_back(third);
        path.push_back(end);
    }
}

void Router::initial_route(Index i) {
    auto& e = edges[i];
    const auto& assignments = ports[i].end;
    const auto src = port(nodes[e.start].box, assignments[0].side, assignments[0].offset.value_or(0));
    const auto dst = port(nodes[e.end].box, assignments[1].side, assignments[1].offset.value_or(0));
    auto start = make_anchor(i, src, assignments[0].side, true);
    auto end = make_anchor(i, dst, assignments[1].side, false);
    const auto aligned_x = same(start.point.x, end.point.x, pipe_margin);
    const auto aligned_y = same(start.point.y, end.point.y, pipe_margin);
    const auto& src_face = faces[e.start][side_index(assignments[0].side)];
    const auto& dst_face = faces[e.end][side_index(assignments[1].side)];
    const auto uncontested = [&](const Face& face, Index node) {
        if (face.size() <= 1) return true;
        if (incident[node] > 2) return false;
        for (const auto& group : face.role)
            for (const auto edge : group)
                if (edges[edge].label_node) return false;
        return true;
    };
    if (start.waypoints.empty() && end.waypoints.empty() && (aligned_x || aligned_y) &&
        !assignments[0].offset && !assignments[1].offset &&
        uncontested(src_face, e.start) && uncontested(dst_face, e.end) && !blocked(src, dst, e)) {
        e.placed.points = {src, start.point, end.point, dst};
        const auto [from, to] = aligned_y ? std::minmax(src.x, dst.x) : std::minmax(src.y, dst.y);
        // The fast path registers only as a crossing obstacle, not as a
        // visibility grid pipe or a track eligible for displacement.
        fast_segments.push_back({i, !aligned_y, aligned_y ? src.y : src.x, from, to});
        return;
    }

    start.point.x = pipes[pipe_at(true, start.point.x)].coord;
    end.point.x = pipes[pipe_at(true, end.point.x)].coord;
    auto path = search(i, start.point, end.point);
    simplify_detour(i, path);

    std::vector<Point> full{src};
    full.insert(full.end(), start.waypoints.begin(), start.waypoints.end());
    full.insert(full.end(), path.begin(), path.end());
    full.insert(full.end(), end.waypoints.rbegin(), end.waypoints.rend());
    full.push_back(dst);
    const auto a = full[full.size() - 3], b = full[full.size() - 2], c = full.back();
    const auto overshoots = [](double a, double b, double c) {
        return sign(b - a) != 0 && sign(b - a) == sign(c - a) && std::abs(b - a) > std::abs(c - a);
    };
    if ((same(a.y, b.y) && same(b.y, c.y) && overshoots(a.x, b.x, c.x)) ||
        (same(a.x, b.x) && same(b.x, c.x) && overshoots(a.y, b.y, c.y)))
        full.erase(std::prev(full.end(), 2));

    std::vector<Point> simplified{full[0], full[1]};
    for (Index j = 2; j + 1 < full.size(); ++j) {
        const auto p = simplified.back(), q = full[j], r = full[j + 1];
        const auto same_h = same(p.y, q.y) && same(q.y, r.y) && (q.x > p.x) == (r.x > q.x);
        const auto same_v = same(p.x, q.x) && same(q.x, r.x) && (q.y > p.y) == (r.y > q.y);
        if (!same_h && !same_v) simplified.push_back(q);
    }
    simplified.push_back(full.back());
    for (Index j = 1; j < simplified.size(); ++j) {
        const auto p = simplified[j - 1], q = simplified[j];
        const auto v = same(p.x, q.x);
        const auto [from, to] = v ? std::minmax(p.y, q.y) : std::minmax(p.x, q.x);
        const auto pipe = pipe_at(v, v ? p.x : p.y);
        if (pipes[pipe].tracks.empty()) pipes[pipe].tracks.emplace_back();
        pipes[pipe].tracks[0].push_back(segments.size());
        indices[i].push_back(segments.size());
        segments.push_back({i, pipe, 0, from, to});
    }
}

void Router::reduce_crossings() {
    const auto overlaps = [](const Segment& a, const Segment& b) {
        return a.from < b.to && b.from < a.to;
    };
    const auto crosses = [&](Index ai, Index bi) {
        const auto& a = segments[ai];
        const auto& b = segments[bi];
        if (pipes[a.pipe].vertical == pipes[b.pipe].vertical) return false;
        const auto& v = pipes[a.pipe].vertical ? a : b;
        const auto& h = pipes[a.pipe].vertical ? b : a;
        const auto x = pipes[v.pipe].coord, y = pipes[h.pipe].coord;
        return x > h.from && x < h.to && y > v.from && y < v.to;
    };
    const auto adjacent = [&](Index id) {
        std::vector<Index> result;
        const auto& seq = indices[segments[id].edge];
        const auto it = std::ranges::find(seq, id);
        if (it != seq.begin()) result.push_back(*std::prev(it));
        if (std::next(it) != seq.end()) result.push_back(*std::next(it));
        return result;
    };
    const auto conflict = [&](Index a, Index b) {
        if (segments[a].track == segments[b].track) return overlaps(segments[a], segments[b]);
        for (const auto p : adjacent(a))
            for (const auto q : adjacent(b))
                if (crosses(p, q)) return true;
        return false;
    };
    const auto move = [&](Index s, Index track) {
        auto& seg = segments[s];
        auto& pipe = pipes[seg.pipe];
        std::erase(pipe.tracks[seg.track], s);
        seg.track = track;
        pipe.tracks[track].push_back(s);
    };
    const auto available = [&](const std::vector<Index>& track, Index id, Index ignore) {
        return std::ranges::none_of(track, [&](Index other) {
            return other != ignore && overlaps(segments[id], segments[other]);
        });
    };
    const auto resolve = [&](Index a, Index b, bool chain) {
        auto& first = segments[a];
        auto& second = segments[b];
        auto& pipe = pipes[first.pipe];
        const auto t1 = first.track, t2 = second.track;
        if (available(pipe.tracks[t2], a, b) && available(pipe.tracks[t1], b, a)) {
            if (t1 == t2) {
                // Upstream assigns through both references; the second
                // assignment replaces the first when the track is shared.
                std::erase(pipe.tracks[t1], a);
                std::erase(pipe.tracks[t1], b);
                pipe.tracks[t1].push_back(a);
            } else {
                move(a, t2);
                move(b, t1);
            }
            return;
        }
        Index next = 0;
        while (next < pipe.tracks.size() && !available(pipe.tracks[next], b, b)) ++next;
        if (next == pipe.tracks.size()) pipe.tracks.emplace_back();
        if (chain) {
            for (const auto id : indices[second.edge])
                if (segments[id].pipe == second.pipe) move(id, next);
        } else {
            move(b, next);
        }
    };
    const auto resolve_pairs = [&](std::span<const Index> seq, bool chain) {
        bool changed = false;
        for (Index a = 0; a < seq.size(); ++a) {
            for (Index b = a + 1; b < seq.size(); ++b) {
                const auto s = seq[a], t = seq[b];
                if (segments[s].pipe == segments[t].pipe && conflict(s, t)) {
                    changed = true;
                    resolve(s, t, chain);
                }
            }
        }
        return changed;
    };

    const auto make_groups = [&](bool source) {
        std::vector<std::vector<Index>> groups;
        std::map<Index, Index> group_index;
        for (Index i = 0; i < indices.size(); ++i) {
            if (indices[i].empty()) continue;
            const auto node = source ? edges[i].start : edges[i].end;
            auto [it, fresh] = group_index.try_emplace(node, groups.size());
            if (fresh) groups.emplace_back();
            groups[it->second].push_back(i);
        }
        return groups;
    };
    const auto source_order = [&](Index i) {
        const auto& seq = indices[i];
        const auto base = pipes[segments[seq.front()].pipe].coord;
        auto dest = base;
        for (Index j = 1; j < seq.size(); ++j) {
            const auto& s = segments[seq[j]];
            if (!pipes[s.pipe].vertical) {
                dest = std::abs(s.from - base) > std::abs(s.to - base) ? s.from : s.to;
                break;
            }
        }
        return std::pair{std::abs(dest - base), dest};
    };
    auto source = make_groups(true);
    for (auto& group : source)
        stable_insert_sort(group, [&](Index a, Index b) {
            const auto [dev_a, dest_a] = source_order(a);
            const auto [dev_b, dest_b] = source_order(b);
            if (std::abs(dev_a - dev_b) > 1) return dev_a < dev_b;
            if (std::abs(dest_a - dest_b) > 1) return dest_a < dest_b;
            if (std::abs(edge_distance(a) - edge_distance(b)) > 1)
                return edge_distance(a) > edge_distance(b);
            const auto& a_seq = indices[a];
            const auto& b_seq = indices[b];
            if (a_seq.size() != b_seq.size()) return a_seq.size() < b_seq.size();
            if (a_seq.size() == 1) {
                const auto& s = segments[a_seq.front()];
                const auto& t = segments[b_seq.front()];
                if (std::abs((s.to - s.from) - (t.to - t.from)) > 1)
                    return s.to - s.from < t.to - t.from;
            }
            return false;
        });
    auto target = make_groups(false);
    const auto before_target = [&](Index i) {
        const auto& seq = indices[i];
        if (seq.size() < 2) return 0.0;
        const auto& s = segments[seq[seq.size() - 2]];
        return s.to - s.from;
    };
    for (auto& group : target)
        stable_insert_sort(group, [&](Index a, Index b) {
            const auto delta = before_target(a) - before_target(b);
            return std::abs(delta) > 0.1 ? delta < 0 : a < b;
        });
    for (Index pass = 0; pass < 10; ++pass) {
        bool changed = false;
        for (const auto& group : source) {
            std::vector<Index> handles;
            for (const auto i : group) handles.push_back(indices[i].front());
            changed = resolve_pairs(handles, true) || changed;
        }
        for (const auto& group : target) {
            std::vector<Index> handles;
            for (const auto i : group) handles.push_back(indices[i].back());
            changed = resolve_pairs(handles, true) || changed;
        }
        for (const auto& pipe : pipes) {
            std::vector<Index> seq;
            for (const auto& track : pipe.tracks)
                seq.insert(seq.end(), track.begin(), track.end());
            std::ranges::sort(seq, [&](Index a, Index b) {
                if (segments[a].edge != segments[b].edge) return segments[a].edge < segments[b].edge;
                return a < b;
            });
            changed = resolve_pairs(seq, false) || changed;
        }
        if (!changed) break;
    }
}

void Router::rebuild() {
    std::vector<double> coords;
    coords.reserve(segments.size());
    for (const auto& s : segments) coords.push_back(pipes[s.pipe].coord);
    std::vector<double> dest_delta(edges.size());
    for (Index i = 0; i < edges.size(); ++i) {
        if (indices[i].empty()) continue;
        const auto base = coords[indices[i].front()];
        for (Index j = 1; j < indices[i].size(); ++j) {
            const auto& s = segments[indices[i][j]];
            if (!pipes[s.pipe].vertical) {
                dest_delta[i] = std::abs(s.from - base) > std::abs(s.to - base)
                    ? s.from - base : s.to - base;
                break;
            }
        }
    }
    for (const auto& pipe : pipes) {
        std::vector<Index> seq;
        for (const auto& track : pipe.tracks)
            seq.insert(seq.end(), track.begin(), track.end());
        std::stable_sort(seq.begin(), seq.end(), [&](Index a, Index b) {
            return segments[a].from < segments[b].from;
        });
        for (Index first = 0; first < seq.size();) {
            auto last = first + 1;
            auto end = segments[seq[first]].to;
            while (last < seq.size() && segments[seq[last]].from < end) {
                end = std::max(end, segments[seq[last]].to);
                ++last;
            }
            const auto cluster = std::span<const Index>{seq}.subspan(first, last - first);
            std::vector<Index> used, left, neutral, right;
            std::vector<double> scores(pipe.tracks.size());
            for (const auto id : cluster) {
                const auto& s = segments[id];
                if (std::ranges::find(used, s.track) == used.end()) used.push_back(s.track);
                scores[s.track] += dest_delta[s.edge];
            }
            for (const auto t : used)
                (scores[t] < -1 ? left : scores[t] > 1 ? right : neutral).push_back(t);
            std::stable_sort(left.begin(), left.end(), [&](Index a, Index b) { return scores[a] > scores[b]; });
            std::stable_sort(right.begin(), right.end(), [&](Index a, Index b) { return scores[a] < scores[b]; });
            const auto assign = [&](Index track, double x) {
                for (const auto s : cluster)
                    if (segments[s].track == track) coords[s] = x;
            };
            for (Index j = 0; j < left.size(); ++j)
                assign(left[j], pipe.coord - (j + 1) * track_spacing);
            if (neutral.empty()) {
                const auto best = *std::ranges::min_element(used, {}, [&](Index t) {
                    return std::abs(scores[t]);
                });
                std::erase(left, best);
                std::erase(right, best);
                neutral.push_back(best);
            }
            for (Index j = 0; j < neutral.size(); ++j) {
                const auto offset = static_cast<double>((j + 1) / 2) * (j % 2 ? 1 : -1);
                assign(neutral[j], pipe.coord + offset * track_spacing / 2);
            }
            for (Index j = 0; j < right.size(); ++j)
                assign(right[j], pipe.coord + (j + 1) * track_spacing);
            first = last;
        }
    }

    for (Index i = 0; i < edges.size(); ++i) {
        auto& e = edges[i];
        auto& points = e.placed.points;
        const auto& seq = indices[i];
        if (!seq.empty()) {
            const auto& p = ports[i].end;
            const auto src = port(nodes[e.start].box, p[0].side, p[0].offset.value_or(0));
            const auto dst = port(nodes[e.end].box, p[1].side, p[1].offset.value_or(0));
            points.push_back(src);
            for (Index j = 0; j < seq.size(); ++j) {
                const auto index = seq[j];
                const auto& line = segments[index];
                const auto v = pipes[line.pipe].vertical;
                const auto c = coords[index];
                const auto prev = points.back();
                const auto along = v ? prev.y : prev.x;
                const auto across = v ? prev.x : prev.y;
                const auto on_line = [&](double a, double coord) {
                    return v ? Point{coord, a} : Point{a, coord};
                };
                if (std::abs(across - c) > eps) points.push_back(on_line(along, c));
                if (j + 1 < seq.size()) {
                    const auto ni = seq[j + 1];
                    const auto& next = segments[ni];
                    const auto nc = coords[ni];
                    const auto shared = same(line.to, next.from) || same(line.to, next.to)
                        ? line.to : line.from;
                    if (pipes[next.pipe].vertical != v) {
                        points.push_back(on_line(nc, c));
                    } else if (std::abs(nc - c) > eps) {
                        const auto junction = v ? (along + next.from) / 2 : shared;
                        points.push_back(on_line(junction, c));
                        points.push_back(on_line(junction, nc));
                    } else if (j == 0 || j + 2 == seq.size()) {
                        points.push_back(on_line(shared, c));
                    }
                } else {
                    const auto end = std::abs(line.from - along) < std::abs(line.to - along)
                        ? line.to : line.from;
                    points.push_back(on_line(end, c));
                }
            }
            if (std::abs(points.back().x - dst.x) > eps ||
                std::abs(points.back().y - dst.y) > eps)
                points.push_back(dst);
            std::vector<Point> filtered;
            for (const auto point : points)
                if (filtered.empty() || std::abs(filtered.back().x - point.x) > eps ||
                    std::abs(filtered.back().y - point.y) > eps)
                    filtered.push_back(point);
            points = std::move(filtered);
        }
        if (points.size() >= 2) {
            points.front() = clamp_boundary(points.front(), nodes[e.start].box);
            points.back() = clamp_boundary(points.back(), nodes[e.end].box);
        }
    }
}

} // namespace

void route_swimlane_edges(std::span<const SwimlaneRoutingNode> nodes,
                         std::span<SwimlaneRoutingEdge> edges,
                         std::u16string_view direction) {
    Router(nodes, edges, direction == u"LR").run();
}

} // namespace folia::mermaid
