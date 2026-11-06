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
