module folia.mermaid.rough_hachure;
import std;
import folia.mermaid.number;

namespace folia::mermaid {
namespace {
using Point = RoughPoint;
using Polygon = std::vector<Point>;
using Line = std::array<Point, 2>;
using Cubic = std::array<Point, 4>;
constexpr auto pi = std::numbers::pi;
constexpr auto tau = 2 * pi;
constexpr auto roughness = 0.7;
constexpr auto max_randomness = 2.0;
constexpr auto max_scan_rows = 1'000'000;

class Random {
public:
    // A zero seed uses Math.random in RoughJS. Keep that branch stable for
    // repeated native renders by assigning a local nonzero seed.
    explicit Random(std::uint32_t seed) : state_(seed ? seed : 1) {}
    double next() {
        state_ *= 48271u;
        return (state_ & 0x7fffffffu) / 2147483648.0;
    }
    double between(double min, double max, double gain = 1) {
        return roughness * gain * (next() * (max - min) + min);
    }
    double offset(double max, double gain = 1) { return between(-max, max, gain); }
private:
    std::uint32_t state_;
};

std::string number(double x) {
    auto result = format_number(x);
    if (!result) throw std::runtime_error("Invalid RoughJS coordinate");
    return std::move(*result);
}
std::string point(Point p) { return number(p.x) + " " + number(p.y); }
void move(std::string& path, Point p) {
    if (!path.empty()) path += ' ';
    path += "M" + point(p);
}
void curve(std::string& path, Point a, Point b, Point end) {
    path += " C" + point(a) + ", " + point(b) + ", " + point(end);
}
double square(double x) { return x * x; }
bool same(Point a, Point b) { return a.x == b.x && a.y == b.y; }
double distance_squared(Point a, Point b) { return square(a.x - b.x) + square(a.y - b.y); }
double js_round(double x) {
    const auto lower = std::floor(x);
    return x - lower < 0.5 ? lower : lower + 1;
}

// hachure-fill rotates the polygon, pairs active edges on each scanline,
// rounds the two x coordinates, and rotates the resulting lines back.
std::vector<Line> scanlines(Polygon& polygon, int gap, double angle) {
    if (polygon.size() < 2) return {};
    auto rotate = [](Point& p, double c, double s) {
        p = {p.x * c - p.y * s, p.x * s + p.y * c};
    };
    const auto a = (pi / 180) * angle;
    const auto c = std::cos(a), s = std::sin(a);
    for (auto& p : polygon) rotate(p, c, s);
    struct Edge { double ymin, ymax, x, slope; };
    std::vector<Edge> edges;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto p = polygon[i], q = polygon[(i + 1) % polygon.size()];
        if (p.y == q.y) continue;
        const auto low = p.y < q.y ? p : q, high = p.y < q.y ? q : p;
        edges.push_back({low.y, high.y, low.x, (q.x - p.x) / (q.y - p.y)});
    }
    std::ranges::stable_sort(edges, {}, [](const auto& e) { return std::tuple{e.ymin, e.x, e.ymax}; });
    std::vector<Line> lines;
    std::vector<Edge> active;
    if (!edges.empty()) {
        auto y = edges.front().ymin;
        auto next = edges.begin();
        for (int row = 0; !active.empty() || next != edges.end(); ++row, ++y) {
            if (row == max_scan_rows) throw std::runtime_error("RoughJS hatch exceeds scanline limit");
            while (next != edges.end() && next->ymin <= y) active.push_back(*next++);
            std::erase_if(active, [y](const auto& e) { return e.ymax <= y; });
            std::ranges::stable_sort(active, {}, &Edge::x);
            if (row % gap == 0) {
                for (std::size_t i = 1; i < active.size(); i += 2)
                    lines.push_back({Point{js_round(active[i - 1].x), y}, Point{js_round(active[i].x), y}});
            }
            for (auto& e : active) e.x += e.slope;
        }
    }
    for (auto& p : polygon) rotate(p, c, -s);
    for (auto& line : lines)
        for (auto& p : line) rotate(p, c, -s);
    return lines;
}

void hatch_line(std::string& path, Line line, Random& random) {
    const auto [a, b] = line;
    const auto dx = b.x - a.x, dy = b.y - a.y;
    const auto length_squared = square(dx) + square(dy);
    const auto length = std::sqrt(length_squared);
    const auto gain = length < 200 ? 1.0 : length > 500 ? 0.4 : -0.0016668 * length + 1.233334;
    const auto offset = square(max_randomness) * 100 > length_squared ? length / 10 : max_randomness;
    for (const auto jitter : {offset, offset / 2}) {
        const auto diverge = 0.2 + random.next() * 0.2;
        const auto bow_x = random.offset(max_randomness * dy / 200, gain);
        const auto bow_y = random.offset(max_randomness * -dx / 200, gain);
        auto noise = [&] { return random.offset(jitter, gain); };
        move(path, {a.x + noise(), a.y + noise()});
        const Point first{bow_x + a.x + dx * diverge + noise(), bow_y + a.y + dy * diverge + noise()};
        const Point second{bow_x + a.x + 2 * dx * diverge + noise(), bow_y + a.y + 2 * dy * diverge + noise()};
        const Point end{b.x + noise(), b.y + noise()};
        curve(path, first, second, end);
    }
}

void hatch(std::string& path, Polygon& polygon, int gap, double angle, Random& random) {
    for (const auto& line : scanlines(polygon, gap, angle + 90))
        hatch_line(path, line, random);
}

Polygon ellipse_points(Point center, double rx, double ry, double increment,
    double jitter, double overlap, Random& random, Polygon* estimated) {
    const auto first = random.offset(0.5) - pi / 2;
    auto sample = [&](double angle, double factor) -> Point {
        return {random.offset(jitter) + center.x + factor * rx * std::cos(angle),
                random.offset(jitter) + center.y + factor * ry * std::sin(angle)};
    };
    Polygon points{sample(first - increment, 0.9)};
    for (auto angle = first; angle < tau + first - 0.01; angle += increment) {
        points.push_back(sample(angle, 1));
        if (estimated) estimated->push_back(points.back());
    }
    points.push_back(sample(first + tau + overlap * 0.5, 1));
    points.push_back(sample(first + overlap, 0.98));
    points.push_back(sample(first + overlap * 0.5, 0.9));
    return points;
}

void ellipse_curve(std::string& path, const Polygon& points) {
    move(path, points[1]);
    for (std::size_t i = 1; i + 2 < points.size(); ++i) {
        const auto a = points[i - 1], b = points[i], c = points[i + 1], d = points[i + 2];
        curve(path, {b.x + (c.x - a.x) / 6, b.y + (c.y - a.y) / 6},
            {c.x + (b.x - d.x) / 6, c.y + (b.y - d.y) / 6}, c);
    }
}

// path-data-parser divides circular SVG arcs into cubic segments of at most
// 120 degrees. Only zero-axis-rotation circular arcs are used by Venn.js.
void arc_cubics(std::vector<Cubic>& curves, const RoughArc& arc) {
    auto [start, end, radius, large, sweep] = arc;
    if (radius == 0) {
        curves.push_back({start, start, end, end});
        return;
    }
    if (same(start, end)) return;
    const auto x = (start.x - end.x) / 2, y = (start.y - end.y) / 2;
    const auto h = square(x) / square(radius) + square(y) / square(radius);
    if (h > 1) radius *= std::sqrt(h);
    const auto rr = square(radius);
    const auto k = (large == sweep ? -1 : 1) *
        std::sqrt(std::abs((rr * rr - rr * y * y - rr * x * x) / (rr * y * y + rr * x * x)));
    const Point center{k * radius * y / radius + (start.x + end.x) / 2,
                      k * -radius * x / radius + (start.y + end.y) / 2};
    auto angle = [&](Point p) {
        auto f = std::asin(std::clamp(js_round((p.y - center.y) / radius * 1e9) / 1e9, -1.0, 1.0));
        if (p.x < center.x) f = pi - f;
        return f < 0 ? tau + f : f;
    };
    auto f1 = angle(start), f2 = angle(end);
    if (sweep && f1 > f2) f1 -= tau;
    if (!sweep && f2 > f1) f2 -= tau;
    constexpr auto limit = pi * 120 / 180;
    auto append = [&](double from, double to, Point a, Point b) {
        const auto t = 4.0 / 3 * radius * std::tan((to - from) / 4);
        const Point c1{2 * a.x - (a.x + t * std::sin(from)), 2 * a.y - (a.y - t * std::cos(from))};
        const Point c2{b.x + t * std::sin(to), b.y - t * std::cos(to)};
        curves.push_back({a, c1, c2, b});
    };
    while (std::abs(f2 - f1) > limit) {
        const auto next = f1 + (sweep && f2 > f1 ? limit : -limit);
        const Point p{center.x + radius * std::cos(next), center.y + radius * std::sin(next)};
        append(f1, next, start, p);
        start = p;
        f1 = next;
    }
    append(f1, f2, start, end);
}

Point lerp(Point a, Point b, double t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

void flatten_curve(const Cubic& curve, Polygon& out) {
    const auto [a, b, c, d] = curve;
    const auto flatness_x = std::max(square(3 * b.x - 2 * a.x - d.x), square(3 * c.x - 2 * d.x - a.x));
    const auto flatness_y = std::max(square(3 * b.y - 2 * a.y - d.y), square(3 * c.y - 2 * d.y - a.y));
    const auto flatness = flatness_x + flatness_y;
    if (!std::isfinite(flatness) || out.size() >= max_scan_rows)
        throw std::runtime_error("RoughJS curve exceeds subdivision limit");
    if (flatness < 1) {
        if (out.empty() || distance_squared(out.back(), a) > 1) out.push_back(a);
        out.push_back(d);
        return;
    }
    const auto ab = lerp(a, b, 0.5), bc = lerp(b, c, 0.5), cd = lerp(c, d, 0.5);
    const auto abc = lerp(ab, bc, 0.5), bcd = lerp(bc, cd, 0.5), mid = lerp(abc, bcd, 0.5);
    flatten_curve({a, ab, abc, mid}, out);
    flatten_curve({mid, bcd, cd, d}, out);
}

void simplify(std::span<const Point> points, Polygon& out) {
    const auto a = points.front(), b = points.back();
    const auto length = distance_squared(a, b);
    std::size_t furthest = 0;
    double max_distance = 0;
    for (std::size_t i = 1; i + 1 < points.size(); ++i) {
        const auto p = points[i];
        const auto t = length == 0 ? 0.0 :
            ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / length;
        const auto distance = distance_squared(p, lerp(a, b, std::clamp(t, 0.0, 1.0)));
        if (distance > max_distance) {
            max_distance = distance;
            furthest = i;
        }
    }
    if (std::sqrt(max_distance) > (1 + roughness) / 2) {
        simplify(points.first(furthest + 1), out);
        simplify(points.subspan(furthest), out);
    } else {
        if (out.empty()) out.push_back(a);
        out.push_back(b);
    }
}
}

RoughEllipsePaths rough_ellipse_hachure(Point center, double width, double height,
    double angle, std::uint32_t seed) {
    Random random(seed);
    auto rx = std::abs(width / 2), ry = std::abs(height / 2);
    const auto psq = std::sqrt(tau * std::sqrt((square(rx) + square(ry)) / 2));
    const auto steps = std::ceil(std::max(9.0, (9 / std::sqrt(200.0)) * psq));
    if (!std::isfinite(steps) || steps > max_scan_rows)
        throw std::runtime_error("RoughJS ellipse exceeds subdivision limit");
    const auto increment = tau / steps;
    rx += random.offset(rx * (1 - 0.95));
    ry += random.offset(ry * (1 - 0.95));
    const auto overlap = increment * random.between(0.1, random.between(0.4, 1));
    RoughEllipsePaths result;
    Polygon estimated;
    ellipse_curve(result.stroke_path, ellipse_points(center, rx, ry, increment, 1, overlap, random, &estimated));
    ellipse_curve(result.stroke_path, ellipse_points(center, rx, ry, increment, 1.5, 0, random, nullptr));
    hatch(result.fill_path, estimated, 8, angle, random);
    return result;
}

std::string rough_arc_cross_hatch(std::span<const RoughArc> arcs, std::uint32_t seed) {
    std::vector<Cubic> curves;
    for (const auto& arc : arcs) arc_cubics(curves, arc);
    if (curves.empty()) return {};
    Polygon points{curves.front().front()};
    Polygon flattened;
    for (const auto& c : curves) flatten_curve(c, flattened);
    points.insert(points.end(), flattened.begin(), flattened.end());
    Polygon simplified;
    simplify(points, simplified);

    // RoughGenerator consumes the unpainted SVG stroke's random draws before
    // producing the cross-hatch. A cubic uses six draws, then eight more.
    Random random(seed);
    for (std::size_t i = 0; i < curves.size(); ++i)
        for (int draw = 0; draw < 14; ++draw) random.next();
    std::string result;
    hatch(result, simplified, 6, 60, random);
    hatch(result, simplified, 6, 150, random);
    return result;
}
}
