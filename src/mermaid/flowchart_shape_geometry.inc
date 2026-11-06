// Measured, node-local geometry shared by metadata shapes. Layout uses the
// measured box; SVG bounds, labels, and edge attachment use their own geometry.
enum class FlowchartContourPaint { shape, fill, stroke };

struct FlowchartContour {
    std::string path;
    FlowchartPoint translation{};
    FlowchartContourPaint paint = FlowchartContourPaint::shape;
};

struct FlowchartLocalShape {
    FlowchartNodeBox box;
    RoughBounds paint_bounds;
    FlowchartPoint label_offset{};
    std::vector<FlowchartContour> contours;
    // An empty outline means the upstream shape clips against its layout box.
    std::vector<FlowchartPoint> clip_outline;
};

using FlowchartLocalShapes = std::span<const std::optional<FlowchartLocalShape>>;

const FlowchartLocalShape* find_local_shape(FlowchartLocalShapes shapes, std::size_t index) {
    return index < shapes.size() && shapes[index] ? &*shapes[index] : nullptr;
}

std::string paint_local_shape(const FlowchartLocalShape& shape, const FlowchartPaint& paint,
    std::string_view fill, std::string_view stroke, std::string_view width, std::string_view shadow) {
    const bool grouped = shape.contours.size() != 1;
    std::string svg = grouped ? "<g class=\"label-container\"" +
        paint.attributes(fill, stroke, width) + ">" : std::string{};
    for (const auto& contour : shape.contours) {
        svg += "<path";
        if (!grouped) svg += " class=\"label-container\"";
        svg += " d=\"" + contour.path + "\"";
        if (contour.translation.x != 0 || contour.translation.y != 0)
            svg += " transform=\"translate(" + point(contour.translation) + ")\"";
        if (contour.paint == FlowchartContourPaint::shape) {
            svg += paint.attributes(fill, stroke, width);
        } else if (contour.paint == FlowchartContourPaint::fill) {
            svg += " fill=\"" + paint.get(u"fill", fill) + "\" stroke=\"none\"";
        } else {
            svg += " fill=\"none\" stroke=\"" + paint.get(u"stroke", stroke) +
                "\" stroke-width=\"" + paint.get(u"stroke-width", width) +
                "\" stroke-dasharray=\"" + paint.get(u"stroke-dasharray", "none") + "\"";
        }
        svg += shadow;
        svg += "/>";
    }
    if (grouped) svg += "</g>";
    return svg;
}
