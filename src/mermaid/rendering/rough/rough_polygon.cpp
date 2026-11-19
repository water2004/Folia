module folia.mermaid.rough_polygon;
import std;
import folia.mermaid.number;

namespace folia::mermaid {
namespace {
using Point = RoughPoint;
using Polygon = std::vector<Point>;
using Cubic = std::array<Point, 4>;
using Line = std::array<Point, 2>;
constexpr double randomness = 2;
constexpr std::size_t limit = 1'000'000;

double square(double x) { return x * x; }
double distance_sq(Point a, Point b) { return square(a.x - b.x) + square(a.y - b.y); }
bool same(Point a, Point b) { return a.x == b.x && a.y == b.y; }
Point lerp(Point a, Point b, double t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

class Random {
public:
    explicit Random(const RoughPolygonOptions& options)
        : state_(options.seed ? options.seed : 1), roughness_(options.roughness) {}
    double next() {
        state_ *= 48271u;
        return (state_ & 0x7fffffffu) / 2147483648.0;
    }
    double offset(double max, double gain = 1) {
        return roughness_ * gain * (next() * (2 * max) - max);
    }
    double between(double min,double max) {
        return roughness_ * (next() * (max - min) + min);
    }
    void set_roughness(double value) { roughness_ = value; }
private:
    std::uint32_t state_;
    double roughness_;
};

std::string number(double x) {
    auto result = format_number(x);
    if (!result) throw std::runtime_error("Invalid RoughJS polygon coordinate");
    return std::move(*result);
}
std::string point(Point p) { return number(p.x) + " " + number(p.y); }

class Paths {
public:
    explicit Paths(std::string& output) : path(output) {}
    std::string& path;
    std::optional<RoughBounds> bounds;

    void move(Point p) {
        if (!path.empty()) path += ' ';
        path += "M" + point(p);
        last_ = p;
    }
    void line(Point p) {
        path += " L" + point(p);
        include(last_);
        include(p);
        last_ = p;
    }
    void curve(Point a, Point b, Point end) {
        path += " C" + point(a) + ", " + point(b) + ", " + point(end);
        include(last_);
        include(end);
        extrema(last_.x, a.x, b.x, end.x, true);
        extrema(last_.y, a.y, b.y, end.y, false);
        last_ = end;
    }
private:
    Point last_{};
    void include(Point p) {
        if (!bounds) bounds.emplace(RoughBounds{p.x, p.y, p.x, p.y});
        else {
            bounds->left = std::min(bounds->left, p.x);
            bounds->top = std::min(bounds->top, p.y);
            bounds->right = std::max(bounds->right, p.x);
            bounds->bottom = std::max(bounds->bottom, p.y);
        }
    }
    void extrema(double p0, double p1, double p2, double p3, bool horizontal) {
        const auto a = -p0 + 3 * p1 - 3 * p2 + p3;
        const auto b = 2 * (p0 - 2 * p1 + p2);
        const auto c = p1 - p0;
        auto evaluate = [&](double t) {
            if (t <= 0 || t >= 1) return;
            const auto s = 1 - t;
            const auto p = s * s * s * p0 + 3 * s * s * t * p1 + 3 * s * t * t * p2 + t * t * t * p3;
            auto& low = horizontal ? bounds->left : bounds->top;
            auto& high = horizontal ? bounds->right : bounds->bottom;
            low = std::min(low, p);
            high = std::max(high, p);
        };
        if (a == 0) {
            if (b != 0) evaluate(-c / b);
        } else if (const auto discriminant = b * b - 4 * a * c; discriminant >= 0) {
            const auto root = std::sqrt(discriminant);
            evaluate((-b + root) / (2 * a));
            evaluate((-b - root) / (2 * a));
        }
    }
};

void line(Paths& paths, Point a, Point b, Random& random, bool merged = false) {
    const auto dx = b.x - a.x, dy = b.y - a.y;
    const auto length_sq = square(dx) + square(dy);
    const auto length = std::sqrt(length_sq);
    const auto gain = length < 200 ? 1.0 : length > 500 ? 0.4 : -0.0016668 * length + 1.233334;
    const auto offset = square(randomness) * 100 > length_sq ? length / 10 : randomness;
    for (auto jitter : {offset, offset / 2}) {
        const auto diverge = 0.2 + random.next() * 0.2;
        const auto bow_x = random.offset(randomness * dy / 200, gain);
        const auto bow_y = random.offset(randomness * -dx / 200, gain);
        auto noise = [&] { return random.offset(jitter, gain); };
        const Point start{a.x + noise(), a.y + noise()};
        if (!merged || paths.path.empty()) paths.move(start);
        const Point first{bow_x + a.x + dx * diverge + noise(), bow_y + a.y + dy * diverge + noise()};
        const Point second{bow_x + a.x + 2 * dx * diverge + noise(), bow_y + a.y + 2 * dy * diverge + noise()};
        const Point end{b.x + noise(), b.y + noise()};
        paths.curve(first, second, end);
        if (merged) break;
    }
}
void bezier(Paths& paths, Cubic curve, Random& random, bool merged = false) {
    const auto [start, first, second, end] = curve;
    auto jitter = [&](Point p, double magnitude) {
        return Point{p.x + random.offset(magnitude), p.y + random.offset(magnitude)};
    };
    for (bool overlay : {false, true}) {
        const auto magnitude = randomness + (overlay ? 0.3 : 0);
        if (!merged || paths.path.empty()) paths.move(overlay ? jitter(start, randomness) : start);
        const auto e = jitter(end, magnitude);
        const auto a = jitter(first, magnitude);
        const auto b = jitter(second, magnitude);
        paths.curve(a, b, e);
        if (merged) break;
    }
}

// hachure-fill 0.5.2 active-edge scan conversion. Keep the edge's x at
// insertion unchanged until the following row, as in the upstream filler.
void hatch(Paths& paths, const std::vector<Polygon>& polygons, const RoughPolygonOptions& options, Random& random) {
    const auto angle = (std::numbers::pi / 180) * (options.hachure_angle + 90);
    const auto cos = std::cos(angle), sin = std::sin(angle);
    auto rotate = [&](Point p, double s) {
        return Point{p.x * cos - p.y * s, p.x * s + p.y * cos};
    };
    struct Edge { double ymin, ymax, x, slope; };
    std::vector<Edge> edges;
    for (const auto& polygon : polygons) {
        if (polygon.size() < 2) continue;
        for (std::size_t i = 0; i < polygon.size(); ++i) {
            const auto a = rotate(polygon[i], sin), b = rotate(polygon[(i + 1) % polygon.size()], sin);
            if (a.y == b.y) continue;
            const auto low = a.y < b.y ? a : b, high = a.y < b.y ? b : a;
            edges.push_back({low.y, high.y, low.x, (b.x - a.x) / (b.y - a.y)});
        }
    }
    if (edges.empty()) return;
    std::ranges::stable_sort(edges, {}, [](const auto& e) { return std::tuple{e.ymin, e.x, e.ymax}; });
    const auto gap = std::floor(std::max(options.hachure_gap, 0.1) + 0.5);
    const auto step = options.roughness >= 1 && random.next() > 0.7 && gap > 0 ? gap : 1;
    std::vector<Edge> active;
    auto next = edges.begin();
    auto y = next->ymin;
    for (std::size_t row = 0; !active.empty() || next != edges.end(); ++row, y += step) {
        if (row == limit) throw std::runtime_error("RoughJS hatch exceeds scanline limit");
        while (next != edges.end() && next->ymin <= y) active.push_back(*next++);
        std::erase_if(active, [y](const auto& e) { return e.ymax <= y; });
        std::ranges::stable_sort(active, {}, &Edge::x);
        if (step != 1 || (gap > 0 && std::fmod(static_cast<double>(row), gap) == 0)) {
            for (std::size_t i = 1; i < active.size(); i += 2) {
                const auto a = rotate({std::floor(active[i - 1].x + 0.5), y}, -sin);
                const auto b = rotate({std::floor(active[i].x + 0.5), y}, -sin);
                line(paths, a, b, random);
            }
        }
        for (auto& e : active) e.x += step * e.slope;
    }
}

void flatten(const Cubic& curve, Polygon& points) {
    const auto [a, b, c, d] = curve;
    const auto fx = std::max(square(3 * b.x - 2 * a.x - d.x), square(3 * c.x - 2 * d.x - a.x));
    const auto fy = std::max(square(3 * b.y - 2 * a.y - d.y), square(3 * c.y - 2 * d.y - a.y));
    if (!std::isfinite(fx + fy) || points.size() >= limit)
        throw std::runtime_error("RoughJS curve exceeds subdivision limit");
    if (fx + fy < 1) {
        if (points.empty() || distance_sq(points.back(), a) > 1) points.push_back(a);
        points.push_back(d);
        return;
    }
    const auto ab = lerp(a, b, .5), bc = lerp(b, c, .5), cd = lerp(c, d, .5);
    const auto abc = lerp(ab, bc, .5), bcd = lerp(bc, cd, .5), mid = lerp(abc, bcd, .5);
    flatten({a, ab, abc, mid}, points);
    flatten({mid, bcd, cd, d}, points);
}
void simplify(std::span<const Point> points, double tolerance, Polygon& out) {
    const auto a = points.front(), b = points.back();
    const auto length = distance_sq(a, b);
    double farthest = 0;
    std::size_t index = 0;
    for (std::size_t i = 1; i + 1 < points.size(); ++i) {
        const auto p = points[i];
        const auto t = length == 0 ? 0.0 :
            ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / length;
        const auto d = distance_sq(p, lerp(a, b, std::clamp(t, 0.0, 1.0)));
        if (d > farthest) { farthest = d; index = i; }
    }
    if (std::sqrt(farthest) > tolerance) {
        simplify(points.first(index + 1), tolerance, out);
        simplify(points.subspan(index), tolerance, out);
    } else {
        if (out.empty()) out.push_back(a);
        out.push_back(b);
    }
}

void fill_polygons(Paths& paths, const std::vector<Polygon>& polygons,
    const RoughPolygonOptions& options, Random& random) {
    if (options.fill == RoughFill::hachure) {
        hatch(paths, polygons, options, random);
    } else if (options.fill == RoughFill::solid) {
        auto jitter = [&](Point p) {
            return Point{p.x + random.offset(randomness), p.y + random.offset(randomness)};
        };
        for (const auto& polygon : polygons) {
            if (polygon.size() < 3) continue;
            paths.move(jitter(polygon.front()));
            for (auto p : polygon | std::views::drop(1)) paths.line(jitter(p));
        }
    }
}

std::optional<RoughBounds> merge(std::optional<RoughBounds> a, std::optional<RoughBounds> b) {
    if (!a) return b;
    if (!b) return a;
    return RoughBounds{std::min(a->left, b->left), std::min(a->top, b->top),
        std::max(a->right, b->right), std::max(a->bottom, b->bottom)};
}

void ellipse_curve(Paths& paths, const Polygon& points) {
    paths.move(points[1]);
    for (std::size_t i=1;i+2<points.size();++i) {
        const auto a=points[i-1],b=points[i],c=points[i+1],d=points[i+2];
        paths.curve({b.x+(c.x-a.x)/6,b.y+(c.y-a.y)/6},
            {c.x+(b.x-d.x)/6,c.y+(b.y-d.y)/6},c);
    }
}

Polygon ellipse_points(Point center,double rx,double ry,double step,double jitter,double overlap,
    Random& random,Polygon* estimated,bool core_only) {
    constexpr auto pi=std::numbers::pi;
    constexpr auto tau=2*pi;
    auto at=[&](double angle,double scale) -> Point {
        if(core_only) return {center.x+rx*std::cos(angle),center.y+ry*std::sin(angle)};
        return {random.offset(jitter)+center.x+scale*rx*std::cos(angle),
                random.offset(jitter)+center.y+scale*ry*std::sin(angle)};
    };
    if(core_only) {
        step/=4;
        Polygon points{at(-step,1)};
        for(auto a=0.0;a<=tau;a+=step) {
            points.push_back(at(a,1));
            if(estimated) estimated->push_back(points.back());
        }
        points.push_back(at(0,1));
        points.push_back(at(step,1));
        return points;
    }
    const auto start=random.offset(.5)-pi/2;
    Polygon points{at(start-step,.9)};
    for(auto a=start;a<tau+start-.01;a+=step) {
        points.push_back(at(a,1));
        if(estimated) estimated->push_back(points.back());
    }
    points.push_back(at(start+tau+overlap*.5,1));
    points.push_back(at(start+overlap,.98));
    points.push_back(at(start+overlap*.5,.9));
    return points;
}

Cubic to_cubic(Point start, const RoughCommand& cmd) {
    if (cmd.verb == RoughVerb::cubic) return {start, cmd.first_control, cmd.second_control, cmd.end};
    const auto control = cmd.first_control, end = cmd.end;
    return {start, {start.x + 2 * (control.x - start.x) / 3, start.y + 2 * (control.y - start.y) / 3},
        {end.x + 2 * (control.x - end.x) / 3, end.y + 2 * (control.y - end.y) / 3}, end};
}

void merged_fill(Paths& fill, std::span<const RoughCommand> commands, Random& random) {
    Point current{}, start{};
    for (const auto& command : commands) {
        switch (command.verb) {
        case RoughVerb::move:
            current = start = command.end;
            break;
        case RoughVerb::line:
            line(fill, current, command.end, random, true);
            current = command.end;
            break;
        case RoughVerb::quadratic:
        case RoughVerb::cubic:
            bezier(fill, to_cubic(current, command), random, true);
            current = command.end;
            break;
        case RoughVerb::close:
            line(fill, current, start, random, true);
            current = start;
            break;
        }
    }
}

// The pinned path-data-parser normalizes SVG arcs into cubic pieces of at
// most 120 degrees, which RoughJS then draws as ordinary cubic segments.
void arc_commands(std::vector<RoughCommand>& out, Point start, Point end,
    double rx, double ry, double angle, bool large, bool sweep) {
    rx = std::abs(rx);
    ry = std::abs(ry);
    if (rx == 0 || ry == 0) {
        out.push_back({RoughVerb::cubic, end, start, end});
        return;
    }
    if (same(start, end)) return;
    constexpr auto pi = std::numbers::pi;
    constexpr auto tau = 2 * pi;
    const auto radians = pi * angle / 180;
    const auto sin = std::sin(radians), cos = std::cos(radians);
    auto rotate = [&](Point p, double s) {
        return Point{p.x * cos - p.y * s, p.x * s + p.y * cos};
    };
    start = rotate(start, -sin);
    end = rotate(end, -sin);
    const auto x = (start.x - end.x) / 2, y = (start.y - end.y) / 2;
    const auto h = square(x) / square(rx) + square(y) / square(ry);
    if (h > 1) {
        const auto scale = std::sqrt(h);
        rx *= scale;
        ry *= scale;
    }
    const auto xx = square(rx), yy = square(ry);
    const auto k = (large == sweep ? -1 : 1) *
        std::sqrt(std::abs((xx * yy - xx * y * y - yy * x * x) / (xx * y * y + yy * x * x)));
    const Point center{k * rx * y / ry + (start.x + end.x) / 2,
                       k * -ry * x / rx + (start.y + end.y) / 2};
    auto parameter = [&](Point p) {
        const auto ratio = std::round((p.y - center.y) / ry * 1e9) / 1e9;
        auto f = std::asin(std::clamp(ratio, -1.0, 1.0));
        if (p.x < center.x) f = pi - f;
        return f < 0 ? tau + f : f;
    };
    auto f1 = parameter(start), f2 = parameter(end);
    if (sweep && f1 > f2) f1 -= tau;
    if (!sweep && f2 > f1) f2 -= tau;
    constexpr auto max_angle = pi * 120 / 180;
    auto append = [&](double from, double to, Point a, Point b) {
        const auto t = std::tan((to - from) / 4);
        const auto hx = 4.0 / 3 * rx * t, hy = 4.0 / 3 * ry * t;
        const Point c1{2 * a.x - (a.x + hx * std::sin(from)), 2 * a.y - (a.y - hy * std::cos(from))};
        const Point c2{b.x + hx * std::sin(to), b.y - hy * std::cos(to)};
        out.push_back({RoughVerb::cubic, rotate(b, sin), rotate(c1, sin), rotate(c2, sin)});
    };
    while (std::abs(f2 - f1) > max_angle) {
        const auto next = f1 + (sweep && f2 > f1 ? max_angle : -max_angle);
        const Point p{center.x + rx * std::cos(next), center.y + ry * std::sin(next)};
        append(f1, next, start, p);
        start = p;
        f1 = next;
    }
    append(f1, f2, start, end);
}

std::vector<RoughCommand> parse_path(std::string_view path) {
    auto skip_separators = [&] {
        const auto pos = path.find_first_not_of(" ,\t\r\n");
        path.remove_prefix(pos == std::string_view::npos ? path.size() : pos);
    };
    auto scalar = [&] {
        skip_separators();
        if (path.empty()) throw std::runtime_error("Missing RoughJS SVG path coordinate");
        if (path.front() == '+') path.remove_prefix(1);
        double value = 0;
        const auto parsed = std::from_chars(path.data(), path.data() + path.size(), value);
        if (parsed.ec != std::errc{} || !std::isfinite(value))
            throw std::runtime_error("Invalid RoughJS SVG path coordinate");
        path.remove_prefix(static_cast<std::size_t>(parsed.ptr - path.data()));
        return value;
    };
    auto pair = [&]() -> Point { return {scalar(), scalar()}; };
    std::vector<RoughCommand> out;
    Point current{}, start{};
    char verb = 0;
    for (skip_separators(); !path.empty(); skip_separators()) {
        const auto first = path.front();
        if ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')) {
            verb = first;
            path.remove_prefix(1);
        }
        switch (verb) {
        case 'M':
            current = start = pair();
            out.push_back({RoughVerb::move, current});
            verb = 'L';
            break;
        case 'H':
            current.x = scalar();
            out.push_back({RoughVerb::line, current});
            break;
        case 'V':
            current.y = scalar();
            out.push_back({RoughVerb::line, current});
            break;
        case 'L':
            current = pair();
            out.push_back({RoughVerb::line, current});
            break;
        case 'Q': {
            const auto control = pair();
            current = pair();
            out.push_back({RoughVerb::quadratic, current, control});
            break;
        }
        case 'C': {
            const auto a = pair(), b = pair();
            current = pair();
            out.push_back({RoughVerb::cubic, current, a, b});
            break;
        }
        case 'A': {
            const auto rx = scalar(), ry = scalar(), angle = scalar();
            const auto large = scalar() != 0, sweep = scalar() != 0;
            const auto end = pair();
            arc_commands(out, current, end, rx, ry, angle, large, sweep);
            current = end;
            break;
        }
        case 'Z':
        case 'z':
            out.push_back({RoughVerb::close});
            current = start;
            verb = 0;
            break;
        default:
            throw std::runtime_error("Unsupported RoughJS SVG path command");
        }
    }
    return out;
}
}

RoughPolygonResult sketch_polygon(std::span<const RoughPoint> points, const RoughPolygonOptions& options) {
    RoughPolygonResult result;
    Paths outline{result.stroke_path}, fill{result.fill_path};
    Random random(options);
    for (std::size_t i = 1; i < points.size(); ++i) line(outline, points[i - 1], points[i], random);
    if (points.size() > 2) line(outline, points.back(), points.front(), random);
    if (options.fill != RoughFill::none) fill_polygons(fill, {Polygon(points.begin(), points.end())}, options, random);
    if (!options.stroke) result.stroke_path.clear();
    result.bounds = merge(fill.bounds, options.stroke ? outline.bounds : std::nullopt);
    return result;
}

RoughPolygonResult sketch_rectangle(double x, double y, double width, double height,
    const RoughPolygonOptions& options) {
    return sketch_polygon(std::array{Point{x, y}, Point{x + width, y},
        Point{x + width, y + height}, Point{x, y + height}}, options);
}

RoughPolygonResult sketch_ellipse(RoughPoint center,double width,double height,
    const RoughPolygonOptions& options) {
    RoughPolygonResult result;
    Paths outline{result.stroke_path},fill{result.fill_path};
    Random random(options);
    auto rx=std::abs(width/2),ry=std::abs(height/2);
    constexpr auto tau=2*std::numbers::pi;
    const auto count=std::ceil(std::max(9.0,(9/std::sqrt(200.0))*std::sqrt(tau*std::sqrt((square(rx)+square(ry))/2))));
    if(!std::isfinite(count)||count>limit) throw std::runtime_error("RoughJS ellipse exceeds subdivision limit");
    const auto step=tau/count;
    rx+=random.offset(rx*(1-.95));
    ry+=random.offset(ry*(1-.95));
    const auto core_only=options.roughness==0;
    auto ellipse=[&](Paths& paths,Polygon* estimated) {
        const auto overlap=step*random.between(.1,random.between(.4,1));
        ellipse_curve(paths,ellipse_points(center,rx,ry,step,1,overlap,random,estimated,core_only));
        if(!core_only) ellipse_curve(paths,ellipse_points(center,rx,ry,step,1.5,0,random,nullptr,false));
    };
    Polygon estimated;
    ellipse(outline,&estimated);
    if(options.fill==RoughFill::solid) ellipse(fill,nullptr);
    else if(options.fill==RoughFill::hachure) hatch(fill,{std::move(estimated)},options,random);
    if(!options.stroke) result.stroke_path.clear();
    result.bounds=merge(fill.bounds,options.stroke?outline.bounds:std::nullopt);
    return result;
}

RoughPolygonResult sketch_path(std::span<const RoughCommand> commands, const RoughPolygonOptions& options) {
    RoughPolygonResult result;
    Paths outline{result.stroke_path}, fill{result.fill_path};
    Random random(options);
    const bool filled = options.fill != RoughFill::none;
    Point current{}, start{};
    Polygon vertices, pending;
    std::vector<Polygon> polygons;
    auto flush_curves = [&] {
        vertices.insert(vertices.end(), pending.begin(), pending.end());
        pending.clear();
    };
    auto finish_polygon = [&] {
        flush_curves();
        if (vertices.empty()) return;
        Polygon simplified;
        simplify(vertices, (1 + options.roughness) / 2, simplified);
        polygons.push_back(std::move(simplified));
        vertices.clear();
    };
    for (const auto& command : commands) {
        switch (command.verb) {
        case RoughVerb::move:
            current = start = command.end;
            if (filled) { finish_polygon(); vertices.push_back(start); }
            break;
        case RoughVerb::line:
            line(outline, current, command.end, random);
            current = command.end;
            if (filled) { flush_curves(); vertices.push_back(current); }
            break;
        case RoughVerb::quadratic:
        case RoughVerb::cubic: {
            const auto curve = to_cubic(current, command);
            bezier(outline, curve, random);
            if (filled) flatten(curve, pending);
            current = command.end;
            break;
        }
        case RoughVerb::close:
            line(outline, current, start, random);
            current = start;
            if (filled) { flush_curves(); vertices.push_back(start); }
            break;
        }
    }
    if (filled) {
        finish_polygon();
        if (options.fill == RoughFill::solid && polygons.size() == 1) {
            random.set_roughness(options.roughness == 0 ? 0 : options.roughness + 0.8);
            merged_fill(fill, commands, random);
        } else {
            fill_polygons(fill, polygons, options, random);
        }
    }
    if (!options.stroke) result.stroke_path.clear();
    result.bounds = merge(fill.bounds, options.stroke ? outline.bounds : std::nullopt);
    return result;
}

RoughPolygonResult sketch_svg_path(std::string_view path, const RoughPolygonOptions& options) {
    return sketch_path(parse_path(path), options);
}
}
