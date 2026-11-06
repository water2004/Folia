// Based on Mermaid 12.2.0 swimlanes/orthogonalRouter/router.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

namespace folia::mermaid {
namespace {
using Point = FlowchartPoint;
using Index = std::size_t;
constexpr double eps = 1e-6;
constexpr double geometry_eps = 1e-3;
constexpr double node_padding = 8;
constexpr double pipe_margin = 15;
constexpr double routing_margin = 25;
constexpr double anchor_offset = 20;
constexpr double track_spacing = 10;
constexpr double inf = std::numeric_limits<double>::infinity();

enum class Side { top, bottom, left, right };
bool vertical(Side side) { return side == Side::top || side == Side::bottom; }
Index side_index(Side side) { return static_cast<Index>(side); }
bool same(double a, double b, double tolerance = eps) { return std::abs(a - b) < tolerance; }
bool same(Point a, Point b, double tolerance = eps) {
    return same(a.x, b.x, tolerance) && same(a.y, b.y, tolerance);
}
int sign(double value) { return (value > 0) - (value < 0); }
double distance(Point a, Point b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }
Point center(const FlowchartPlacedNode& box) { return {box.x, box.y}; }

struct Rect { double left, right, top, bottom; };
Rect rect(const FlowchartPlacedNode& b, double margin = 0) {
    return {b.x - b.width / 2 - margin, b.x + b.width / 2 + margin,
            b.y - b.height / 2 - margin, b.y + b.height / 2 + margin};
}
bool inside(Point p, Rect r) {
    return p.x > r.left && p.x < r.right && p.y > r.top && p.y < r.bottom;
}

Side choose_side(Point from, Point to) {
    const auto dx = to.x - from.x, dy = to.y - from.y;
    if (std::abs(dy) > eps && std::abs(dy) * 3 >= std::abs(dx))
        return dy > 0 ? Side::bottom : Side::top;
    if (std::abs(dx) > eps)
        return dx > 0 ? Side::right : Side::left;
    return Side::bottom;
}
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

bool within(double v, double lo, double hi) {
    return v >= lo - geometry_eps && v <= hi + geometry_eps;
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

struct TrackSegment {
    Index edge, index;
    Point a, b;
    bool horizontal;
};
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
                                    double tolerance = geometry_eps) {
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
bool crowded_track(const TrackSegment& a, const TrackSegment& b) {
    if (a.horizontal != b.horizontal) return false;
    if (a.horizontal)
        return overlap_length(a.a.x, a.b.x, b.a.x, b.b.x) >= 8 && same(a.a.y, b.a.y, 7);
    return overlap_length(a.a.y, a.b.y, b.a.y, b.b.y) >= 8 && same(a.a.x, b.a.x, 7);
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

constexpr std::array cardinal_sides{Side::top, Side::bottom, Side::left, Side::right};

Side nearest_side(Point p, Rect r) {
    const std::array distances{std::abs(p.y - r.top), std::abs(p.y - r.bottom),
                               std::abs(p.x - r.left), std::abs(p.x - r.right)};
    return cardinal_sides[std::ranges::min_element(distances) - distances.begin()];
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

using RouteSegments = std::vector<std::vector<TrackSegment>>;

Point intersect_rectangle(const FlowchartPlacedNode& node, Point p) {
    const auto dx = p.x - node.x, dy = p.y - node.y;
    auto w = node.width / 2, h = node.height / 2;
    if (std::abs(dy) * w > std::abs(dx) * h) {
        if (dy < 0) h = -h;
        return {node.x + (dy == 0 ? 0 : h * dx / dy), node.y + h};
    }
    if (dx < 0) w = -w;
    return {node.x + w, node.y + (dx == 0 ? 0 : w * dy / dx)};
}

// materializedGeometry.ts: separate terminal rails that coincide after
// projecting renderer endpoints onto their shared node face.
void separate_terminal_lanes(std::span<const SwimlaneRoutingNode> nodes,
                             std::span<SwimlaneRoutingEdge> edges) {
    struct Lane {
        Index edge, node;
        bool source, horizontal;
        double coord, min, max;
        Point boundary, rail;
        Rect box;
    };
    const auto aligned = [](Point a, Point b) {
        return same(a.x, b.x, geometry_eps) || same(a.y, b.y, geometry_eps);
    };
    const auto lane_for = [&](Index i, bool source, std::vector<Point> points) -> std::optional<Lane> {
        const auto& e = edges[i];
        const auto node = source ? e.start : e.end;
        if (node >= nodes.size() || points.size() < 2) return std::nullopt;
        const auto& n = nodes[node].box;
        if (n.width <= 0 || n.height <= 0) return std::nullopt;
        if (!source) std::ranges::reverse(points);
        const auto boundary = intersect_rectangle(n, points[0]);
        const auto rail = aligned(boundary, points[1]) ? points[1] : points[0];
        if (same(boundary.x, rail.x, geometry_eps))
            return Lane{i, node, source, false, boundary.x, std::min(boundary.y, rail.y),
                        std::max(boundary.y, rail.y), boundary, rail, rect(n)};
        if (same(boundary.y, rail.y, geometry_eps))
            return Lane{i, node, source, true, boundary.y, std::min(boundary.x, rail.x),
                        std::max(boundary.x, rail.x), boundary, rail, rect(n)};
        return std::nullopt;
    };
    const auto overlap = [](const Lane& a, const Lane& b) {
        return std::min(a.max, b.max) - std::max(a.min, b.min);
    };
    const auto exact = [&](const Lane& a, const Lane& b) {
        return a.node == b.node && a.horizontal == b.horizontal &&
               overlap(a, b) >= 8 && same(a.coord, b.coord, 0.5);
    };
    const auto near = [&](const Lane& a, const Lane& b) {
        if (a.node != b.node || !a.horizontal || !b.horizontal || a.source == b.source) return false;
        const auto shared = overlap(a, b);
        const auto span = a.box.bottom - a.box.top;
        return shared >= 8 && shared >= span && shared <= 2 * span &&
               (same(a.boundary.x, a.box.left, 1) || same(a.boundary.x, a.box.right, 1)) &&
               same(a.boundary.x, b.boundary.x, 1) && same(a.coord, b.coord, 16);
    };
    const auto shifted = [&](const Lane& lane, double shift) {
        auto points = distinct_points(edges[lane.edge].placed.points);
        if (!lane.source) std::ranges::reverse(points);
        auto boundary = lane.boundary, rail = lane.rail;
        (lane.horizontal ? boundary.y : boundary.x) += shift;
        (lane.horizontal ? rail.y : rail.x) += shift;
        const auto r = lane.box;
        if (same(lane.boundary.y, r.top, 1) || same(lane.boundary.y, r.bottom, 1)) {
            if (!same(boundary.y, lane.boundary.y, geometry_eps) ||
                boundary.x < r.left + 1 || boundary.x > r.right - 1)
                return std::vector<Point>{};
        } else if (same(lane.boundary.x, r.left, 1) || same(lane.boundary.x, r.right, 1)) {
            if (!same(boundary.x, lane.boundary.x, geometry_eps) ||
                boundary.y < r.top + 1 || boundary.y > r.bottom - 1)
                return std::vector<Point>{};
        } else {
            return std::vector<Point>{};
        }
        const auto from = same(points[1], lane.rail, geometry_eps) ? 2 : 1;
        if (points.size() > from && !aligned(points[from], rail)) return std::vector<Point>{};
        std::vector<Point> candidate{boundary, rail};
        candidate.insert(candidate.end(), points.begin() + from, points.end());
        if (!lane.source) std::ranges::reverse(candidate);
        return candidate;
    };
    const auto straight = [&](const Lane& lane) {
        const auto& edge = edges[lane.edge];
        if (edge.start >= nodes.size() || edge.end >= nodes.size()) return false;
        const auto points = distinct_points(edge.placed.points);
        if (points.size() != 2) return false;
        const auto a = center(nodes[edge.start].box), b = center(nodes[edge.end].box);
        return (same(points[0].y, points[1].y, geometry_eps) && same(a.y, b.y, 1) &&
                std::abs(a.x - b.x) > 1) ||
               (same(points[0].x, points[1].x, geometry_eps) && same(a.x, b.x, 1) &&
                std::abs(a.y - b.y) > 1);
    };
    for (Index iteration = 0; iteration < 8; ++iteration) {
        std::vector<Lane> lanes;
        for (Index i = 0; i < edges.size(); ++i) {
            for (const auto source : {true, false}) {
                const auto lane = lane_for(i, source, distinct_points(edges[i].placed.points));
                if (lane) lanes.push_back(*lane);
            }
        }
        bool fixed = false;
        for (Index i = 0; i < lanes.size() && !fixed; ++i) {
            for (Index j = i + 1; j < lanes.size() && !fixed; ++j) {
                const auto a = lanes[i], b = lanes[j];
                if (a.edge == b.edge) continue;
                const auto is_exact = exact(a, b);
                if (!is_exact && !near(a, b)) continue;
                std::array candidates{a, b};
                if (std::pair{straight(b), b.source} < std::pair{straight(a), a.source})
                    std::ranges::reverse(candidates);
                for (const auto& lane : candidates) {
                    for (const auto shift : {-7., 7., -14., 14., -21., 21.}) {
                        auto candidate = shifted(lane, shift);
                        const auto next = lane_for(lane.edge, lane.source, distinct_points(candidate));
                        if (!next || std::ranges::any_of(lanes, [&](const Lane& other) {
                                return other.edge != lane.edge &&
                                    (exact(*next, other) || (!is_exact && near(*next, other)));
                            }))
                            continue;
                        edges[lane.edge].placed.points = std::move(candidate);
                        fixed = true;
                        break;
                    }
                    if (fixed) break;
                }
            }
        }
        if (!fixed) return;
    }
}

RouteSegments materialized_segments(std::span<const SwimlaneRoutingEdge> edges) {
    RouteSegments routes;
    for (Index i = 0; i < edges.size(); ++i)
        routes.push_back(tracks_for(i, distinct_points(edges[i].placed.points)));
    return routes;
}

Index crossing_count(std::span<const TrackSegment> candidate, const RouteSegments& routes,
                     Index edge) {
    Index crossings = 0;
    for (Index i = 0; i < routes.size(); ++i) {
        if (i == edge) continue;
        for (const auto& a : candidate)
            for (const auto& b : routes[i])
                if (strictly_crosses(a, b)) ++crossings;
    }
    return crossings;
}

bool materialized_path_clear(std::span<const SwimlaneRoutingNode> nodes,
                             const SwimlaneRoutingEdge& edge,
                             std::span<const TrackSegment> path) {
    for (const auto& s : path) {
        const auto bounds = segment_bounds(s.a, s.b);
        for (Index i = 0; i < nodes.size(); ++i) {
            const auto& n = nodes[i];
            if (n.group || n.box.width <= 0 || n.box.height <= 0) continue;
            if (!n.edge_label && (i == edge.start || i == edge.end)) continue;
            if (overlaps(bounds, inflate(rect(n.box), 2))) return false;
        }
    }
    return true;
}

bool shares_materialized_rail(std::span<const TrackSegment> candidate,
                              const RouteSegments& routes, Index edge) {
    for (Index i = 0; i < routes.size(); ++i) {
        if (i == edge) continue;
        for (const auto& a : candidate)
            for (const auto& b : routes[i])
                if (shared_axis_length(a, b, 0.5) >= 8) return true;
    }
    return false;
}

// materializedGeometry.ts: remove rectangular doglegs only when the
// remaining route has no obstacle, shared rail, or crossing.
void collapse_rectangular_doglegs(std::span<const SwimlaneRoutingNode> nodes,
                                  std::span<SwimlaneRoutingEdge> edges) {
    for (Index iteration = 0; iteration < 8; ++iteration) {
        const auto routes = materialized_segments(edges);
        bool fixed = false;
        for (Index i = 0; i < edges.size() && !fixed; ++i) {
            auto& edge = edges[i];
            const auto points = distinct_points(edge.placed.points);
            for (Index j = 0; j + 4 < points.size(); ++j) {
                const auto drop_end = [&](bool vertical_dogleg) -> Index {
                    const auto axis = [&](Index k) {
                        return vertical_dogleg ? points[j + k].x : points[j + k].y;
                    };
                    const auto h = [&](Index k) {
                        const auto a = points[j + k], b = points[j + k + 1];
                        return vertical_dogleg
                            ? same(a.y, b.y, geometry_eps) && std::abs(a.x - b.x) > geometry_eps
                            : same(a.x, b.x, geometry_eps) && std::abs(a.y - b.y) > geometry_eps;
                    };
                    const auto v = [&](Index k) {
                        const auto a = points[j + k], b = points[j + k + 1];
                        return vertical_dogleg
                            ? same(a.x, b.x, geometry_eps) && std::abs(a.y - b.y) > geometry_eps
                            : same(a.y, b.y, geometry_eps) && std::abs(a.x - b.x) > geometry_eps;
                    };
                    if (h(0) && v(1) && h(2) && v(3) &&
                        same(axis(0), axis(3), geometry_eps) && same(axis(0), axis(4), geometry_eps) &&
                        same(axis(1), axis(2), geometry_eps) &&
                        (axis(1) - axis(0)) * (axis(3) - axis(2)) < 0)
                        return 4;
                    if (j + 5 < points.size() && v(0) && h(1) && v(2) && h(3) && v(4) &&
                        same(axis(0), axis(4), geometry_eps) && same(axis(0), axis(5), geometry_eps) &&
                        same(axis(2), axis(3), geometry_eps) &&
                        (axis(2) - axis(1)) * (axis(4) - axis(3)) < 0)
                        return 5;
                    return 0;
                };
                const auto first = drop_end(true), second = drop_end(false);
                const auto advance = first == 4 || second == 4 ? 4 : std::max(first, second);
                if (!advance) continue;
                auto candidate = points;
                candidate.erase(candidate.begin() + j + 1, candidate.begin() + j + advance);
                candidate = distinct_points(candidate);
                const auto path = tracks_for(i, candidate);
                if (path.size() + 1 != candidate.size() || !materialized_path_clear(nodes, edge, path) ||
                    shares_materialized_rail(path, routes, i) || crossing_count(path, routes, i))
                    continue;
                edge.placed.points = std::move(candidate);
                fixed = true;
                break;
            }
        }
        if (!fixed) return;
    }
}

// materializedGeometry.ts: lift a middle rail out of an obstacle's border
// without adding a crossing or sharing a different edge's rail.
void lift_obstacle_hugging_rails(std::span<const SwimlaneRoutingNode> nodes,
                                 std::span<SwimlaneRoutingEdge> edges) {
    for (Index iteration = 0; iteration < 8; ++iteration) {
        const auto routes = materialized_segments(edges);
        bool fixed = false;
        for (Index i = 0; i < edges.size() && !fixed; ++i) {
            auto& edge = edges[i];
            const auto& current = routes[i];
            if (current.size() != 3 || bends(current) != 2) continue;
            const auto& rail = current[1];
            const auto bounds = segment_bounds(rail.a, rail.b);
            const auto c = rail.horizontal ? rail.a.y : rail.a.x;
            double low = inf, high = -inf;
            for (Index n = 0; n < nodes.size(); ++n) {
                if (n == edge.start || n == edge.end || !real_node(nodes[n])) continue;
                const auto r = rect(nodes[n].box);
                const auto minimum = rail.horizontal ? r.top : r.left;
                const auto maximum = rail.horizontal ? r.bottom : r.right;
                const auto overlap = rail.horizontal
                    ? overlap_length(bounds.left, bounds.right, r.left, r.right)
                    : overlap_length(bounds.top, bounds.bottom, r.top, r.bottom);
                if (overlap < 8 || c < minimum - 2 || c > maximum + 2) continue;
                low = std::min(low, minimum);
                high = std::max(high, maximum);
            }
            if (low > high) continue;
            const auto before = crossing_count(current, routes, i);
            for (const auto coord : {low - 20, high + 20}) {
                auto candidate = distinct_points(edge.placed.points);
                auto& a = candidate[rail.index];
                auto& b = candidate[rail.index + 1];
                (rail.horizontal ? a.y : a.x) = coord;
                (rail.horizontal ? b.y : b.x) = coord;
                // These replacements preserve orthogonality, so simplify
                // only removes duplicate and collinear vertices.
                candidate = simplify_candidate(candidate);
                const auto path = tracks_for(i, candidate);
                if (path.size() + 1 != candidate.size() || !materialized_path_clear(nodes, edge, path) ||
                    shares_materialized_rail(path, routes, i) || crossing_count(path, routes, i) > before)
                    continue;
                edge.placed.points = std::move(candidate);
                fixed = true;
                break;
            }
        }
        if (!fixed) return;
    }
}

double path_length(std::span<const TrackSegment> path) {
    double length = 0;
    for (const auto& s : path) length += std::hypot(s.a.x - s.b.x, s.a.y - s.b.y);
    return length;
}

Index total_crossing_count(const RouteSegments& routes) {
    Index count = 0;
    for (Index i = 0; i < routes.size(); ++i)
        count += crossing_count(routes[i], routes, i);
    return count / 2;
}

Index total_bend_count(const RouteSegments& routes) {
    Index count = 0;
    for (const auto& route : routes) count += bends(route);
    return count;
}

// materializedGeometry.ts: consider destination tails as a two-edge
// transaction so a shared target can exchange ports without new crossings.
void swap_destination_tails(std::span<const SwimlaneRoutingNode> nodes,
                            std::span<SwimlaneRoutingEdge> edges) {
    const auto candidate_for = [](std::span<const Point> current, std::span<const Point> other) {
        const auto a = current[0], b = current[1], tail = other[other.size() - 2];
        const auto h = same(a.y, b.y, geometry_eps);
        if (!h && !same(a.x, b.x, geometry_eps)) return std::vector<Point>{};
        const auto corner = h ? Point{b.x, tail.y} : Point{tail.x, b.y};
        const std::array path{a, b, corner, tail, other.back()};
        return simplify_candidate(path);
    };
    for (Index iteration = 0; iteration < 4; ++iteration) {
        auto routes = materialized_segments(edges);
        const auto crossings = total_crossing_count(routes);
        if (!crossings) return;
        auto best = std::pair{crossings, total_bend_count(routes)};
        std::vector<std::vector<Index>> groups;
        std::map<Index, Index> group_index;
        std::vector<std::vector<Point>> points;
        for (Index i = 0; i < edges.size(); ++i) {
            points.push_back(distinct_points(edges[i].placed.points));
            const auto end = edges[i].end;
            if (end >= nodes.size() || points[i].size() < 4) continue;
            auto [found, fresh] = group_index.try_emplace(end, groups.size());
            if (fresh) groups.emplace_back();
            groups[found->second].push_back(i);
        }
        std::optional<std::pair<Index, Index>> best_pair;
        std::vector<Point> best_first, best_second;
        for (const auto& group : groups) {
            for (Index i = 0; i < group.size(); ++i) {
                const auto a = group[i];
                for (Index j = i + 1; j < group.size(); ++j) {
                    const auto b = group[j];
                    const auto tail_valid = [&](Index i) {
                        return !routes[i].empty() && routes[i].back().index == points[i].size() - 2;
                    };
                    if (!tail_valid(a) || !tail_valid(b)) continue;
                    auto first = candidate_for(points[a], points[b]);
                    auto second = candidate_for(points[b], points[a]);
                    auto first_route = tracks_for(a, first), second_route = tracks_for(b, second);
                    if (first_route.size() + 1 != first.size() || second_route.size() + 1 != second.size())
                        continue;
                    const auto safe = [&](Index i, std::span<const TrackSegment> path) {
                        for (const auto& s : path)
                            if (line_hits_real_node(s.a, s.b, nodes, edges[i].start, edges[i].end, 2))
                                return false;
                        return true;
                    };
                    if (!safe(a, first_route) || !safe(b, second_route)) continue;
                    std::swap(routes[a], first_route);
                    std::swap(routes[b], second_route);
                    if (!shares_materialized_rail(routes[a], routes, a) &&
                        !shares_materialized_rail(routes[b], routes, b)) {
                        const auto score = std::pair{total_crossing_count(routes), total_bend_count(routes)};
                        if (score.first < crossings && score < best) {
                            best = score;
                            best_pair = std::pair{a, b};
                            best_first = std::move(first);
                            best_second = std::move(second);
                        }
                    }
                    std::swap(routes[a], first_route);
                    std::swap(routes[b], second_route);
                }
            }
        }
        if (!best_pair) return;
        edges[best_pair->first].placed.points = std::move(best_first);
        edges[best_pair->second].placed.points = std::move(best_second);
    }
}

// materializedGeometry.ts: reassign interacting exterior channels as a
// group, accepting only strict reductions in the global crossing count.
void reassign_external_rails(std::span<const SwimlaneRoutingNode> nodes,
                             std::span<SwimlaneRoutingEdge> edges) {
    struct Rail { TrackSegment segment; Side side; double coordinate, min, max; };
    for (Index iteration = 0; iteration < 4; ++iteration) {
        const auto original = materialized_segments(edges);
        const auto crossings = total_crossing_count(original);
        if (!crossings) return;
        auto best = std::tuple{crossings, total_bend_count(original), inf};
        std::map<Index, std::vector<Point>> best_replacements;
        std::vector<std::vector<Point>> points;
        std::vector<Rail> rails;
        for (Index i = 0; i < edges.size(); ++i) {
            const auto& edge = edges[i];
            points.push_back(distinct_points(edge.placed.points));
            if (edge.start >= nodes.size() || edge.end >= nodes.size()) continue;
            const auto& a = nodes[edge.start].box;
            const auto& b = nodes[edge.end].box;
            if (a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0) continue;
            const auto ar = rect(a), br = rect(b);
            for (const auto& s : original[i]) {
                if (!s.index || s.index + 2 >= points[i].size()) continue;
                const auto lo = s.horizontal ? std::min(ar.top, br.top) : std::min(ar.left, br.left);
                const auto hi = s.horizontal ? std::max(ar.bottom, br.bottom) : std::max(ar.right, br.right);
                const auto coord = s.horizontal ? s.a.y : s.a.x;
                if (coord >= lo - geometry_eps && coord <= hi + geometry_eps) continue;
                const auto side = s.horizontal ? (coord < lo ? Side::top : Side::bottom)
                                               : (coord < lo ? Side::left : Side::right);
                const auto [from, to] = s.horizontal ? std::minmax(s.a.x, s.b.x)
                                                   : std::minmax(s.a.y, s.b.y);
                rails.push_back({s, side, coord, from, to});
            }
        }
        std::vector<bool> seen(rails.size());
        for (Index i = 0; i < rails.size(); ++i) {
            if (seen[i]) continue;
            seen[i] = true;
            std::vector<Index> component, queue{i};
            while (!queue.empty()) {
                const auto index = queue.back();
                queue.pop_back();
                component.push_back(index);
                const auto& rail = rails[index];
                for (Index j = 0; j < rails.size(); ++j) {
                    const auto& other = rails[j];
                    if (!seen[j] && rail.segment.edge != other.segment.edge &&
                        rail.side == other.side && overlap_length(rail.min, rail.max, other.min, other.max) >= 8) {
                        seen[j] = true;
                        queue.push_back(j);
                    }
                }
            }
            if (component.size() < 2) continue;
            std::vector<double> current, coordinates;
            for (const auto index : component) {
                const auto coord = rails[index].coordinate;
                current.push_back(coord);
                if (std::ranges::none_of(coordinates, [&](double c) { return same(c, coord, geometry_eps); }))
                    coordinates.push_back(coord);
            }
            const auto side = rails[component.front()].side;
            while (coordinates.size() < component.size()) {
                const auto delta = 12.0 * (component.size() - coordinates.size());
                if (side == Side::top || side == Side::left)
                    coordinates.push_back(*std::ranges::min_element(coordinates) - delta);
                else
                    coordinates.push_back(*std::ranges::max_element(coordinates) + delta);
            }
            const auto consider = [&](std::span<const double> assignment) {
                double displacement = 0;
                std::map<Index, std::vector<Point>> replacements;
                for (Index j = 0; j < component.size(); ++j) {
                    const auto& s = rails[component[j]].segment;
                    auto& draft = replacements.try_emplace(s.edge, points[s.edge]).first->second;
                    auto& a = draft[s.index];
                    auto& b = draft[s.index + 1];
                    (s.horizontal ? a.y : a.x) = assignment[j];
                    (s.horizontal ? b.y : b.x) = assignment[j];
                    displacement += std::abs(assignment[j] - current[j]);
                }
                auto routes = original;
                for (auto& [edge, path] : replacements) {
                    path = simplify_candidate(path);
                    routes[edge] = tracks_for(edge, path);
                    if (routes[edge].size() + 1 != path.size() ||
                        !materialized_path_clear(nodes, edges[edge], routes[edge]))
                        return;
                }
                for (const auto& [edge, path] : replacements)
                    if (shares_materialized_rail(routes[edge], routes, edge)) return;
                const auto count = total_crossing_count(routes);
                const auto score = std::tuple{count, total_bend_count(routes), displacement};
                if (count < crossings && score < best) {
                    best = score;
                    best_replacements = std::move(replacements);
                }
            };
            if (component.size() <= 6) {
                std::vector<Index> permutation(component.size());
                std::iota(permutation.begin(), permutation.end(), 0);
                do {
                    std::vector<double> assignment;
                    bool changed = false;
                    for (Index j = 0; j < permutation.size(); ++j) {
                        assignment.push_back(coordinates[permutation[j]]);
                        changed = changed || !same(assignment.back(), current[j], geometry_eps);
                    }
                    if (changed) consider(assignment);
                } while (std::next_permutation(permutation.begin(), permutation.end()));
            } else {
                auto assignment = current;
                for (Index j = 0; j < assignment.size(); ++j) {
                    for (Index k = j + 1; k < assignment.size(); ++k) {
                        std::swap(assignment[j], assignment[k]);
                        consider(assignment);
                        std::swap(assignment[j], assignment[k]);
                    }
                }
            }
        }
        if (best_replacements.empty()) return;
        for (auto& [edge, points] : best_replacements)
            edges[edge].placed.points = std::move(points);
    }
}

// materializedGeometry.ts: shorten reverse jogs only if the replacement
// keeps its exit vertex and avoids endpoint borders and other rails.
void shortcut_orthogonal_jogs(std::span<const SwimlaneRoutingNode> nodes,
                              std::span<SwimlaneRoutingEdge> edges) {
    const auto runs_on_border = [](const TrackSegment& s, Rect r) {
        return s.horizontal
            ? (same(s.a.y, r.top, 1) || same(s.a.y, r.bottom, 1)) &&
                overlap_length(s.a.x, s.b.x, r.left, r.right) >= 8
            : (same(s.a.x, r.left, 1) || same(s.a.x, r.right, 1)) &&
                overlap_length(s.a.y, s.b.y, r.top, r.bottom) >= 8;
    };
    for (Index iteration = 0; iteration < 8; ++iteration) {
        const auto routes = materialized_segments(edges);
        const auto crossings = total_crossing_count(routes);
        auto best = std::tuple{crossings, std::numeric_limits<Index>::max(), inf};
        Index best_edge = edges.size();
        std::vector<Point> best_path;
        for (Index i = 0; i < edges.size(); ++i) {
            const auto& edge = edges[i];
            const auto original = distinct_points(edge.placed.points);
            const auto old_bends = bends(routes[i]);
            const auto old_length = path_length(routes[i]);
            const auto old_crossings = crossing_count(routes[i], routes, i);
            for (Index j = 0; j + 3 < original.size(); ++j) {
                const auto path = tracks_for(i, std::span<const Point>{original}.subspan(j, 4));
                if (path.size() != 3 || bends(path) != 2) continue;
                const auto& first = path.front();
                const auto& last = path.back();
                if (first.horizontal ? sign(first.b.x - first.a.x) == sign(last.b.x - last.a.x)
                                     : sign(first.b.y - first.a.y) == sign(last.b.y - last.a.y))
                    continue;
                const auto a = first.a, b = last.b;
                const auto aligned = same(a.x, b.x, geometry_eps) || same(a.y, b.y, geometry_eps);
                const std::array corners{Point{a.x, b.y}, Point{b.x, a.y}};
                for (Index k = 0; k < (aligned ? 1u : corners.size()); ++k) {
                    std::vector<Point> candidate(original.begin(), original.begin() + j + 1);
                    if (!aligned) candidate.push_back(corners[k]);
                    candidate.insert(candidate.end(), original.begin() + j + 3, original.end());
                    candidate = simplify_candidate(candidate);
                    if (std::ranges::none_of(candidate, [&](Point p) { return same(p, b, geometry_eps); }))
                        continue;
                    const auto segments = tracks_for(i, candidate);
                    if (segments.size() + 1 != candidate.size()) continue;
                    const auto candidate_bends = bends(segments);
                    const auto candidate_length = path_length(segments);
                    if (candidate_bends > old_bends ||
                        (candidate_bends == old_bends && candidate_length >= old_length - geometry_eps))
                        continue;
                    if (!materialized_path_clear(nodes, edge, segments) ||
                        shares_materialized_rail(segments, routes, i))
                        continue;
                    const auto on_endpoint = [&](Index node) {
                        if (node >= nodes.size() || nodes[node].box.width <= 0 || nodes[node].box.height <= 0)
                            return false;
                        const auto r = rect(nodes[node].box);
                        return std::ranges::any_of(segments, [&](const TrackSegment& s) {
                            return runs_on_border(s, r);
                        });
                    };
                    if (on_endpoint(edge.start) || on_endpoint(edge.end)) continue;
                    const auto candidate_crossings = crossing_count(segments, routes, i);
                    if (candidate_crossings > old_crossings) continue;
                    const auto score = std::tuple{crossings - old_crossings + candidate_crossings,
                                                  candidate_bends, candidate_length};
                    if (score >= best) continue;
                    best = score;
                    best_edge = i;
                    best_path = std::move(candidate);
                }
            }
        }
        if (best_edge == edges.size()) return;
        edges[best_edge].placed.points = std::move(best_path);
    }
}

using PathKey = std::vector<std::pair<double, double>>;

PathKey rounded_path_key(std::span<const Point> path) {
    PathKey key;
    for (const auto p : path)
        key.emplace_back(std::round(p.x * 1000), std::round(p.y * 1000));
    return key;
}

Index crossings_between(std::span<const TrackSegment> a, std::span<const TrackSegment> b) {
    Index count = 0;
    for (const auto& first : a)
        for (const auto& second : b)
            if (strictly_crosses(first, second)) ++count;
    return count;
}

bool rail_overlap(std::span<const TrackSegment> a, std::span<const TrackSegment> b) {
    for (const auto& first : a)
        for (const auto& second : b)
            if (shared_axis_length(first, second, 0.5) >= 8) return true;
    return false;
}

double orthogonal_length(std::span<const Point> points) {
    double length = 0;
    for (Index i = 1; i < points.size(); ++i) length += distance(points[i - 1], points[i]);
    return length;
}

// Build the bounded face and outer-channel candidates used by Mermaid's
// materialized crossing resolver, preserving generation order for ties.
std::vector<std::vector<Point>> crossing_candidates(
    const SwimlaneRoutingEdge& edge, std::span<const SwimlaneRoutingNode> nodes,
    const std::array<std::array<double, 3>, 4>& outside) {
    std::vector<std::vector<Point>> result;
    if (edge.start >= nodes.size() || edge.end >= nodes.size() ||
        !real_node(nodes[edge.start]) || !real_node(nodes[edge.end]))
        return result;
    const auto& src = nodes[edge.start].box;
    const auto& dst = nodes[edge.end].box;
    const auto tracks = [&](Side side) -> const std::array<double, 3>& {
        return outside[side_index(side)];
    };
    const auto combined_tracks = [&](Side first, Side second) {
        std::array<double, 6> values;
        std::ranges::copy(tracks(first), values.begin());
        std::ranges::copy(tracks(second), values.begin() + 3);
        return values;
    };
    const auto xs = combined_tracks(Side::left, Side::right);
    const auto ys = combined_tracks(Side::top, Side::bottom);
    const auto append = [&](std::vector<Point> path, std::set<PathKey>* seen = nullptr) {
        path = simplify_candidate(path);
        if (path.size() < 2 || tracks_for(0, path).size() + 1 != path.size()) return;
        if (seen && !seen->insert(rounded_path_key(path)).second) return;
        result.push_back(std::move(path));
    };
    for (const auto first : cardinal_sides) {
        const auto a = port(src, first, 0);
        for (const auto second : cardinal_sides) {
            const auto b = port(dst, second, 0);
            std::set<PathKey> seen;
            append(port_path(a, first, b, second, 20), &seen);
            if (first == second) {
                const auto negative = first == Side::top || first == Side::left;
                const auto from = vertical(first) ? a.y : a.x;
                const auto to = vertical(first) ? b.y : b.x;
                const auto local = negative ? std::min(from, to) - 20 : std::max(from, to) + 20;
                for (const auto seed : {local, tracks(first)[0]})
                    for (Index channel = 0; channel <= 2; ++channel)
                        append(same_side_path(a, first, b, seed + (negative ? -20.0 : 20.0) * channel),
                               &seen);
            }
            for (const auto start_track : tracks(first)) {
                for (const auto end_track : tracks(second)) {
                    if (!vertical(first) && vertical(second)) {
                        append({a, {start_track, a.y}, {start_track, end_track},
                                {b.x, end_track}, b}, &seen);
                    } else if (vertical(first) && !vertical(second)) {
                        append({a, {a.x, start_track}, {end_track, start_track},
                                {end_track, b.y}, b}, &seen);
                    } else if (!vertical(first)) {
                        for (const auto y : ys)
                            append({a, {start_track, a.y}, {start_track, y}, {end_track, y},
                                    {end_track, b.y}, b}, &seen);
                    } else {
                        for (const auto x : xs)
                            append({a, {a.x, start_track}, {x, start_track}, {x, end_track},
                                    {b.x, end_track}, b}, &seen);
                    }
                }
            }
        }
    }
    const auto original = distinct_points(edge.placed.points);
    if (original.size() < 4) return result;
    const auto a = original[0], departure = original[1];
    const auto terminal = tracks_for(0, std::span<const Point>{original}.first(2));
    if (terminal.empty()) return result;
    for (const auto side : cardinal_sides) {
        const auto b = port(dst, side, 0);
        if (terminal.front().horizontal) {
            const auto target = !vertical(side) ? std::span<const double>{tracks(side)}
                                               : std::span<const double>{xs};
            for (const auto y : ys) {
                append({a, departure, {departure.x, y}, {b.x, y}, b});
                for (const auto x : target)
                    append({a, departure, {departure.x, y}, {x, y}, {x, b.y}, b});
            }
        } else {
            const auto target = vertical(side) ? std::span<const double>{tracks(side)}
                                              : std::span<const double>{ys};
            for (const auto x : xs) {
                append({a, departure, {x, departure.y}, {x, b.y}, b});
                for (const auto y : target)
                    append({a, departure, {x, departure.y}, {x, y}, {b.x, y}, b});
            }
        }
    }
    return result;
}

// materializedGeometry.ts: resolve crossings first with a single-edge
// candidate, then with bounded pairs from the crossing component and its
// incident edges. Score global crossings before bends and path length.
void resolve_materialized_crossings(std::span<const SwimlaneRoutingNode> nodes,
                                    std::span<SwimlaneRoutingEdge> edges) {
    std::array exterior{inf, -inf, inf, -inf};
    for (const auto& n : nodes) {
        if (!real_node(n)) continue;
        const auto r = rect(n.box);
        exterior[0] = std::min(exterior[0], r.top - 20);
        exterior[1] = std::max(exterior[1], r.bottom + 20);
        exterior[2] = std::min(exterior[2], r.left - 20);
        exterior[3] = std::max(exterior[3], r.right + 20);
    }
    if (!std::isfinite(exterior[0])) return;
    std::array<std::array<double, 3>, 4> outside;
    for (const auto side : cardinal_sides) {
        const auto i = side_index(side);
        const auto step = side == Side::top || side == Side::left ? -20 : 20;
        outside[i] = {exterior[i], exterior[i] + step, exterior[i] + 2 * step};
    }
    const auto hits_node = [&](Index i, std::span<const TrackSegment> path) {
        for (const auto& s : path)
            if (line_hits_real_node(s.a, s.b, nodes, edges[i].start, edges[i].end, 2))
                return true;
        return false;
    };
    struct Option {
        std::vector<Point> path;
        std::vector<TrackSegment> segments;
        std::vector<Index> conflicts, crossings_to;
        Index total_crossings, bend_count;
        double length;
    };
    for (Index iteration = 0; iteration < 4; ++iteration) {
        const auto routes = materialized_segments(edges);
        const auto count = edges.size();
        std::vector<std::vector<Index>> cross_matrix(count, std::vector<Index>(count));
        std::vector<Index> crossing_degree(count), old_bends(count);
        std::vector<double> old_length(count);
        Index current_crossings = 0, current_bends = 0;
        double current_length = 0;
        for (Index i = 0; i < count; ++i) {
            old_bends[i] = bends(routes[i]);
            old_length[i] = orthogonal_length(distinct_points(edges[i].placed.points));
            current_bends += old_bends[i];
            current_length += old_length[i];
            for (Index j = i + 1; j < count; ++j) {
                const auto amount = crossings_between(routes[i], routes[j]);
                cross_matrix[i][j] = cross_matrix[j][i] = amount;
                crossing_degree[i] += amount;
                crossing_degree[j] += amount;
                current_crossings += amount;
            }
        }
        if (!current_crossings) return;
        auto best_single = std::pair{current_crossings, std::numeric_limits<Index>::max()};
        auto single_edge = count;
        std::vector<Point> single_path;
        for (Index i = 0; i < count; ++i) {
            if (!crossing_degree[i]) continue;
            for (auto& path : crossing_candidates(edges[i], nodes, outside)) {
                const auto segments = tracks_for(i, path);
                if (hits_node(i, segments) || shares_materialized_rail(segments, routes, i)) continue;
                const auto crossings = current_crossings - crossing_degree[i] +
                                       crossing_count(segments, routes, i);
                const auto candidate_bends = bends(segments);
                if (crossings > current_crossings ||
                    (crossings == current_crossings && candidate_bends >= old_bends[i]))
                    continue;
                const auto score = std::pair{crossings, candidate_bends};
                if (score < best_single) {
                    best_single = score;
                    single_edge = i;
                    single_path = std::move(path);
                }
            }
        }
        if (single_edge != count) {
            edges[single_edge].placed.points = std::move(single_path);
            continue;
        }

        std::vector<std::optional<std::vector<Option>>> options(count);
        const auto options_for = [&](Index i) -> const std::vector<Option>& {
            if (options[i]) return *options[i];
            auto& candidates = options[i].emplace();
            std::set<PathKey> seen;
            for (auto& path : crossing_candidates(edges[i], nodes, outside)) {
                path = simplify_candidate(path);
                auto segments = tracks_for(i, path);
                if (hits_node(i, segments) || !seen.insert(rounded_path_key(path)).second) continue;
                std::vector<Index> crossings_to(count);
                auto crossings = current_crossings - crossing_degree[i];
                for (Index j = 0; j < count; ++j) {
                    if (j == i) continue;
                    crossings_to[j] = crossings_between(segments, routes[j]);
                    crossings += crossings_to[j];
                }
                if (crossings > current_crossings) continue;
                const auto candidate_bends = bends(segments);
                const auto length = orthogonal_length(path);
                candidates.push_back({std::move(path), std::move(segments), {},
                                      std::move(crossings_to), crossings, candidate_bends, length});
            }
            std::stable_sort(candidates.begin(), candidates.end(), [](const Option& a, const Option& b) {
                return std::tuple{a.total_crossings, a.bend_count, a.length} <
                       std::tuple{b.total_crossings, b.bend_count, b.length};
            });
            if (candidates.size() > 48) candidates.resize(48);
            for (auto& option : candidates)
                for (Index j = 0; j < count; ++j)
                    if (i != j && rail_overlap(option.segments, routes[j]))
                        option.conflicts.push_back(j);
            return candidates;
        };
        const auto only_conflicts_with = [](const Option& option, Index other) {
            return option.conflicts.empty() ||
                   (option.conflicts.size() == 1 && option.conflicts.front() == other);
        };
        auto best_pair = std::tuple{current_crossings, current_bends, current_length};
        Index best_a = count, best_b = count;
        const Option* best_first = nullptr;
        const Option* best_second = nullptr;
        std::vector<bool> seen(count);
        for (Index root = 0; root < count; ++root) {
            if (!crossing_degree[root] || seen[root]) continue;
            std::vector<Index> component, queue{root};
            seen[root] = true;
            while (!queue.empty()) {
                const auto i = queue.back();
                queue.pop_back();
                component.push_back(i);
                for (Index j = 0; j < count; ++j) {
                    if (cross_matrix[i][j] && !seen[j]) {
                        seen[j] = true;
                        queue.push_back(j);
                    }
                }
            }
            auto group = component;
            for (Index i = 0; i < count; ++i) {
                if (std::ranges::find(component, i) != component.end()) continue;
                if (std::ranges::any_of(component, [&](Index j) {
                        return incident_edges(edges[i], edges[j]);
                    }))
                    group.push_back(i);
            }
            std::ranges::sort(group);
            for (Index i = 0; i < group.size(); ++i) {
                const auto a = group[i];
                for (Index j = i + 1; j < group.size(); ++j) {
                    const auto b = group[j];
                    if (!crossing_degree[a] && !crossing_degree[b]) continue;
                    const auto base = current_crossings + cross_matrix[a][b] -
                                      crossing_degree[a] - crossing_degree[b];
                    for (const auto& first : options_for(a)) {
                        if (!only_conflicts_with(first, b)) continue;
                        for (const auto& second : options_for(b)) {
                            if (!only_conflicts_with(second, a) ||
                                rail_overlap(first.segments, second.segments))
                                continue;
                            const auto crossings = base +
                                (first.total_crossings - (current_crossings - crossing_degree[a])) -
                                first.crossings_to[b] +
                                (second.total_crossings - (current_crossings - crossing_degree[b])) -
                                second.crossings_to[a] + crossings_between(first.segments, second.segments);
                            if (crossings >= current_crossings) continue;
                            const auto score = std::tuple{
                                crossings,
                                current_bends - old_bends[a] - old_bends[b] +
                                    first.bend_count + second.bend_count,
                                current_length - old_length[a] - old_length[b] +
                                    first.length + second.length};
                            if (score < best_pair) {
                                best_pair = score;
                                best_a = a;
                                best_b = b;
                                best_first = &first;
                                best_second = &second;
                            }
                        }
                    }
                }
            }
        }
        if (!best_first) return;
        edges[best_a].placed.points = best_first->path;
        edges[best_b].placed.points = best_second->path;
    }
}

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

}

void route_swimlane_edges(std::span<const SwimlaneRoutingNode> nodes,
                         std::span<SwimlaneRoutingEdge> edges,
                         std::u16string_view direction) {
    Router(nodes, edges, direction == u"LR").run();
}

void finish_swimlane_routes(std::span<SwimlaneRoutingNode> nodes,
                            std::span<SwimlaneRoutingEdge> edges) {
    for (auto& e : edges)
        if (e.placed.points.size() >= 2)
            e.placed.points = simplify(std::move(e.placed.points));
    simplify_detoured_edges(nodes, edges);
    straighten_sibling_detours(nodes, edges);
    swap_source_ports(nodes, edges);
    anchor_labels(nodes, edges);
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
    collapse_short_terminal_stubs(nodes, edges);
    nudge_shared_tracks(nodes, edges);
    separate_terminal_lanes(nodes, edges);
    collapse_rectangular_doglegs(nodes, edges);
    lift_obstacle_hugging_rails(nodes, edges);
    swap_destination_tails(nodes, edges);
    const auto snap = [&] {
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
    };
    // Keep the browser handoff's duplicate vertices between passes, where
    // label placement uses the original index of each visible segment.
    const auto finalize = [&] {
        resolve_materialized_crossings(nodes, edges);
        separate_terminal_lanes(nodes, edges);
        reassign_external_rails(nodes, edges);
        shortcut_orthogonal_jogs(nodes, edges);
        anchor_labels(nodes, edges);
        snap();
        lift_obstacle_hugging_rails(nodes, edges);
        anchor_labels(nodes, edges);
        snap();
    };
    finalize();
    nudge_shared_tracks(nodes, edges);
    finalize();
    // The native SVG adapter consumes the materialized endpoints directly.
    for (auto& edge : edges)
        edge.placed.points = distinct_points(edge.placed.points);
}

// materializedGeometry.ts: move the aligned header bands after the rails are
// final; the headers themselves are never routing obstacles.
void adjust_swimlane_title_bands(std::span<SwimlaneRoutingNode> nodes,
                                 std::span<const SwimlaneRoutingEdge> edges,
                                 std::u16string_view direction) {
    const auto adjust = [&](bool left) {
        std::vector<Index> lanes;
        for (Index i = 0; i < nodes.size(); ++i) {
            const auto& node = nodes[i];
            if (!node.group || node.parent || !node.title_box ||
                (left ? node.box.width : node.box.height) <= 0)
                continue;
            const auto mode = node.direction.empty() ? direction : std::u16string_view(node.direction);
            if (left ? mode != u"LR" : mode == u"LR" || mode == u"RL" || mode == u"BT")
                continue;
            const auto& title = *node.title_box;
            if (title.width <= 0 || title.height <= 0 ||
                (left ? title.height < title.width : title.width < title.height))
                continue;
            lanes.push_back(i);
        }
        double shift = 0;
        for (const auto& edge : edges) {
            const auto& points = edge.placed.points;
            for (Index j = 1; j < points.size(); ++j) {
                const auto a = points[j - 1], b = points[j];
                for (const auto i : lanes) {
                    const auto r = rect(*nodes[i].title_box);
                    if (left && same(a.x, b.x, geometry_eps) && within(a.x, r.left, r.right) &&
                        overlap_length(a.y, b.y, r.top, r.bottom) >= 8) {
                        shift = std::max(shift, r.right - a.x + 4);
                    } else if (same(a.y, b.y, geometry_eps) && within(a.y, r.top, r.bottom) &&
                               overlap_length(a.x, b.x, r.left, r.right) >= 8) {
                        shift = std::max(shift, left ? r.right - std::min(a.x, b.x) + 4
                                                    : r.bottom - a.y + 4);
                    }
                }
            }
        }
        if (shift <= geometry_eps) return;
        for (const auto i : lanes) {
            auto& node = nodes[i];
            (left ? node.box.x : node.box.y) -= shift / 2;
            (left ? node.box.width : node.box.height) += shift;
            (left ? node.title_box->x : node.title_box->y) -= shift;
        }
    };
    for (Index repeat = 0; repeat < 2; ++repeat) {
        adjust(false);
        adjust(true);
    }
}

}
