// Mermaid 12.2.0 wave, curved-cap, cylinder and comment shape recipes.
// Inputs are the measured browser-equivalent label box and node padding.
bool document_flowchart_shape(std::u16string_view shape) {
    return shape == u"doc" || shape == u"docs" || shape == u"tag-doc" || shape == u"flag" ||
           shape == u"delay" || shape == u"curv-trap" || shape == u"h-cyl" || shape == u"lin-cyl" ||
           shape == u"brace" || shape == u"brace-r" || shape == u"braces";
}

RoughBounds outline_point_bounds(std::span<const FlowchartPoint> points) {
    RoughBounds b{points.front().x, points.front().y, points.front().x, points.front().y};
    for (const auto p : points) {
        b.left = std::min(b.left, p.x); b.right = std::max(b.right, p.x);
        b.top = std::min(b.top, p.y); b.bottom = std::max(b.bottom, p.y);
    }
    return b;
}
void append_outline_points(FlowchartLocalShape& result, std::span<const FlowchartPoint> points,
    FlowchartPoint translation = {}, bool invisible_stroke = false,
    bool closed = true, RoughFill sketch_fill = RoughFill::hachure) {
    auto path = polygon_path(points);
    if (!closed) path.pop_back();
    std::optional<RoughBounds> bounds;
    if (!result.contours.empty()) bounds = result.paint_bounds;
    include_outline_bounds(bounds, outline_point_bounds(points), translation);
    result.paint_bounds = *bounds;
    result.contours.push_back({std::move(path), translation, FlowchartContourPaint::shape,
                               sketch_fill, true, invisible_stroke});
}

std::vector<FlowchartPoint> document_wave(double left, double right, double final_h, double amplitude) {
    std::vector<FlowchartPoint> points{{left, final_h / 2}};
    append_flowchart_sine_wave(points, {left, final_h / 2}, {right, final_h / 2}, amplitude, 0.8);
    points.push_back({right, -final_h / 2});
    points.push_back({left, -final_h / 2});
    return points;
}

void wave_shape(FlowchartLocalShape& result, std::u16string_view shape, double lw, double lh,
                double padding, bool neo) {
    const double px = neo ? 16 : padding, py = neo ? 12 : padding;
    if (shape == u"doc") {
        const double w = lw + 2 * px, h = lh + 2 * py;
        const double a = neo ? h / 4 : h / 8;
        const double half_w = std::max(14.0, w) / 2;
        std::get<std::vector<FlowchartPoint>>(result.attachment) = document_wave(-half_w, half_w, h + a, a);
        append_outline_points(result, std::get<std::vector<FlowchartPoint>>(result.attachment), {0, -a / 2});
        result.label_offset = {padding - px, padding - py - a};
    } else if (shape == u"tag-doc") {
        const double w = lw + 2 * padding, h = lh + 2 * padding, a = h / 8;
        const double final_h = h + a, tag_w = w * 0.2, tag_h = h * 0.2;
        std::get<std::vector<FlowchartPoint>>(result.attachment) = document_wave(-w * 0.55, w * 0.55, final_h, a);
        append_outline_points(result, std::get<std::vector<FlowchartPoint>>(result.attachment), {0, -a / 2});
        const double x = -w / 2 + w * 0.05, y = -final_h / 2 - tag_h * 0.4;
        std::vector<FlowchartPoint> tag{{x + w - tag_w, (y + h) * 1.3},
            {x + w, y + h - tag_h}, {x + w, (y + h) * 0.9}};
        append_flowchart_sine_wave(tag, {x + w, (y + h) * 1.25},
            {x + w - tag_w, (y + h) * 1.3}, -h * 0.02, 0.5);
        append_outline_points(result, tag, {0, -a / 2}, false, true, RoughFill::solid);
        result.label_offset = {0, -a / 2};
    } else if (shape == u"docs") {
        constexpr double offset = 10;
        const double w = lw + 2 * px, h = lh + 3 * py;
        const double a = neo ? h / 4 : h / 8, final_h = h + a / 2;
        const double x = -w / 2, y = -final_h / 2;
        auto& points = std::get<std::vector<FlowchartPoint>>(result.attachment);
        points = {{x - offset, y + offset}, {x - offset, y + final_h + offset}};
        append_flowchart_sine_wave(points, {x - offset, y + final_h + offset},
            {x + w - offset, y + final_h + offset}, a, 0.8);
        const double last_y = points.back().y;
        const std::array steps{FlowchartPoint{x + w - offset, last_y - offset},
            FlowchartPoint{x + w, last_y - offset}, FlowchartPoint{x + w, last_y - 2 * offset},
            FlowchartPoint{x + w + offset, last_y - 2 * offset}, FlowchartPoint{x + w + offset, y - offset},
            FlowchartPoint{x + offset, y - offset}, FlowchartPoint{x + offset, y},
            FlowchartPoint{x, y}, FlowchartPoint{x, y + offset}};
        points.insert(points.end(), steps.begin(), steps.end());
        append_outline_points(result, points, {0, -a / 2});
        const std::array inner{FlowchartPoint{x, y + offset}, FlowchartPoint{x + w - offset, y + offset},
            FlowchartPoint{x + w - offset, last_y - offset}, FlowchartPoint{x + w, last_y - offset},
            FlowchartPoint{x + w, y}, FlowchartPoint{x, y}};
        append_outline_points(result, inner, {0, -a / 2});
        result.label_offset = {-offset, offset - a};
    } else {
        const double w = lw + 2 * px, h = lh + (neo ? 20 : padding);
        const double a = h / 8, half_h = (h + a * 2) / 2;
        auto& points = std::get<std::vector<FlowchartPoint>>(result.attachment);
        points.push_back({-w / 2, half_h});
        append_flowchart_sine_wave(points, {-w / 2, half_h}, {w / 2, half_h}, a, 1);
        points.push_back({w / 2, -half_h});
        append_flowchart_sine_wave(points, {w / 2, -half_h}, {-w / 2, -half_h}, a, -1);
        append_outline_points(result, points);
    }
}

void curved_cap_shape(FlowchartLocalShape& result, std::u16string_view shape, double lw, double lh,
                      double padding, bool neo) {
    const double px = neo ? 16 : padding, py = neo ? 12 : padding;
    const bool delay = shape == u"delay";
    const double h = delay ? std::max(10.0, lh) + 2 * py : std::max(5.0, lh + 2 * py);
    const double r = h / 2;
    const double clearance = r - std::sqrt(std::max(0.0, r * r - lh * lh / 4));
    auto& points = std::get<std::vector<FlowchartPoint>>(result.attachment);
    if (delay) {
        const double w = std::max(15.0, lw) + clearance * 2 + px * 2;
        points = {{-w / 2, -h / 2}, {w / 2 - r, -h / 2}};
        append_flowchart_arc_samples(points, {-w / 2 + r, 0}, r, 50, 90, 270, true);
        points.push_back({w / 2 - r, h / 2});
        points.push_back({-w / 2, h / 2});
        append_outline_points(result, points);
    } else {
        const double w = std::max({20.0, lw + px * 2 + std::max(h / 4, clearance) * 2,
                                  (lw + px * 2) * 1.25});
        const double rw = w - r, tw = h / 4;
        points = {{rw, 0}, {tw, 0}, {0, h / 2}, {tw, h}, {rw, h}};
        append_flowchart_arc_samples(points, {-rw, -h / 2}, r, 50, 270, 90, true);
        append_outline_points(result, points, {-w / 2, -h / 2});
    }
}

void cylinder_outline_shape(FlowchartLocalShape& result, std::u16string_view shape, double lw, double lh,
                            double padding, bool neo, bool hand_drawn) {
    if (shape == u"h-cyl") {
        const double p = neo ? 12 : padding / 2, h = lh + p;
        const double ry = h / 2, rx = ry / (2.5 + h / 50);
        const double w = lw + rx + p;
        const auto arc = [&](bool sweep, double x, double y) {
            return "A" + point({rx, ry}) + (sweep ? " 0,0,1 " : " 0,0,0 ") + point({x,y});
        };
        const FlowchartPoint translation{-w / 2, h / 2};
        if (!hand_drawn) {
            result.contours.push_back({"M0,0" + arc(true, 0, -h) + "L" + point({w, -h}) +
                arc(true, w, 0) + "M" + point({w, -h}) + arc(false, w, 0) + "L0,0", translation});
        } else {
            result.contours.push_back({"M0,0M" + point({w, 0}) + arc(false, w, -h) +
                "L" + point({0, -h}) + arc(false, 0, 0) + "L" + point({w, 0}), translation});
            result.contours.push_back({"M" + point({w / 2, -h / 2}) + arc(false, w / 2, h / 2),
                {}, FlowchartContourPaint::stroke, RoughFill::none, false});
        }
        result.paint_bounds = {-rx + translation.x, -h + translation.y, w + rx + translation.x, translation.y};
        result.box = {w + 2 * rx, h};
        result.label_offset = {-rx, 0};
        result.attachment = FlowchartCylinderAttachment{FlowchartCylinderAttachment::Axis::horizontal, rx, ry};
    } else {
        const double px = neo ? 16 : padding, py = neo ? 24 : padding;
        const double w = lw + 2 * px, rx = w / 2, ry = rx / (2.5 + w / 50);
        const double h = lh + ry + 2 * py, outer_offset = h * 0.1;
        const auto arc = [&](double x, double y) {return "A" + point({rx, ry}) + " 0,0,0 " + point({x,y});};
        const FlowchartPoint translation{-w / 2, -h / 2 - ry};
        std::string body = "M" + point({0, ry});
        body += hand_drawn ? "M" + point({w, ry}) : arc(w, ry);
        body += arc(0, ry) + "L" + point({0, ry + h}) + arc(w, ry + h) +
            "L" + point({w, ry}) + "M" + point({0, ry + outer_offset}) + arc(w, ry + outer_offset);
        result.contours.push_back({std::move(body), translation});
        if (hand_drawn)
            result.contours.push_back({"M" + point({-w / 2, -h / 2}) + arc(w / 2, -h / 2),
                {}, FlowchartContourPaint::shape, RoughFill::hachure, false});
        result.paint_bounds = {translation.x, translation.y, w + translation.x, h + 2 * ry + translation.y};
        result.box = {w, h + 2 * ry};
        result.label_offset = {0, ry};
        result.attachment = FlowchartCylinderAttachment{FlowchartCylinderAttachment::Axis::vertical, rx, ry};
    }
}

std::vector<FlowchartPoint> brace_curve(double w, double h, double r, bool right, bool paired) {
    std::vector<FlowchartPoint> points;
    const auto arc = [&](double x, double y, int count, double start, double end) {
        append_flowchart_arc_samples(points, {x, y}, r, count, start, end, !right || paired);
    };
    if (right && paired) {
        arc(-w / 2 + r * 1.5, -h / 2, 20, -90, -180);
        points.push_back({w / 2 - r / 2, r});
        arc(-w / 2 - r / 2, -r, 20, 0, 90);
        arc(-w / 2 - r / 2, r, 20, -90, 0);
        points.push_back({w / 2 - r / 2, -r});
        arc(-w / 2 + r * 1.5, h / 2, 30, -180, -270);
    } else {
        arc(w / 2, -h / 2, right ? 20 : 30, -90, 0);
        points.push_back(right ? FlowchartPoint{w / 2 + r, -r} : FlowchartPoint{-w / 2 - r, r});
        arc(w / 2 + r * 2, -r, 20, -180, -270);
        arc(w / 2 + r * 2, r, 20, -90, -180);
        points.push_back(right ? FlowchartPoint{w / 2 + r, h / 2} : FlowchartPoint{-w / 2 - r, -h / 2});
        arc(w / 2, h / 2, 20, 0, 90);
    }
    return points;
}

void brace_outline_shape(FlowchartLocalShape& result, std::u16string_view shape, double lw, double lh,
                         double padding, bool neo) {
    const bool left = shape == u"brace", right = shape == u"brace-r", paired = !left && !right;
    const double px = neo ? 18 : padding, py = neo ? 12 : padding;
    const double scale = neo && !left ? 2 : 1;
    const double w = lw + px * scale, h = lh + py * scale, r = std::max(5.0, h * 0.1);
    const FlowchartPoint translation{left ? r : right ? -r : r - r / 4, 0};
    if (right || paired)
        append_outline_points(result, brace_curve(w, h, r, true, paired), translation,
            false, false, RoughFill::none);
    if (!right)
        append_outline_points(result, brace_curve(w, h, r, false, paired), translation,
            false, false, RoughFill::none);
    auto& points = std::get<std::vector<FlowchartPoint>>(result.attachment);
    const double half_w = w / 2, half_h = h / 2;
    points = right ? std::vector<FlowchartPoint>{{-half_w, -half_h - r}, {half_w, -half_h - r}}
                   : std::vector<FlowchartPoint>{{half_w, -half_h - r}, {-half_w, -half_h - r}};
    const auto arc = [&](double x, double y, double start, double end) {
        append_flowchart_arc_samples(points, {x, y}, r, 20, start, end, !right);
    };
    arc(half_w, -half_h, -90, 0);
    points.push_back({right ? half_w + r : -half_w - r, -r});
    const double shoulder_x = half_w + (left ? w * 0.1 : r * 2);
    arc(shoulder_x, -r, -180, -270);
    arc(shoulder_x, r, -90, -180);
    points.push_back({right ? half_w + r : -half_w - r, half_h});
    arc(half_w, half_h, 0, 90);
    points.push_back({right ? half_w : -half_w, half_h + r});
    points.push_back({right ? -half_w : paired ? half_w - r * 1.5 : half_w, half_h + r});
    if (paired) {
        const auto extra = brace_curve(w, h, r, true, true);
        points.insert(points.end(), extra.begin(), extra.end());
    }
    append_outline_points(result, points, translation, true, true, RoughFill::none);
    result.label_offset = {left ? r - px / 2 : (padding - px * scale) / 2,
                           (padding - py * scale) / 2};
}

FlowchartLocalShape measure_document_flowchart_shape(std::u16string_view shape,
    double label_width, double label_height, double padding, bool neo, bool hand_drawn) {
    FlowchartLocalShape result{};
    result.attachment = std::vector<FlowchartPoint>{};
    if (shape == u"doc" || shape == u"docs" || shape == u"tag-doc" || shape == u"flag")
        wave_shape(result, shape, label_width, label_height, padding, neo);
    else if (shape == u"delay" || shape == u"curv-trap")
        curved_cap_shape(result, shape, label_width, label_height, padding, neo);
    else if (shape == u"h-cyl" || shape == u"lin-cyl")
        cylinder_outline_shape(result, shape, label_width, label_height, padding, neo, hand_drawn);
    else
        brace_outline_shape(result, shape, label_width, label_height, padding, neo);
    if (std::holds_alternative<std::vector<FlowchartPoint>>(result.attachment))
        result.box = {result.paint_bounds.width(), result.paint_bounds.height()};
    if (shape == u"flag" || shape == u"brace" || shape == u"brace-r" || shape == u"braces")
        result.shadow = FlowchartShadowPaint::none;
    else if (result.contours.size() > 1)
        result.shadow = FlowchartShadowPaint::group;
    return result;
}

FlowchartPoint apply_cylinder_attachment(const FlowchartCylinderAttachment& shape,
    const FlowchartPlacedNode& node, FlowchartPoint target, FlowchartPoint p) {
    const double dx = target.x - node.x, dy = target.y - node.y;
    const double x = p.x - node.x, y = p.y - node.y;
    const auto rx = shape.rx, ry = shape.ry;
    if (shape.axis == FlowchartCylinderAttachment::Axis::horizontal) {
        if (ry != 0 && (std::abs(y) < node.height / 2 ||
            (std::abs(y) == node.height / 2 && std::abs(x) > node.width / 2 - rx))) {
            const double x2 = rx * rx * (1 - y * y / (ry * ry));
            const double delta = rx - std::sqrt(std::abs(x2));
            p.x += dx > 0 ? -delta : delta;
        }
    } else if (rx != 0 && (std::abs(x) < node.width / 2 ||
        (std::abs(x) == node.width / 2 && std::abs(y) > node.height / 2 - ry))) {
        double delta = ry * ry * (1 - x * x / (rx * rx));
        if (delta > 0) delta = std::sqrt(delta);
        delta = ry - delta;
        p.y += dy > 0 ? -delta : delta;
    }
    return p;
}
