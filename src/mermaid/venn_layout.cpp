module folia.mermaid.venn_layout;
import std;

namespace folia::mermaid {
namespace {
constexpr double pi = std::numbers::pi;
constexpr double epsilon = 1e-10;
using Vector = std::vector<double>;
using Indices = std::vector<std::size_t>;

// The simplex and bisection routines follow the patched fmin 0.0.4
// implementation shipped in @upsetjs/venn.js 2.0.0.
Vector minimize(const auto& loss, Vector initial, double error, double tolerance) {
    struct Vertex { Vector x; double value; };
    auto evaluate = [&](Vector x) {
        const auto value = loss(x);
        return Vertex{std::move(x), value};
    };
    const auto n = initial.size();
    std::vector<Vertex> simplex{evaluate(initial)};
    for (std::size_t i = 0; i < n; ++i) {
        auto point = initial;
        point[i] = point[i] != 0 ? point[i] * 1.05 : 0.001;
        simplex.push_back(evaluate(std::move(point)));
    }
    auto sort = [&] { std::ranges::stable_sort(simplex, {}, &Vertex::value); };
    auto blend = [&](const Vector& x, double wx, const Vector& y, double wy) {
        Vector out(n);
        for (std::size_t i = 0; i < n; ++i) out[i] = wx * x[i] + wy * y[i];
        return evaluate(std::move(out));
    };
    for (int iteration = 0; iteration < 500; ++iteration) {
        sort();
        double difference = 0;
        for (std::size_t i = 0; i < n; ++i)
            difference = std::max(difference, std::abs(simplex[0].x[i] - simplex[1].x[i]));
        auto& best = simplex.front();
        auto& worst = simplex.back();
        if (std::abs(best.value - worst.value) < error && difference < tolerance) break;

        Vector centroid(n);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) centroid[i] += simplex[j].x[i];
            centroid[i] /= static_cast<double>(n);
        }
        auto reflected = blend(centroid, 2, worst.x, -1);
        if (reflected.value < best.value) {
            auto expanded = blend(centroid, 3, worst.x, -2);
            worst = expanded.value < reflected.value ? std::move(expanded) : std::move(reflected);
        } else if (reflected.value >= simplex[n - 1].value) {
            const bool inside = reflected.value > worst.value;
            auto contracted = inside ? blend(centroid, 0.5, worst.x, 0.5)
                                     : blend(centroid, 1.5, worst.x, -0.5);
            if (contracted.value < (inside ? worst.value : reflected.value)) {
                worst = std::move(contracted);
            } else {
                for (std::size_t i = 1; i <= n; ++i)
                    simplex[i] = blend(best.x, 0.5, simplex[i].x, 0.5);
            }
        } else {
            worst = std::move(reflected);
        }
    }
    sort();
    return std::move(simplex.front().x);
}

double distance(VennPoint a, VennPoint b) {
    const auto x = a.x - b.x, y = a.y - b.y;
    return std::sqrt(x * x + y * y);
}
double segment(double r, double w) {
    w = std::clamp(w, 0.0, 2 * r);
    return r * r * std::acos(1 - w / r) - (r - w) * std::sqrt(w * (2 * r - w));
}
double overlap(double a, double b, double d) {
    if (d >= a + b) return 0;
    if (d <= std::abs(a - b)) return pi * std::min(a, b) * std::min(a, b);
    const auto wa = a - (d * d - b * b + a * a) / (2 * d);
    const auto wb = b - (d * d - a * a + b * b) / (2 * d);
    return segment(a, wa) + segment(b, wb);
}
double overlap_distance(double a, double b, double area) {
    if (std::min(a, b) * std::min(a, b) * pi <= area + epsilon) return std::abs(a - b);
    if (area == 0) return a + b;
    double left = 0, delta = a + b;
    for (int i = 0; i < 100; ++i) {
        delta /= 2;
        const auto mid = left + delta;
        const auto error = overlap(a, b, mid) - area;
        if (error >= 0) left = mid;
        if (delta < epsilon || error == 0) return mid;
    }
    return left + delta;
}
std::vector<VennPoint> crossings(VennCircle a, VennCircle b) {
    const auto d = distance(a.center, b.center), r = a.radius, s = b.radius;
    if (d >= r + s || d <= std::abs(r - s)) return {};
    const auto line = (r * r - s * s + d * d) / (2 * d);
    const auto height = std::sqrt(std::max(0.0, r * r - line * line));
    const auto dx = b.center.x - a.center.x, dy = b.center.y - a.center.y;
    const auto x = a.center.x + line * dx / d, y = a.center.y + line * dy / d;
    const auto rx = -dy * (height / d), ry = -dx * (height / d);
    return {{x + rx, y - ry}, {x - rx, y + ry}};
}
VennPoint midpoint(VennPoint a, VennPoint b) {
    return {(a.x + b.x) / 2, (a.y + b.y) / 2};
}
struct Intersection { double area = 0; std::vector<VennArc> arcs; };
Intersection intersect(const std::vector<VennCircle>& circles) {
    struct Vertex { VennPoint point; std::size_t a, b; };
    std::vector<Vertex> vertices;
    VennPoint center{};
    for (std::size_t i = 0; i < circles.size(); ++i) {
        for (std::size_t j = i + 1; j < circles.size(); ++j) {
            for (auto p : crossings(circles[i], circles[j])) {
                if (std::ranges::all_of(circles, [&](const auto& circle) {
                    return distance(p, circle.center) < circle.radius + epsilon;
                })) {
                    vertices.push_back({p, i, j});
                    center.x += p.x;
                    center.y += p.y;
                }
            }
        }
    }
    if (vertices.size() < 2) {
        const auto circle = *std::ranges::min_element(circles, {}, &VennCircle::radius);
        if (std::ranges::any_of(circles, [&](const auto& outer) {
            return distance(circle.center, outer.center) > std::abs(circle.radius - outer.radius);
        })) return {};
        return {pi * circle.radius * circle.radius, {{circle, {}, {}, true}}};
    }
    center.x /= static_cast<double>(vertices.size());
    center.y /= static_cast<double>(vertices.size());
    std::ranges::stable_sort(vertices, std::greater{}, [&](const Vertex& vertex) {
        return std::atan2(vertex.point.x - center.x, vertex.point.y - center.y);
    });
    Intersection result;
    double polygon = 0;
    auto previous = vertices.back();
    for (auto current : vertices) {
        const auto a = current.point, b = previous.point;
        polygon += (a.x + b.x) * (a.y - b.y);
        auto min_width = std::numeric_limits<double>::infinity();
        std::optional<VennArc> arc;
        for (auto i : {current.a, current.b}) {
            if (i != previous.a && i != previous.b) continue;
            const auto circle = circles[i];
            const auto c = circle.center;
            const auto r = circle.radius;
            const auto angle_a = std::atan2(a.x - c.x, a.y - c.y);
            const auto angle_b = std::atan2(b.x - c.x, b.y - c.y);
            auto delta = angle_b - angle_a;
            if (delta < 0) delta += 2 * pi;
            const auto angle = angle_b - delta / 2;
            const auto width = std::min(2 * r, distance(midpoint(a, b),
                {c.x + r * std::sin(angle), c.y + r * std::cos(angle)}));
            if (width < min_width) {
                min_width = width;
                arc = VennArc{circle, b, a, width > r};
            }
        }
        if (arc) {
            result.area += segment(arc->circle.radius, min_width);
            result.arcs.push_back(*arc);
            previous = current;
        }
    }
    result.area += polygon / 2;
    return result;
}

struct Constraint { Indices members; double size; };
std::vector<VennCircle> select_circles(const std::vector<VennCircle>& circles, const Indices& ids) {
    std::vector<VennCircle> result;
    for (const auto id : ids) result.push_back(circles[id]);
    return result;
}
double area_loss(const std::vector<VennCircle>& circles, std::span<const Constraint> areas) {
    double loss = 0;
    for (const auto& area : areas) {
        const auto& ids = area.members;
        const auto a = circles[ids[0]], b = circles[ids[1]];
        const auto value = ids.size() == 2 ? overlap(a.radius, b.radius, distance(a.center, b.center))
                                         : intersect(select_circles(circles, ids)).area;
        const auto error = value - area.size;
        loss += error * error;
    }
    return loss;
}
void position_circles(std::vector<VennCircle>& circles, const Vector& sizes,
    const std::vector<Constraint>& pairs, const std::vector<Constraint>& areas) {
    struct Neighbor { std::size_t id; double size; };
    std::vector<std::vector<Neighbor>> neighbors(circles.size());
    Vector score(circles.size());
    for (const auto& pair : pairs) {
        const auto a = pair.members[0], b = pair.members[1];
        neighbors[a].push_back({b, pair.size});
        neighbors[b].push_back({a, pair.size});
        if (pair.size + epsilon < std::min(sizes[a], sizes[b])) {
            score[a] += pair.size;
            score[b] += pair.size;
        }
    }
    Indices order(circles.size());
    std::iota(order.begin(), order.end(), 0);
    std::ranges::stable_sort(order, std::greater{}, [&](auto id) { return score[id]; });
    std::vector<bool> positioned(circles.size());
    circles[order.front()].center = {0, 0};
    positioned[order.front()] = true;
    for (auto id : order | std::views::drop(1)) {
        auto& circle = circles[id];
        auto near = neighbors[id];
        std::erase_if(near, [&](auto n) { return !positioned[n.id]; });
        std::ranges::stable_sort(near, std::greater{}, &Neighbor::size);
        std::vector<VennPoint> candidates;
        for (std::size_t j = 0; j < near.size(); ++j) {
            const auto a = circles[near[j].id];
            const auto d = overlap_distance(circle.radius, a.radius, near[j].size);
            const auto [x, y] = a.center;
            candidates.insert(candidates.end(), {{x + d, y}, {x - d, y}, {x, y + d}, {x, y - d}});
            for (std::size_t k = j + 1; k < near.size(); ++k) {
                const auto b = circles[near[k].id];
                const auto e = overlap_distance(circle.radius, b.radius, near[k].size);
                const auto points = crossings({a.center, d}, {b.center, e});
                candidates.insert(candidates.end(), points.begin(), points.end());
            }
        }
        auto best = candidates.front();
        auto loss = std::numeric_limits<double>::infinity();
        for (auto point : candidates) {
            circle.center = point;
            const auto value = area_loss(circles, pairs);
            if (value < loss) { loss = value; best = point; }
        }
        circle.center = best;
        positioned[id] = true;
    }
    Vector initial;
    for (const auto& circle : circles) {
        initial.push_back(circle.center.x);
        initial.push_back(circle.center.y);
    }
    auto move_circles = [&](const Vector& point) {
        for (std::size_t i = 0; i < circles.size(); ++i) circles[i].center = {point[2*i], point[2*i+1]};
    };
    auto optimized = minimize([&](const Vector& point) {
        move_circles(point);
        return area_loss(circles, areas);
    }, std::move(initial), 1e-6, 1e-5);
    move_circles(optimized);
}

struct Bounds {
    double left, right, top, bottom;
    double width() const { return right - left; }
    double height() const { return bottom - top; }
    double area() const { return width() * height(); }
};
Bounds bounds(const std::vector<VennCircle>& circles, const Indices& ids) {
    const auto inf = std::numeric_limits<double>::infinity();
    Bounds result{inf, -inf, inf, -inf};
    for (auto i : ids) {
        const auto& [p, r] = circles[i];
        result.left = std::min(result.left, p.x - r);
        result.right = std::max(result.right, p.x + r);
        result.top = std::min(result.top, p.y - r);
        result.bottom = std::max(result.bottom, p.y + r);
    }
    return result;
}
void orient(std::vector<VennCircle>& circles, Indices& ids) {
    std::ranges::stable_sort(ids, std::greater{}, [&](auto i) { return circles[i].radius; });
    const auto origin = circles[ids[0]].center;
    for (auto id : ids) {
        circles[id].center.x -= origin.x;
        circles[id].center.y -= origin.y;
    }
    if (ids.size() < 2) return;
    auto& second = circles[ids[1]];
    const auto difference = circles[ids[0]].radius - second.radius;
    if (ids.size() == 2 && distance({}, second.center) < difference)
        second.center = {difference - epsilon, 0};
    const auto rotation = std::atan2(second.center.x, second.center.y) - pi / 2;
    const auto c = std::cos(rotation), s = std::sin(rotation);
    for (auto id : ids) {
        auto [x, y] = circles[id].center;
        circles[id].center = {c * x - s * y, s * x + c * y};
    }
    if (ids.size() < 3) return;
    const auto third = circles[ids[2]].center;
    auto angle = std::atan2(third.x, third.y) - pi / 2;
    if (angle < 0) angle += 2 * pi;
    if (angle <= pi) return;
    const auto slope = second.center.y / (epsilon + second.center.x);
    for (auto id : ids) {
        auto [x, y] = circles[id].center;
        const auto d = (x + slope * y) / (1 + slope * slope);
        circles[id].center = {2 * d - x, 2 * d * slope - y};
    }
}
Indices normalize(std::vector<VennCircle>& circles, double width, double height, double padding) {
    Indices parents(circles.size());
    std::iota(parents.begin(), parents.end(), 0);
    auto root = [&](std::size_t i) {
        while (parents[i] != i) i = parents[i];
        return i;
    };
    for (std::size_t i = 0; i < circles.size(); ++i) {
        for (std::size_t j = i + 1; j < circles.size(); ++j) {
            if (distance(circles[i].center, circles[j].center) + epsilon <
                circles[i].radius + circles[j].radius) parents[root(j)] = root(i);
        }
    }
    struct Cluster { Indices ids; Bounds box; };
    std::vector<Cluster> clusters;
    std::map<std::size_t, std::size_t> cluster_index;
    for (std::size_t i = 0; i < circles.size(); ++i) {
        auto [entry, inserted] = cluster_index.try_emplace(root(i), clusters.size());
        if (inserted) clusters.push_back({});
        clusters[entry->second].ids.push_back(i);
    }
    for (auto& cluster : clusters) {
        orient(circles, cluster.ids);
        cluster.box = bounds(circles, cluster.ids);
    }
    std::ranges::stable_sort(clusters, std::greater{}, [](const auto& c) { return c.box.area(); });
    auto ids = clusters.front().ids;
    auto overall = clusters.front().box;
    const auto spacing = overall.width() / 50;
    for (std::size_t i = 1; i < clusters.size(); ++i) {
        const auto& [members, box] = clusters[i];
        const bool right = i % 3 != 2, bottom = i % 3 != 1;
        const auto dx = right ? overall.right - box.left + spacing
            : overall.right - box.right + std::min(0.0, (box.width() - overall.width()) / 2);
        const auto dy = bottom ? overall.bottom - box.top + spacing
            : overall.bottom - box.bottom + std::min(0.0, (box.height() - overall.height()) / 2);
        for (auto id : members) {
            circles[id].center.x += dx;
            circles[id].center.y += dy;
            ids.push_back(id);
        }
        if (i % 3 == 0) overall = bounds(circles, ids);
    }
    overall = bounds(circles, ids);
    width -= 2 * padding;
    height -= 2 * padding;
    const auto scale = std::min(width / overall.width(), height / overall.height());
    const auto dx = padding + (width - overall.width() * scale) / 2;
    const auto dy = padding + (height - overall.height() * scale) / 2;
    for (auto& [p, r] : circles) {
        p = {dx + (p.x - overall.left) * scale, dy + (p.y - overall.top) * scale};
        r *= scale;
    }
    return ids;
}

VennPoint label_center(const std::vector<VennCircle>& inside, const std::vector<VennCircle>& outside) {
    auto margin = [&](VennPoint point) {
        auto best = std::numeric_limits<double>::infinity();
        for (const auto& [p, r] : inside) best = std::min(best, r - distance(p, point));
        for (const auto& [p, r] : outside) best = std::min(best, distance(p, point) - r);
        return best;
    };
    VennPoint initial{};
    auto best = -std::numeric_limits<double>::infinity();
    for (const auto& [p, r] : inside) {
        for (auto sample : {p, VennPoint{p.x + r/2, p.y}, VennPoint{p.x - r/2, p.y},
            VennPoint{p.x, p.y + r/2}, VennPoint{p.x, p.y - r/2}}) {
            const auto m = margin(sample);
            if (m >= best) { best = m; initial = sample; }
        }
    }
    auto solution = minimize([&](const Vector& p) { return -margin({p[0], p[1]}); },
        {initial.x, initial.y}, epsilon, epsilon);
    const VennPoint center{solution[0], solution[1]};
    if (margin(center) >= 0) return center;
    if (inside.size() == 1) return inside.front().center;
    const auto shape = intersect(inside);
    if (shape.arcs.empty()) return {0, -1000};
    if (shape.arcs.size() == 1) return shape.arcs.front().circle.center;
    if (!outside.empty()) return label_center(inside, {});
    VennPoint result{};
    for (const auto& arc : shape.arcs) {
        result.x += arc.end.x;
        result.y += arc.end.y;
    }
    const auto count = static_cast<double>(shape.arcs.size());
    return {result.x / count, result.y / count};
}
}

std::expected<std::vector<VennRegion>, std::string> layout_venn(
    const VennDiagram& diagram, double width, double height, double padding) {
    if (padding < 0 || width <= 2 * padding || height <= 2 * padding)
        return std::unexpected("Invalid Venn canvas or padding");
    std::vector<VennArea> visible;
    std::map<std::u16string, std::size_t, std::less<>> lookup;
    std::set<std::u16string> zero;
    Vector sizes;
    std::vector<VennCircle> circles;
    for (const auto& area : diagram.areas) {
        if (area.sets.size() != 1) continue;
        if (area.size == 0) { zero.insert(area.sets[0]); continue; }
        if (!lookup.try_emplace(area.sets[0], circles.size()).second)
            return std::unexpected("Duplicate Venn set");
        sizes.push_back(area.size);
        circles.push_back({{1e10, 1e10}, std::sqrt(area.size / pi)});
    }
    if (circles.empty()) return std::vector<VennRegion>{};
    if (circles.size() > 3) return std::unexpected("Venn layout supports up to three nonempty sets");

    std::set<VennSets> explicit_areas;
    for (const auto& area : diagram.areas) {
        if (std::ranges::any_of(area.sets, [&](const auto& id) { return zero.contains(id); })) continue;
        if (!explicit_areas.insert(area.sets).second)
            return std::unexpected("Duplicate Venn region");
        visible.push_back(area);
    }
    std::vector<VennArea> synthetic;
    for (const auto& area : visible) {
        for (std::size_t i = 0; area.sets.size() > 2 && i < area.sets.size(); ++i) {
            for (std::size_t j = i + 1; j < area.sets.size(); ++j) {
                VennSets pair{area.sets[i], area.sets[j]};
                if (!explicit_areas.insert(pair).second) continue;
                auto a = lookup.find(pair[0]), b = lookup.find(pair[1]);
                if (a == lookup.end() || b == lookup.end()) return std::unexpected("Unknown Venn set");
                synthetic.push_back({std::move(pair), std::min(sizes[a->second], sizes[b->second]) / 4, {}});
            }
        }
    }
    visible.insert(visible.end(), synthetic.begin(), synthetic.end());
    std::vector<Constraint> areas, pairs;
    auto indices = [&](const VennSets& sets) -> std::optional<Indices> {
        Indices result;
        for (const auto& id : sets) {
            const auto found = lookup.find(id);
            if (found == lookup.end()) return std::nullopt;
            result.push_back(found->second);
        }
        return result;
    };
    for (const auto& area : visible) {
        if (area.sets.size() == 1) continue;
        auto ids = indices(area.sets);
        if (!ids || ids->size() < 2) return std::unexpected("Invalid Venn region");
        areas.push_back({*ids, area.size});
        if (ids->size() == 2) pairs.push_back(areas.back());
    }
    for (auto a = lookup.begin(); a != lookup.end(); ++a) {
        for (auto b = std::next(a); b != lookup.end(); ++b) {
            if (explicit_areas.contains({a->first, b->first})) continue;
            areas.push_back({{a->second, b->second}, 0});
            pairs.push_back(areas.back());
        }
    }
    position_circles(circles, sizes, pairs, areas);
    const auto order = normalize(circles, width, height, padding);
    std::vector<Indices> containing(circles.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        for (std::size_t j = i + 1; j < order.size(); ++j) {
            const auto a = order[i], b = order[j];
            const auto d = distance(circles[a].center, circles[b].center);
            if (d + circles[b].radius <= circles[a].radius + epsilon) containing[b].push_back(a);
            else if (d + circles[a].radius <= circles[b].radius + epsilon) containing[a].push_back(b);
        }
    }
    std::vector<VennRegion> regions;
    for (auto& area : visible) {
        const auto ids = *indices(area.sets);
        Indices interior, exterior;
        for (auto id : order) {
            if (std::ranges::contains(ids, id)) interior.push_back(id);
            else if (std::ranges::none_of(ids, [&](auto i) { return std::ranges::contains(containing[i], id); }))
                exterior.push_back(id);
        }
        auto members = select_circles(circles, ids);
        const auto anchor = label_center(select_circles(circles, interior), select_circles(circles, exterior));
        auto shape = intersect(members);
        regions.push_back({std::move(area), anchor, std::move(members), std::move(shape.arcs)});
    }
    return regions;
}
}
