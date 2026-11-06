// Measured, node-local geometry shared by metadata shapes. Layout uses the
// measured box; SVG bounds, labels, and edge attachment use their own geometry.
enum class FlowchartContourPaint { shape, fill, stroke, accent_fill, accent_stroke };
enum class FlowchartShadowPaint { contour, group, none };

struct FlowchartContour {
    std::string path;
    FlowchartPoint translation{};
    FlowchartContourPaint paint = FlowchartContourPaint::shape;
    RoughFill sketch_fill = RoughFill::hachure;
    bool measured = true;
    bool invisible_stroke = false;
    std::string dash, opacity;
    std::optional<RoughPolygonResult> flat_drawing;
    bool merge_flat_drawing = false;
};

struct FlowchartCylinderAttachment {
    enum class Axis { horizontal, vertical } axis;
    double rx, ry;
};

struct FlowchartLocalShape {
    FlowchartNodeBox box;
    RoughBounds paint_bounds;
    FlowchartPoint label_offset{};
    std::vector<FlowchartContour> contours;
    // The empty attachment clips against the layout box.
    std::variant<std::monostate, std::vector<FlowchartPoint>, FlowchartCylinderAttachment> attachment;
    FlowchartShadowPaint shadow = FlowchartShadowPaint::contour;
    bool flat_rough_paths = false;
    std::optional<FlowchartPoint> console_glyph;
};

void include_outline_bounds(std::optional<RoughBounds>& bounds, RoughBounds box,
                            FlowchartPoint translation = {}) {
    box.left += translation.x; box.right += translation.x;
    box.top += translation.y; box.bottom += translation.y;
    if (!bounds) { bounds = box; return; }
    bounds->left = std::min(bounds->left, box.left);
    bounds->top = std::min(bounds->top, box.top);
    bounds->right = std::max(bounds->right, box.right);
    bounds->bottom = std::max(bounds->bottom, box.bottom);
}

using FlowchartLocalShapes = std::span<const std::optional<FlowchartLocalShape>>;

const FlowchartLocalShape* find_local_shape(FlowchartLocalShapes shapes, std::size_t index) {
    return index < shapes.size() && shapes[index] ? &*shapes[index] : nullptr;
}

bool flowchart_accent(const FlowchartContour& contour) {
    return contour.paint==FlowchartContourPaint::accent_fill||
           contour.paint==FlowchartContourPaint::accent_stroke;
}

std::string paint_flowchart_accent(const FlowchartContour& contour,std::string_view accent) {
    std::string svg="<path d=\""+contour.path+"\"";
    if(contour.paint==FlowchartContourPaint::accent_fill)
        svg+=" fill=\""+std::string(accent)+"\" stroke=\"none\"";
    else svg+=" fill=\"none\" stroke=\""+std::string(accent)+"\" stroke-width=\"1\"";
    if(!contour.opacity.empty()) svg+=" opacity=\""+contour.opacity+"\"";
    return svg+"/>";
}

std::string paint_storage_decorations(const FlowchartLocalShape& shape,std::string_view accent,
    bool accents_already_painted) {
    std::string svg;
    if(!accents_already_painted) for(const auto& contour:shape.contours)
        if(flowchart_accent(contour)) svg+=paint_flowchart_accent(contour,accent);
    if(shape.console_glyph) {
        const auto at=*shape.console_glyph;
        svg+="<text class=\"console-glyph\" x=\""+number(at.x)+"\" y=\""+number(at.y)+
            "\" font-family=\"monospace\" font-weight=\"700\" font-size=\"14\" fill=\""+
            std::string(accent)+"\">&gt;_</text>";
    }
    return svg;
}

std::string paint_local_shape(const FlowchartLocalShape& shape, const FlowchartPaint& paint,
    std::string_view fill, std::string_view stroke, std::string_view width, std::string_view shadow,
    std::uint32_t rough_seed,std::string_view accent={}) {
    const bool grouped = shape.contours.size() != 1 || shape.flat_rough_paths || shape.console_glyph;
    std::string svg = grouped ? "<g class=\"label-container\"" +
        paint.attributes(fill, stroke, width) +
        (shape.shadow == FlowchartShadowPaint::group ? std::string(shadow) : std::string{}) +
        ">" : std::string{};
    for (const auto& contour : shape.contours) {
        if(flowchart_accent(contour)) {
            svg+=paint_flowchart_accent(contour,accent);
            continue;
        }
        const auto append = [&](std::string_view path, std::string_view dash = "none",
            bool omit_fill = false, bool omit_stroke = false) {
            svg += "<path";
            if (!grouped) svg += " class=\"label-container\"";
            svg += " d=\"";
            svg += path;
            svg += '"';
            if (contour.translation.x != 0 || contour.translation.y != 0)
                svg += " transform=\"translate(" + point(contour.translation) + ")\"";
            if (contour.invisible_stroke) svg += " stroke-opacity=\"0\"";
            if (contour.paint == FlowchartContourPaint::shape) {
                svg += paint.attributes(omit_fill?"none":fill,omit_stroke?"none":stroke,
                    width,contour.dash.empty()?dash:contour.dash);
            } else if (contour.paint == FlowchartContourPaint::fill) {
                svg += " fill=\"" + paint.get(u"fill", fill) + "\" stroke=\"none\"";
            } else {
                svg += " fill=\"none\" stroke=\"" + paint.get(u"stroke", stroke) +
                    "\" stroke-width=\"" + paint.get(u"stroke-width", width) +
                    "\" stroke-dasharray=\"" + paint.get(u"stroke-dasharray", dash) + "\"";
            }
            if (shape.shadow == FlowchartShadowPaint::contour ||
                (!grouped && shape.shadow == FlowchartShadowPaint::group))
                svg += shadow;
            svg += "/>";
        };
        if (contour.flat_drawing) {
            const auto& paths=*contour.flat_drawing;
            if(contour.merge_flat_drawing) {
                if(!paths.fill_path.empty()||!paths.stroke_path.empty())
                    append(paths.fill_path+" "+paths.stroke_path,"none",
                        paths.fill_path.empty(),paths.stroke_path.empty());
            } else {
                if(!paths.fill_path.empty()) append(paths.fill_path,"none",false,true);
                if(!paths.stroke_path.empty()) append(paths.stroke_path,"0 0",true);
            }
        } else if (shape.flat_rough_paths) {
            // Mermaid applies node CSS to both RoughJS paths. The outline has
            // separate subpaths, so dash phase resets at every segment.
            const auto paths = sketch_svg_path(contour.path, {.seed = rough_seed, .roughness = 0,
                .fill = paint.get(u"fill", fill) == "none" ? RoughFill::none : RoughFill::solid,
                .stroke = paint.get(u"stroke", stroke) != "none"});
            if (!paths.fill_path.empty()) append(paths.fill_path);
            if (!paths.stroke_path.empty()) append(paths.stroke_path, "0 0");
        } else {
            append(contour.path);
        }
    }
    if(shape.console_glyph) svg+=paint_storage_decorations(shape,accent,true);
    if (grouped) svg += "</g>";
    return svg;
}
