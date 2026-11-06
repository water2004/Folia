// Mermaid 12.2.0 linedWaveEdgedRect (Agentflow refdoc), classic zero-roughness
// outline. Bounds derive from the measured label box plus Neo shape padding.
std::vector<FlowchartPoint> lined_document_points(double width, double height, bool neo) {
    const double w = width / 1.1, h = height / (neo ? 1.5 : 1.25);
    const double amplitude = neo ? h / 4 : h / 8, final_height = h + amplitude;
    const double x1 = -w / 2 - w * 0.05, x2 = w / 2 + w * 0.05;
    const double y1 = -final_height / 2, y2 = final_height / 2;
    std::vector<FlowchartPoint> points{{x1, y1}, {x1, y2}};
    const double delta_x = x2 - x1, cycle_length = delta_x / 0.8;
    const double frequency = 2 * std::numbers::pi / cycle_length, wave_y = final_height / 2;
    for (int i = 0; i <= 50; ++i) {
        const double t = static_cast<double>(i) / 50;
        const double x = x1 + t * delta_x;
        points.push_back({x, wave_y + amplitude * std::sin(frequency * (x - x1))});
    }
    points.push_back({x2, y1});
    points.push_back({x1, y1});
    points.push_back({-w / 2, y1});
    points.push_back({-w / 2, y2 * 1.1});
    points.push_back({-w / 2, y1});
    for (auto &p : points)
        p.y -= amplitude / 2;
    return points;
}
std::string lined_document_path(double width, double height, bool neo) {
    const auto points = lined_document_points(width, height, neo);
    std::string path;
    for (std::size_t i = 0; i < points.size(); ++i)
        path += (i == 0 ? "M" : "L") + number(points[i].x) + "," + number(points[i].y);
    return path;
}
