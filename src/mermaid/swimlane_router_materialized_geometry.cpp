// Based on Mermaid 12.2.0 swimlanes/direction/materializedGeometry.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_router;
import std;

#include "swimlane_router_geometry.inc"
#include "swimlane_router_phases.inc"

namespace folia::mermaid::swimlane_routing_detail {

namespace {

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

} // namespace

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

namespace {

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

} // namespace

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

namespace {

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

} // namespace

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

namespace {

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

} // namespace

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

} // namespace folia::mermaid::swimlane_routing_detail

namespace folia::mermaid {

// materializedGeometry.ts: move the aligned header bands after the rails are
// final; the headers themselves are never routing obstacles.
void adjust_swimlane_title_bands(std::span<SwimlaneRoutingNode> nodes,
                                 std::span<const SwimlaneRoutingEdge> edges,
                                 std::u16string_view direction) {
    using namespace swimlane_routing_detail;
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
