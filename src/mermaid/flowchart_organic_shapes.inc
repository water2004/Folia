// Mermaid 12.2.0 person.ts, cloud.ts and generateCirclePoints. MIT.
// The native layout receives the same painted bounding box as getBBox().
struct FlowchartOrganicGeometry {
    FlowchartNodeBox box;
    RoughBounds bounds{};
    FlowchartPoint label_offset{}, path_offset{};
    double person_body_top = 0, person_body_height = 0, person_body_radius = 0;
    double person_head_y = 0, person_head_radius = 0;
    std::string cloud_path;
};

bool organic_flowchart_shape(std::u16string_view shape) {
    return shape == u"person" || shape == u"cloud";
}

FlowchartOrganicGeometry person_geometry(double width, double height) {
    FlowchartOrganicGeometry out;
    out.box = {width, height};
    out.bounds = {-width / 2, -height / 2, width / 2, height / 2};
    out.person_head_radius = std::clamp(width * .23, 16.0, 56.0);
    out.person_head_y = out.bounds.top + out.person_head_radius;
    out.person_body_top = out.bounds.top + (2 - .27) * out.person_head_radius;
    out.person_body_height = height - (2 - .27) * out.person_head_radius;
    out.person_body_radius = std::min(width * .177, out.person_body_height * .45);
    out.label_offset.y = out.person_body_top + out.person_body_height / 2;
    return out;
}

std::vector<FlowchartPoint> person_outline(double width, double height) {
    const auto shape = person_geometry(width, height);
    const auto radius = shape.person_body_radius;
    constexpr double degrees = std::numbers::pi / 180;
    std::vector<FlowchartPoint> outline;
    outline.reserve(72);
    const auto arc = [&](double x, double y, double r, int count, double start, double end) {
        const auto first = start * degrees;
        const auto step = (end * degrees - first) / (count - 1);
        for (int i = 0; i < count; ++i) {
            const auto angle = first + i * step;
            outline.push_back({-(x + r * std::cos(angle)), -(y + r * std::sin(angle))});
        }
    };
    const auto head_angle = std::asin(std::min(1.0,
        (shape.person_body_top - shape.person_head_y) / shape.person_head_radius)) / degrees;
    const auto left = width / 2 - radius;
    const auto top = -(shape.person_body_top + radius);
    const auto bottom = -(height / 2 - radius);
    arc(0, -shape.person_head_y, shape.person_head_radius, 24, 180 + head_angle, -head_angle);
    arc(left, top, radius, 12, 90, 0);
    arc(left, bottom, radius, 12, 360, 270);
    arc(-left, bottom, radius, 12, 270, 180);
    arc(-left, top, radius, 12, 180, 90);
    return outline;
}

// SVG elliptical arc endpoint-to-center conversion for the cloud's short,
// positive sweep arcs. Test derivative zeros as well as both endpoints.
void cloud_arc_bounds(RoughBounds& bounds, FlowchartPoint from, FlowchartPoint to,
                      double rx, double ry, double rotation) {
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
    const double coefficient = std::sqrt(std::max(0.0, 1 / distance_squared - 1));
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
    const double sweep = positive_angle(end - start);
    const double dx_zero = std::atan2(-ry * sine, rx * cosine);
    const double dy_zero = std::atan2(ry * cosine, rx * sine);
    for (const auto angle : {dx_zero, dx_zero + std::numbers::pi, dy_zero, dy_zero + std::numbers::pi}) {
        if (positive_angle(angle - start) > sweep) continue;
        include({center_x + rx * cosine * std::cos(angle) - ry * sine * std::sin(angle),
                 center_y + rx * sine * std::cos(angle) + ry * cosine * std::sin(angle)});
    }
}

FlowchartOrganicGeometry cloud_geometry(double width, double height) {
    FlowchartOrganicGeometry out;
    out.path_offset = {-width / 2, -height / 2};
    const double r1 = .15 * width, r2 = .25 * width, r3 = .35 * width, r4 = .2 * width;
    struct Arc { double rx, ry, rotation; FlowchartPoint delta; };
    const Arc arcs[] = {
        {r1, r1, 0, {.25 * width, -.1 * width}},
        {r3, r3, 1, {.4 * width, -.1 * width}},
        {r2, r2, 1, {.35 * width, .2 * width}},
        {r1, r1, 1, {.15 * width, .35 * height}},
        {r4, r4, 1, {-.15 * width, .65 * height}},
        {r2, r1, 1, {-.25 * width, .15 * width}},
        {r3, r3, 1, {-.5 * width, 0}},
        {r1, r1, 1, {-.25 * width, -.15 * width}},
        {r1, r1, 1, {-.1 * width, -.35 * height}},
        {r4, r4, 1, {.1 * width, -.65 * height}},
    };
    FlowchartPoint from{};
    out.cloud_path = "M0 0";
    for (const auto& arc : arcs) {
        const FlowchartPoint to{from.x + arc.delta.x, from.y + arc.delta.y};
        out.cloud_path += "a" + number(arc.rx) + "," + number(arc.ry) + " " +
            number(arc.rotation) + " 0,1 " + point(arc.delta);
        cloud_arc_bounds(out.bounds, from, to, arc.rx, arc.ry, arc.rotation * std::numbers::pi / 180);
        from = to;
    }
    out.cloud_path += "H0V0Z";
    out.bounds.left += out.path_offset.x;
    out.bounds.right += out.path_offset.x;
    out.bounds.top += out.path_offset.y;
    out.bounds.bottom += out.path_offset.y;
    out.box = {out.bounds.width(), out.bounds.height()};
    return out;
}

FlowchartOrganicGeometry organic_shape_geometry(std::u16string_view shape, double label_width,
                                                 double label_height, double padding) {
    if (shape == u"cloud") return cloud_geometry(label_width + padding, label_height + padding);
    const auto width = std::max(label_width + 2 * padding, 100.0);
    const auto height = label_height + 2 * padding + (2 - .27) * std::clamp(width * .23, 16.0, 56.0);
    return person_geometry(width, height);
}

FlowchartLocalShape measure_organic_shape(std::u16string_view shape, double width,
    double height, double padding) {
    auto measured = organic_shape_geometry(shape, width, height, padding);
    FlowchartLocalShape result{measured.box, measured.bounds, measured.label_offset};
    if (!measured.cloud_path.empty()) {
        result.contours.push_back({std::move(measured.cloud_path), measured.path_offset});
        return result;
    }

    const auto l = -measured.box.width / 2, r = measured.box.width / 2;
    const auto t = measured.person_body_top, b = t + measured.person_body_height;
    const auto radius = measured.person_body_radius;
    const auto arc = "A" + number(radius) + "," + number(radius) + " 0 0 1 ";
    auto body = "M" + point({l + radius, t}) + "H" + number(r - radius) + arc + point({r, t + radius}) +
        "V" + number(b - radius) + arc + point({r - radius, b}) + "H" + number(l + radius) +
        arc + point({l, b - radius}) + "V" + number(t + radius) + arc + point({l + radius, t}) + "Z";
    result.contours.push_back({std::move(body)});

    const auto head = measured.person_head_radius, cy = measured.person_head_y;
    const auto half = "A" + number(head) + "," + number(head) + " 0 1 0 ";
    result.contours.push_back({"M" + point({-head, cy}) + half + point({head, cy}) +
        half + point({-head, cy}) + "Z"});
    result.clip_outline = person_outline(measured.box.width, measured.box.height);
    return result;
}
