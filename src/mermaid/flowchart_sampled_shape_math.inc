// Mermaid 12.2.0 shapes/util.ts wave/arc helpers, with the same sample order.
// This file is included from the Flowchart SVG implementation's private scope.
void append_flowchart_sine_wave(std::vector<FlowchartPoint>& points,
    FlowchartPoint start, FlowchartPoint end, double amplitude, double cycles) {
    constexpr int steps = 50;
    const double delta_x = end.x - start.x;
    const double mid_y = start.y + (end.y - start.y) / 2;
    const double frequency = 2 * std::numbers::pi / (delta_x / cycles);
    for (int i = 0; i <= steps; ++i) {
        const auto x = start.x + static_cast<double>(i) / steps * delta_x;
        points.push_back({x, mid_y + amplitude * std::sin(frequency * (x - start.x))});
    }
}
void append_flowchart_arc_samples(std::vector<FlowchartPoint>& points, FlowchartPoint center,
    double radius, int count, double start_degrees, double end_degrees, bool negate) {
    const double start = start_degrees * std::numbers::pi / 180;
    const double step = (end_degrees * std::numbers::pi / 180 - start) / (count - 1);
    for (int i = 0; i < count; ++i) {
        const double a = start + i * step;
        const FlowchartPoint p{center.x + radius * std::cos(a), center.y + radius * std::sin(a)};
        points.push_back(negate ? FlowchartPoint{-p.x, -p.y} : p);
    }
}

// SVG elliptical arc endpoint-to-center conversion for a short arc in either
// sweep direction. Test derivative zeros as well as both endpoints.
void short_svg_arc_bounds(RoughBounds& bounds, FlowchartPoint from, FlowchartPoint to,
                      double rx, double ry, double rotation, bool positive_sweep=true) {
    const auto include = [&](FlowchartPoint p) {
        bounds.left = std::min(bounds.left, p.x);
        bounds.top = std::min(bounds.top, p.y);
        bounds.right = std::max(bounds.right, p.x);
        bounds.bottom = std::max(bounds.bottom, p.y);
    };
    include(to);
    if (rx == 0 || ry == 0 || (from.x == to.x && from.y == to.y)) return;

    const double cosine = std::cos(rotation), sine = std::sin(rotation);
    const double dx = (from.x - to.x) / 2, dy = (from.y - to.y) / 2;
    const double x = cosine * dx + sine * dy, y = -sine * dx + cosine * dy;
    const double radii_scale = std::hypot(x / rx, y / ry);
    if (radii_scale > 1) {
        rx *= radii_scale;
        ry *= radii_scale;
    }
    const double nx = x / rx, ny = y / ry;
    const double distance_squared = nx * nx + ny * ny;
    if (distance_squared == 0) return;
    const double coefficient = (positive_sweep ? 1 : -1) *
        std::sqrt(std::max(0.0, 1 / distance_squared - 1));
    const double cx = coefficient * ny * rx, cy = -coefficient * nx * ry;
    const double center_x = cosine * cx - sine * cy + (from.x + to.x) / 2;
    const double center_y = sine * cx + cosine * cy + (from.y + to.y) / 2;
    constexpr double revolution = 2 * std::numbers::pi;
    const auto positive_angle = [](double angle) {
        const auto normalized = std::fmod(angle, revolution);
        return normalized < 0 ? normalized + revolution : normalized;
    };
    const double start = std::atan2((y - cy) / ry, (x - cx) / rx);
    const double end = std::atan2((-y - cy) / ry, (-x - cx) / rx);
    const double sweep = positive_angle(positive_sweep ? end - start : start - end);
    const double dx_zero = std::atan2(-ry * sine, rx * cosine);
    const double dy_zero = std::atan2(ry * cosine, rx * sine);
    for (const auto angle : {dx_zero, dx_zero + std::numbers::pi, dy_zero, dy_zero + std::numbers::pi}) {
        if (positive_angle(positive_sweep ? angle - start : start - angle) > sweep) continue;
        include({center_x + rx * cosine * std::cos(angle) - ry * sine * std::sin(angle),
                 center_y + rx * sine * std::cos(angle) + ry * cosine * std::sin(angle)});
    }
}
