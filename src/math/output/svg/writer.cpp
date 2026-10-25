// Output behavior references MathJax 4.1.3 at fb987178c2d279a99b0db5ea02e751691703955e:
// ts/output/svg.ts, ts/output/svg/Wrapper.ts, and ts/output/svg/FontCache.ts.
// Copyright (c) 2018-2026 The MathJax Consortium; Apache-2.0. See README.md.
module folia.math.output.svg.writer;
import folia.math.output.svg.serializer;
import std;
import folia.math.util.lengths;
import folia.math.util.styles;
import folia.math.util.utf16;

namespace folia::math::output::svg {
namespace scene = folia::math::output::scene;
namespace detail {
std::string escape_text(std::string_view);
void append_attribute(std::string &, std::string_view, std::string_view);
void append_metadata(std::string &, scene::SemanticMetadata const &,
                     std::span<std::string_view const>);
} // namespace detail
namespace {

std::string number(double value, int precision = 1) {
    if (!std::isfinite(value))
        throw std::invalid_argument("non-finite SVG coordinate");
    std::string result =
        folia::math::lengths::format_decimal(value, static_cast<unsigned>(precision));
    while (result.find('.') != std::string::npos && result.back() == '0')
        result.pop_back();
    if (!result.empty() && result.back() == '.')
        result.pop_back();
    if (result == "-0")
        result = "0";
    return result;
}

std::string foreign_fixed(double value) {
    if (std::abs(value) < 0.0006)
        return "0";
    return number(value, 3);
}

std::string ex_value(double em, double x_height) {
    if (!(x_height > 0) || !std::isfinite(x_height))
        throw std::invalid_argument("Scene font x-height must be finite and positive");
    const auto amount = em / x_height;
    if (std::abs(amount) < 0.001)
        return "0";
    auto result = folia::math::lengths::format_decimal(amount, 3);
    while (result.find('.') != std::string::npos && result.back() == '0')
        result.pop_back();
    if (!result.empty() && result.back() == '.')
        result.pop_back();
    return result + "ex";
}

std::string color(scene::Color const &color) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string value = "#000000";
    const std::array<std::uint8_t, 3> channels{color.red, color.green, color.blue};
    for (std::size_t i = 0; i < channels.size(); ++i) {
        value[1 + i * 2] = digits[channels[i] >> 4];
        value[2 + i * 2] = digits[channels[i] & 0x0f];
    }
    return value;
}

void append_color(std::string &out, std::string_view name, std::string_view opacity_name,
                  scene::PaintColor const &paint) {
    std::visit(
        [&](auto const &value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, scene::InheritedPaint>) {
                return;
            } else if constexpr (std::is_same_v<T, scene::NoPaint>) {
                detail::append_attribute(out, name, "none");
            } else if constexpr (std::is_same_v<T, scene::CurrentColorPaint>) {
                detail::append_attribute(out, name, "currentColor");
            } else if constexpr (std::is_same_v<T, scene::CssColor>) {
                detail::append_attribute(out, name, value.value);
            } else {
                detail::append_attribute(out, name, color(value));
                if (value.alpha != 255)
                    detail::append_attribute(out, opacity_name, number(value.alpha / 255.0, 3));
            }
        },
        paint);
}

std::string_view stroke_cap_name(scene::StrokeCap cap) {
    switch (cap) {
    case scene::StrokeCap::butt:
        return "butt";
    case scene::StrokeCap::round:
        return "round";
    case scene::StrokeCap::square:
        return "square";
    }
    throw std::invalid_argument("unknown Scene stroke cap");
}

std::string_view stroke_join_name(scene::StrokeJoin join) {
    switch (join) {
    case scene::StrokeJoin::miter:
        return "miter";
    case scene::StrokeJoin::round:
        return "round";
    case scene::StrokeJoin::bevel:
        return "bevel";
    }
    throw std::invalid_argument("unknown Scene stroke join");
}

void append_paint(std::string &out, scene::Paint const &paint, bool stroke_first = false) {
    if (stroke_first)
        append_color(out, "stroke", "stroke-opacity", paint.stroke);
    append_color(out, "fill", "fill-opacity", paint.fill);
    if (paint.fill_rule == scene::FillRule::evenodd)
        detail::append_attribute(out, "fill-rule", "evenodd");
    if (!stroke_first)
        append_color(out, "stroke", "stroke-opacity", paint.stroke);
    if (paint.stroke_width != 0)
        detail::append_attribute(out, "stroke-width", number(paint.stroke_width * 1000));
    if (paint.opacity != 1)
        detail::append_attribute(out, "opacity", number(paint.opacity, 3));
    if (paint.line_cap)
        detail::append_attribute(out, "stroke-linecap", stroke_cap_name(*paint.line_cap));
    if (paint.line_join)
        detail::append_attribute(out, "stroke-linejoin", stroke_join_name(*paint.line_join));
    if (paint.dash_array) {
        if (paint.dash_array->empty()) {
            detail::append_attribute(out, "stroke-dasharray", "none");
        } else {
            std::string values;
            for (auto const length : *paint.dash_array) {
                if (!values.empty())
                    values.push_back(' ');
                values += number(length * 1000);
            }
            detail::append_attribute(out, "stroke-dasharray", values);
        }
    }
}

void append_path(std::string &out, scene::PathGeometry const &path,
                 scene::PathSyntax syntax = scene::PathSyntax::compact) {
    const bool spaced = syntax == scene::PathSyntax::spaced;
    auto command = [&](char value) {
        if (spaced && !out.empty() && out.back() != ' ')
            out.push_back(' ');
        out.push_back(value);
        if (spaced)
            out.push_back(' ');
    };
    auto point = [&](scene::Point const &value) {
        out += number(value.x * 1000);
        out += spaced ? " " : ",";
        out += number(value.y * 1000);
    };
    for (auto const &segment : path.segments) {
        std::visit(
            [&](auto const &item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, scene::MoveTo>) {
                    command('M');
                    point(item.to);
                } else if constexpr (std::is_same_v<T, scene::LineTo>) {
                    command(item.mode == scene::CoordinateMode::relative ? 'l' : 'L');
                    point(item.to);
                } else if constexpr (std::is_same_v<T, scene::QuadTo>) {
                    command('Q');
                    point(item.control);
                    out.push_back(' ');
                    point(item.to);
                } else if constexpr (std::is_same_v<T, scene::CubicTo>) {
                    command('C');
                    point(item.control1);
                    out.push_back(' ');
                    point(item.control2);
                    out.push_back(' ');
                    point(item.to);
                } else if constexpr (std::is_same_v<T, scene::ArcTo>) {
                    command(item.mode == scene::CoordinateMode::relative ? 'a' : 'A');
                    out += number(item.radius_x * 1000) + " " + number(item.radius_y * 1000) + " ";
                    // Arc values remain in Scene's y-up local coordinate system.
                    // The enclosing SVG transform maps the finished path; changing
                    // arc flags here would apply the reflection twice.
                    out += number(item.rotation, 3) + " " + (item.large_arc ? "1" : "0");
                    out += item.flags_separator == scene::ArcFlagSeparator::comma ? "," : " ";
                    out += item.sweep ? "1 " : "0 ";
                    point(item.to);
                } else {
                    if (spaced && !out.empty() && out.back() != ' ')
                        out.push_back(' ');
                    out += 'Z';
                }
            },
            segment);
    }
}

std::string_view translation_separator(
    scene::ElementRole role,
    scene::TranslationSeparator spelling = scene::TranslationSeparator::role_default) {
    switch (spelling) {
    case scene::TranslationSeparator::role_default:
        break;
    case scene::TranslationSeparator::comma:
        return ",";
    case scene::TranslationSeparator::space:
        return " ";
    case scene::TranslationSeparator::comma_space:
        return ", ";
    default:
        throw std::invalid_argument("unknown Scene translation separator");
    }
    switch (role) {
    case scene::ElementRole::math_wrapper:
    case scene::ElementRole::id_alignment_box:
    case scene::ElementRole::unknown_text:
        return ",";
    case scene::ElementRole::enclosure_content:
        return ", ";
    case scene::ElementRole::operator_adjustment:
    case scene::ElementRole::inferred_row_adjustment:
    case scene::ElementRole::enclosure_mark:
    case scene::ElementRole::table_content_viewport:
    case scene::ElementRole::table_label_frame:
    case scene::ElementRole::image_glyph:
    case scene::ElementRole::foreign_content_viewport:
    case scene::ElementRole::vertical_extender_viewport:
    case scene::ElementRole::horizontal_extender_viewport:
    case scene::ElementRole::table_frame:
    case scene::ElementRole::table_vertical_line:
    case scene::ElementRole::table_horizontal_line:
    case scene::ElementRole::anonymous_group:
    case scene::ElementRole::glyph_composition:
    case scene::ElementRole::background_box:
    case scene::ElementRole::link_hitbox:
    case scene::ElementRole::id_alignment_marker:
    case scene::ElementRole::fraction_rule:
    case scene::ElementRole::radical_rule:
    case scene::ElementRole::error_background:
        return " ";
    }
    throw std::invalid_argument("unknown Scene element role");
}

std::string transform_operation(scene::TransformOperation const &operation,
                                scene::ElementRole role) {
    return std::visit(
        [&](auto const &value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, scene::Translation>) {
                std::string result = "translate(";
                result += number(value.x * 1000);
                result += translation_separator(role, value.separator);
                result += number(value.y * 1000);
                result += ')';
                return result;
            } else if constexpr (std::is_same_v<T, scene::Scaling>) {
                if (value.x == value.y && !value.explicit_axes)
                    return "scale(" + number(value.x, static_cast<int>(value.precision)) + ")";
                return "scale(" + number(value.x, static_cast<int>(value.precision)) + "," +
                       number(value.y, static_cast<int>(value.precision)) + ")";
            } else if constexpr (std::is_same_v<T, scene::Rotation>) {
                return "rotate(" + number(value.degrees, 3) + " " + number(value.center.x * 1000) +
                       " " + number(value.center.y * 1000) + ")";
            } else {
                return "matrix(" + number(value.a, 6) + " " + number(value.b, 6) + " " +
                       number(value.c, 6) + " " + number(value.d, 6) + " " +
                       number(value.e * 1000) + " " + number(value.f * 1000) + ")";
            }
        },
        operation);
}

std::string transform_value(scene::Transform const &transform, scene::ElementRole role) {
    std::string serialized;
    for (auto const &operation : transform.operations()) {
        if (!serialized.empty())
            serialized.push_back(' ');
        serialized += transform_operation(operation, role);
    }
    return serialized;
}

void append_transform(std::string &out, scene::Transform const &transform,
                      scene::ElementRole role) {
    auto serialized = transform_value(transform, role);
    if (!serialized.empty())
        detail::append_attribute(out, "transform", serialized);
}

std::string codepoint(std::uint32_t value) {
    std::array<char, 16> buffer{};
    auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 16);
    if (ec != std::errc{})
        throw std::runtime_error("glyph id formatting failed");
    std::string result(buffer.data(), end);
    std::ranges::transform(result, result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

std::string_view path_data(scene::Glyph const &glyph, SvgWriteOptions const &options) {
    if (!glyph.resource)
        throw UnsupportedError("Scene glyph has no immutable common outline resource");
    if (!options.font_resources)
        throw UnsupportedError(
            "SVG glyph output requires the immutable Source font resource catalog");
    const auto *source = options.font_resources->find({glyph.key.variant, glyph.key.codepoint});
    if (!source)
        throw UnsupportedError("SVG glyph has no exact Source font resource key");
    if (!source->path_defined)
        throw UnsupportedError(
            "SVG glyph has no Source path; composition must be completed in Scene");
    // The catalog/request owns this immutable value throughout write(). The
    // caller immediately appends it, without another font lookup in between.
    return source->path_data;
}

bool has_glyphs(scene::Group const &group) {
    for (auto const &entry : group.entries) {
        if (auto const *primitive = std::get_if<scene::Primitive>(&entry)) {
            if (auto const *run = std::get_if<scene::GlyphRun>(primitive);
                run && !run->glyphs.empty())
                return true;
        } else if (auto const *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child && has_glyphs(**child))
                return true;
        } else {
            auto const &region = std::get<scene::ActionRegion>(entry);
            if (region.tooltip && has_glyphs(region.tooltip->root()))
                return true;
        }
    }
    return false;
}

std::string cache_id(scene::Glyph const &glyph, FontCachePlan const *plan, CacheScopeId scope) {
    if (!plan)
        throw std::logic_error("local glyph reference has no FontCachePlan");
    return plan->scope(scope).references.at(&glyph);
}

void validate_metadata(scene::SemanticMetadata const &metadata) {
    if (!metadata.annotation.empty() || !metadata.annotation_encoding.empty())
        throw UnsupportedError("SVG metadata annotations are not yet serializable");
}

void validate_scene(scene::Group const &group) {
    validate_metadata(group.metadata);
    for (auto const &entry : group.entries) {
        std::visit(
            [&](auto const &value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, std::shared_ptr<scene::Group const>>) {
                    if (value)
                        validate_scene(*value);
                } else if constexpr (std::is_same_v<T, scene::ActionRegion>) {
                    if (value.tooltip)
                        validate_scene(value.tooltip->root());
                }
            },
            entry);
    }
}

void append_glyphs(std::string &out, scene::GlyphRun const &run, SvgWriteOptions const &options,
                   FontCachePlan const *plan, CacheScopeId scope) {
    for (auto const &glyph : run.glyphs) {
        out += options.font_cache == FontCacheMode::none ? "<path" : "<use";
        detail::append_attribute(out, "data-c", codepoint(glyph.key.codepoint));
        if (options.font_cache == FontCacheMode::none)
            detail::append_attribute(out, "d", path_data(glyph, options));
        else
            detail::append_attribute(out, options.use_xlink ? "xlink:href" : "href",
                                     "#" + cache_id(glyph, plan, scope));
        std::string transform;
        if (glyph.origin.x || glyph.origin.y)
            transform = "translate(" + number(glyph.origin.x * 1000) + "," +
                        number(glyph.origin.y * 1000) + ")";
        for (auto const &operation : glyph.transform.operations()) {
            if (!transform.empty())
                transform.push_back(' ');
            transform += transform_operation(operation, scene::ElementRole::math_wrapper);
        }
        if (!transform.empty())
            detail::append_attribute(out, "transform", transform);
        append_paint(out, glyph.paint);
        out += "/>";
    }
}

void append_table_markers(std::string &out, scene::SemanticMetadata const &metadata) {
    for (auto const &attribute : metadata.ordered_attributes) {
        if (attribute.name == "data-frame" || attribute.name == "data-line" ||
            attribute.name == "class")
            detail::append_attribute(out, attribute.name, attribute.value);
    }
}
void append_table_stroke(std::string &out, scene::Paint const &paint) {
    if (paint.stroke_width != 0.07) {
        detail::append_attribute(out, "stroke-thickness", number(paint.stroke_width * 1000, 1));
        if (paint.dash_array) {
            std::string values;
            for (auto const length : *paint.dash_array) {
                if (!values.empty())
                    values.push_back(',');
                values += number(length * 1000, 1);
            }
            if (paint.dash_array->empty())
                values = "none";
            detail::append_attribute(out, "stroke-dasharray", values);
        }
    }
}

void append_rule(std::string &out, scene::Rule const &rule) {
    out += "<rect";
    if (rule.role == scene::ElementRole::error_background) {
        detail::append_attribute(out, "data-background", "true");
        detail::append_attribute(out, "width", number(rule.bounds.width * 1000));
        detail::append_attribute(out, "height", number(rule.bounds.height * 1000));
        detail::append_attribute(out, "y", number(rule.bounds.y * 1000));
        append_paint(out, rule.paint);
        if (rule.corner_radius)
            detail::append_attribute(out, "rx", number(*rule.corner_radius * 1000));
        static constexpr std::array<std::string_view, 16> reserved{"data-background",
                                                                   "width",
                                                                   "height",
                                                                   "x",
                                                                   "y",
                                                                   "rx",
                                                                   "fill",
                                                                   "stroke",
                                                                   "fill-rule",
                                                                   "fill-opacity",
                                                                   "stroke-opacity",
                                                                   "stroke-width",
                                                                   "opacity",
                                                                   "stroke-linecap",
                                                                   "stroke-linejoin",
                                                                   "stroke-dasharray"};
        detail::append_metadata(out, rule.metadata, reserved);
        out += "/>";
        return;
    }
    if (rule.role == scene::ElementRole::enclosure_mark) {
        detail::append_attribute(out, "x", number(rule.bounds.x * 1000));
        detail::append_attribute(out, "y", number(rule.bounds.y * 1000));
        detail::append_attribute(out, "width", number(rule.bounds.width * 1000));
        detail::append_attribute(out, "height", number(rule.bounds.height * 1000));
        append_paint(out, rule.paint);
        if (rule.corner_radius)
            detail::append_attribute(out, "rx", number(*rule.corner_radius * 1000));
        static constexpr std::array<std::string_view, 17> reserved{"x",
                                                                   "y",
                                                                   "width",
                                                                   "height",
                                                                   "fill",
                                                                   "stroke",
                                                                   "fill-rule",
                                                                   "fill-opacity",
                                                                   "stroke-opacity",
                                                                   "stroke-width",
                                                                   "opacity",
                                                                   "stroke-linecap",
                                                                   "stroke-linejoin",
                                                                   "stroke-dasharray",
                                                                   "rx",
                                                                   "ry",
                                                                   "transform"};
        detail::append_metadata(out, rule.metadata, reserved);
        out += "/>";
        return;
    }
    if (rule.role == scene::ElementRole::table_frame) {
        append_table_markers(out, rule.metadata);
        detail::append_attribute(out, "width", number(rule.bounds.width * 1000));
        detail::append_attribute(out, "height", number(rule.bounds.height * 1000));
        detail::append_attribute(out, "x", number(rule.bounds.x * 1000));
        detail::append_attribute(out, "y", number(rule.bounds.y * 1000));
        append_table_stroke(out, rule.paint);
        if (rule.corner_radius)
            detail::append_attribute(out, "rx", number(*rule.corner_radius * 1000));
        out += "/>";
        return;
    }
    const bool background = rule.role == scene::ElementRole::background_box;
    if (rule.role == scene::ElementRole::link_hitbox) {
        detail::append_attribute(out, "data-hitbox", "true");
        detail::append_attribute(out, "fill", "none");
        detail::append_attribute(out, "stroke", "none");
        detail::append_attribute(out, "pointer-events", "all");
        detail::append_attribute(out, "width", number(rule.bounds.width * 1000));
        detail::append_attribute(out, "height", number(rule.bounds.height * 1000));
        detail::append_attribute(out, "x", number(rule.bounds.x * 1000));
        detail::append_attribute(out, "y", number(rule.bounds.y * 1000));
        if (rule.corner_radius)
            detail::append_attribute(out, "rx", number(*rule.corner_radius * 1000));
        auto paint = rule.paint;
        paint.fill = scene::InheritedPaint{};
        paint.stroke = scene::InheritedPaint{};
        append_paint(out, paint);
        static constexpr std::array<std::string_view, 17> reserved{
            "data-hitbox", "fill", "stroke", "pointer-events", "width", "height", "x", "y",
            "rx", "fill-rule", "fill-opacity", "stroke-opacity", "stroke-width", "opacity",
            "stroke-linecap", "stroke-linejoin", "stroke-dasharray"};
        detail::append_metadata(out, rule.metadata, reserved);
        out += "/>";
        return;
    }
    if (background)
        append_paint(out, rule.paint);
    if (rule.role == scene::ElementRole::fraction_rule ||
        rule.role == scene::ElementRole::radical_rule) {
        detail::append_attribute(out, "width", number(rule.bounds.width * 1000));
        detail::append_attribute(out, "height", number(rule.bounds.height * 1000));
        detail::append_attribute(out, "x", number(rule.bounds.x * 1000));
        detail::append_attribute(out, "y", number(rule.bounds.y * 1000));
    } else {
        detail::append_attribute(out, "x", number(rule.bounds.x * 1000));
        detail::append_attribute(out, "y", number(rule.bounds.y * 1000));
        detail::append_attribute(out, "width", number(rule.bounds.width * 1000));
        detail::append_attribute(out, "height", number(rule.bounds.height * 1000));
    }
    if (!background)
        append_paint(out, rule.paint);
    if (rule.corner_radius)
        detail::append_attribute(out, "rx", number(*rule.corner_radius * 1000));
    static constexpr std::array<std::string_view, 17> reserved{"x",
                                                               "y",
                                                               "width",
                                                               "height",
                                                               "fill",
                                                               "stroke",
                                                               "fill-rule",
                                                               "fill-opacity",
                                                               "stroke-opacity",
                                                               "stroke-width",
                                                               "opacity",
                                                               "stroke-linecap",
                                                               "stroke-linejoin",
                                                               "stroke-dasharray",
                                                               "data-hitbox",
                                                               "pointer-events",
                                                               "rx"};
    detail::append_metadata(out, rule.metadata, reserved);
    out += "/>";
}

std::vector<scene::Point> path_vertices(scene::PathGeometry const &geometry) {
    std::vector<scene::Point> points;
    scene::Point current{};
    for (auto const &segment : geometry.segments)
        std::visit(
            [&](auto const &part) {
                using T = std::decay_t<decltype(part)>;
                if constexpr (std::is_same_v<T, scene::MoveTo>) {
                    current = part.to;
                    points.push_back(current);
                } else if constexpr (std::is_same_v<T, scene::LineTo>) {
                    if (part.mode == scene::CoordinateMode::relative) {
                        current.x += part.to.x;
                        current.y += part.to.y;
                    } else
                        current = part.to;
                    points.push_back(current);
                }
            },
            segment);
    return points;
}

void append_path_element(std::string &out, scene::Path const &path) {
    bool const table_line = path.role == scene::ElementRole::table_vertical_line ||
                            path.role == scene::ElementRole::table_horizontal_line;
    auto const tag = path.element == scene::PathElement::polygon ? "polygon"
                     : path.element == scene::PathElement::line  ? "line"
                                                                 : "path";
    out += '<';
    out += tag;
    if (table_line)
        append_table_markers(out, path.metadata);
    if (path.paint_encoding == scene::PathPaintEncoding::stroke_width_style)
        detail::append_attribute(out, "style",
                                 "stroke-width: " + number(path.paint.stroke_width * 1000) + ";");
    if (path.element == scene::PathElement::path) {
        std::string data;
        append_path(data, path.geometry, path.syntax);
        detail::append_attribute(out, "d", data);
    } else {
        auto const points = path_vertices(path.geometry);
        if (path.element == scene::PathElement::polygon) {
            std::string values;
            for (auto const &point : points) {
                if (!values.empty())
                    values += ' ';
                values += number(point.x * 1000) + "," + number(point.y * 1000);
            }
            detail::append_attribute(out, "points", values);
        } else if (points.size() >= 2) {
            detail::append_attribute(out, "x1", number(points[0].x * 1000));
            detail::append_attribute(out, "y1", number(points[0].y * 1000));
            detail::append_attribute(out, "x2", number(points[1].x * 1000));
            detail::append_attribute(out, "y2", number(points[1].y * 1000));
        }
    }
    const bool dashed_border_line = path.element == scene::PathElement::line &&
                                    path.role == scene::ElementRole::background_box;
    const bool enclosure_line = path.element == scene::PathElement::line &&
                                path.role == scene::ElementRole::enclosure_mark;
    if (dashed_border_line) {
        // Source Wrapper.addBorderBroken writes line-specific attributes in this order.
        detail::append_attribute(out, "stroke-width", number(path.paint.stroke_width * 1000));
        if (auto const *css = std::get_if<scene::CssColor>(&path.paint.stroke))
            detail::append_attribute(out, "stroke", css->value);
        else
            append_color(out, "stroke", "stroke-opacity", path.paint.stroke);
        if (path.paint.line_cap)
            detail::append_attribute(out, "stroke-linecap", stroke_cap_name(*path.paint.line_cap));
        if (path.paint.dash_array) {
            std::string values;
            for (auto const length : *path.paint.dash_array) {
                if (!values.empty())
                    values.push_back(' ');
                values += number(length * 1000);
            }
            if (path.paint.dash_array->empty())
                values = "none";
            detail::append_attribute(out, "stroke-dasharray", values);
        }
    } else if (enclosure_line) {
        // Menclose line primitives inherit both fill and stroke from the group.
        detail::append_attribute(out, "stroke-width", number(path.paint.stroke_width * 1000));
        auto paint = path.paint;
        paint.stroke_width = 0;
        append_paint(out, paint);
    } else if (path.paint_encoding == scene::PathPaintEncoding::stroke_width_style) {
        if (path.paint.line_cap)
            detail::append_attribute(out, "stroke-linecap", stroke_cap_name(*path.paint.line_cap));
        if (path.paint.line_join) {
            const std::string_view spelling = path.join_spelling == scene::StrokeJoinSpelling::mitre
                                                  ? std::string_view{"mitre"}
                                                  : stroke_join_name(*path.paint.line_join);
            detail::append_attribute(out, "stroke-linejoin", spelling);
        }
        append_color(out, "fill", "fill-opacity", path.paint.fill);
        if (path.paint.fill_rule == scene::FillRule::evenodd)
            detail::append_attribute(out, "fill-rule", "evenodd");
        append_color(out, "stroke", "stroke-opacity", path.paint.stroke);
        if (path.paint.opacity != 1)
            detail::append_attribute(out, "opacity", number(path.paint.opacity, 3));
        if (path.paint.dash_array) {
            std::string values;
            for (auto const length : *path.paint.dash_array) {
                if (!values.empty())
                    values.push_back(' ');
                values += number(length * 1000);
            }
            detail::append_attribute(out, "stroke-dasharray", values.empty() ? "none" : values);
        }
    } else if (!table_line)
        append_paint(out, path.paint, path.role == scene::ElementRole::background_box);
    else {
        append_table_stroke(out, path.paint);
        out += "/>";
        return;
    }
    static constexpr std::array<std::string_view, 18> reserved_with_source_path{"d",
                                                                                "points",
                                                                                "x1",
                                                                                "y1",
                                                                                "x2",
                                                                                "y2",
                                                                                "transform",
                                                                                "style",
                                                                                "fill",
                                                                                "stroke",
                                                                                "fill-rule",
                                                                                "fill-opacity",
                                                                                "stroke-opacity",
                                                                                "stroke-width",
                                                                                "opacity",
                                                                                "stroke-dasharray",
                                                                                "stroke-linecap",
                                                                                "stroke-linejoin"};
    if (path.layout_width) {
        static constexpr std::array<std::string_view, 19> reserved_with_width{"d",
                                                                              "points",
                                                                              "x1",
                                                                              "y1",
                                                                              "x2",
                                                                              "y2",
                                                                              "width",
                                                                              "transform",
                                                                              "style",
                                                                              "fill",
                                                                              "stroke",
                                                                              "fill-rule",
                                                                              "fill-opacity",
                                                                              "stroke-opacity",
                                                                              "stroke-width",
                                                                              "opacity",
                                                                              "stroke-dasharray",
                                                                              "stroke-linecap",
                                                                              "stroke-linejoin"};
        detail::append_metadata(out, path.metadata, reserved_with_width);
    } else {
        detail::append_metadata(out, path.metadata, reserved_with_source_path);
    }
    if (path.layout_width)
        detail::append_attribute(out, "width", number(*path.layout_width * 1000));
    append_transform(out, path.transform, path.role);
    out += "/>";
}

void append_ellipse(std::string &out, scene::Ellipse const &ellipse) {
    out += "<ellipse";
    detail::append_attribute(out, "rx", number(ellipse.radius_x * 1000));
    detail::append_attribute(out, "ry", number(ellipse.radius_y * 1000));
    detail::append_attribute(out, "cx", number(ellipse.center.x * 1000));
    detail::append_attribute(out, "cy", number(ellipse.center.y * 1000));
    append_paint(out, ellipse.paint);
    static constexpr std::array<std::string_view, 15> reserved{"rx",
                                                               "ry",
                                                               "cx",
                                                               "cy",
                                                               "fill",
                                                               "stroke",
                                                               "fill-rule",
                                                               "fill-opacity",
                                                               "stroke-opacity",
                                                               "stroke-width",
                                                               "opacity",
                                                               "stroke-linecap",
                                                               "stroke-linejoin",
                                                               "stroke-dasharray",
                                                               "transform"};
    detail::append_metadata(out, ellipse.metadata, reserved);
    out += "/>";
}

void append_text_run(std::string &out, scene::TextRun const &run) {
    out += "<text";
    if (run.role == scene::ElementRole::id_alignment_marker)
        detail::append_attribute(out, "data-id-align", "true");
    bool const empty_alignment_marker =
        run.role == scene::ElementRole::id_alignment_marker && run.utf8.empty();
    const bool source_unknown_text = run.role == scene::ElementRole::unknown_text;
    if (source_unknown_text && run.font)
        detail::append_attribute(out, "data-variant", run.font->variant);
    if (!empty_alignment_marker) {
        std::string transform = "scale(1,-1)";
        if (run.origin.x || run.origin.y)
            transform = "translate(" + number(run.origin.x * 1000) +
                        (source_unknown_text ? "," : " ") +
                        number(run.origin.y * 1000) + ") " + transform;
        detail::append_attribute(out, "transform", transform);
    }
    if (run.font) {
        if (source_unknown_text) {
            detail::append_attribute(out, "font-size", number(run.font->size * 1000) + "px");
            if (run.font->variant != "-explicitFont" && !run.font->family.empty()) {
                detail::append_attribute(out, "font-family", run.font->family);
                if (run.font->italic)
                    detail::append_attribute(out, "font-style", "italic");
                if (run.font->bold)
                    detail::append_attribute(out, "font-weight", "bold");
            }
        } else {
            detail::append_attribute(out, "font-family", run.font->family);
            detail::append_attribute(out, "font-size", number(run.font->size * 1000) + "px");
            if (run.font->italic)
                detail::append_attribute(out, "font-style", "italic");
            if (run.font->bold)
                detail::append_attribute(out, "font-weight", "bold");
            if (!run.font->variant.empty())
                detail::append_attribute(out, "data-variant", run.font->variant);
        }
    }
    append_paint(out, run.paint);
    static constexpr std::array<std::string_view, 17> reserved{
        "transform",       "font-family",  "font-size",        "font-style",   "font-weight",
        "data-variant",    "fill",         "stroke",           "fill-opacity", "stroke-opacity",
        "opacity",         "stroke-width", "stroke-dasharray", "fill-rule",    "stroke-linecap",
        "stroke-linejoin", "data-id-align"};
    detail::append_metadata(out, run.metadata, reserved);
    if (run.utf8.empty())
        out += "/>";
    else
        out += '>' + detail::escape_text(run.utf8) + "</text>";
}

void append_primitive(std::string &out, scene::Primitive const &primitive,
                      SvgWriteOptions const &options, FontCachePlan const *plan,
                      CacheScopeId scope) {
    std::visit(
        [&](auto const &value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, scene::GlyphRun>)
                append_glyphs(out, value, options, plan, scope);
            else if constexpr (std::is_same_v<T, scene::Rule>)
                append_rule(out, value);
            else if constexpr (std::is_same_v<T, scene::Path>)
                append_path_element(out, value);
            else if constexpr (std::is_same_v<T, scene::Ellipse>)
                append_ellipse(out, value);
            else if constexpr (std::is_same_v<T, scene::TextRun>)
                append_text_run(out, value);
            else if constexpr (std::is_same_v<T, scene::Title>) {
                // LiteParser treats title as PCDATA, which serializeInner copies verbatim.
                if (value.text.empty())
                    out += "<title/>";
                else
                    out += "<title>" + value.text + "</title>";
            } else if constexpr (std::is_same_v<T, scene::Image>) {
                out += "<image";
                detail::append_attribute(out, "width", number(value.size.width * 1000));
                detail::append_attribute(out, "height", number(value.size.height * 1000));
                detail::append_attribute(
                    out, "transform",
                    transform_value(value.transform, scene::ElementRole::image_glyph));
                detail::append_attribute(out, "preserveAspectRatio", "none");
                detail::append_attribute(out, "aria-label", value.alt);
                detail::append_attribute(out, "href", value.href);
                out += "/>";
            }
        },
        primitive);
}

void open_group(std::string &out, scene::Group const &group, SvgWriteOptions const &options,
                FontCachePlan const *plan, CacheScopeId scope);
void append_action_region(std::string &out, scene::ActionRegion const &region,
                          SvgWriteOptions const &options, FontCachePlan const *plan,
                          CacheScopeId scope);

void append_group_entries(std::string &out, scene::Group const &group,
                          SvgWriteOptions const &options, FontCachePlan const *plan,
                          CacheScopeId scope) {
    for (auto const &entry : group.entries) {
        if (auto child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child)
                open_group(out, **child, options, plan, scope);
        } else if (auto primitive = std::get_if<scene::Primitive>(&entry)) {
            append_primitive(out, *primitive, options, plan, scope);
        } else {
            append_action_region(out, std::get<scene::ActionRegion>(entry), options, plan, scope);
        }
    }
}

void open_group(std::string &out, scene::Group const &group, SvgWriteOptions const &options,
                FontCachePlan const *plan, CacheScopeId scope) {
    if (group.foreign_content) {
        auto const &foreign = *group.foreign_content;
        auto const &geometry = foreign.geometry;
        out += "<foreignObject";
        detail::append_attribute(out, "data-mjx-xml", "true");
        if (geometry.x != 0)
            detail::append_attribute(out, "x", foreign_fixed(geometry.x) + "px");
        detail::append_attribute(out, "y", foreign_fixed(geometry.y) + "px");
        detail::append_attribute(out, "width", foreign_fixed(geometry.width) + "px");
        detail::append_attribute(out, "height", foreign_fixed(geometry.height) + "px");
        static constexpr std::array<std::string_view, 6> reserved{
            "data-mjx-xml", "x", "y", "width", "height", "transform"};
        detail::append_metadata(out, foreign.metadata, reserved);
        std::string transform = transform_value(group.transform, group.role);
        if (!transform.empty())
            transform.push_back(' ');
        transform +=
            "scale(" + foreign_fixed(1000 * geometry.em_per_pixel) + ") matrix(1 0 0 -1 0 0)";
        detail::append_attribute(out, "transform", transform);
        out += '>';
        out += serializeHtml(foreign.content);
        out += "</foreignObject>";
        return;
    }
    auto const inherited_paint = [](scene::Paint const &paint) {
        return std::holds_alternative<scene::InheritedPaint>(paint.fill) &&
               std::holds_alternative<scene::InheritedPaint>(paint.stroke) &&
               paint.stroke_width == 0 && paint.opacity == 1 &&
               paint.fill_rule == scene::FillRule::nonzero && !paint.line_cap && !paint.line_join &&
               !paint.dash_array && !paint.stylesheet;
    };
    bool const sentinel =
        group.id == 0 && group.transform.operations().empty() && !group.clip && !group.link &&
        !group.tooltip && group.role == scene::ElementRole::math_wrapper &&
        group.metadata.source_tag.empty() && group.metadata.math_role.empty() &&
        group.metadata.annotation_encoding.empty() && group.metadata.annotation.empty() &&
        group.metadata.ordered_attributes.empty() && !group.metadata.initial_transform &&
        inherited_paint(group.paint) && !group.viewport;
    bool const self_closing = !sentinel && group.entries.empty() && !group.tooltip && !group.clip;
    bool const is_viewport = group.viewport.has_value();
    if (group.link) {
        out += "<a";
        detail::append_attribute(out, "href", group.link->href);
        if (!group.link->target.empty())
            detail::append_attribute(out, "target", group.link->target);
        if (!group.link->rel.empty())
            detail::append_attribute(out, "rel", group.link->rel);
        out += '>';
    }
    if (!sentinel)
        out += is_viewport ? "<svg" : "<g";
    static constexpr std::array<std::string_view, 24> reserved{"data-mml-node",
                                                               "transform",
                                                               "clip-path",
                                                               "fill",
                                                               "stroke",
                                                               "fill-rule",
                                                               "fill-opacity",
                                                               "stroke-opacity",
                                                               "stroke-width",
                                                               "opacity",
                                                               "stroke-linecap",
                                                               "stroke-linejoin",
                                                               "stroke-dasharray",
                                                               "href",
                                                               "target",
                                                               "rel",
                                                               "style",
                                                               "x",
                                                               "y",
                                                               "width",
                                                               "height",
                                                               "viewBox",
                                                               "preserveAspectRatio",
                                                               "xmlns"};
    if (!sentinel) {
        auto const is_extender = group.role == scene::ElementRole::vertical_extender_viewport ||
                                 group.role == scene::ElementRole::horizontal_extender_viewport;
        auto append_viewbox = [&] {
            auto const &v = *group.viewport;
            detail::append_attribute(
                out, "viewBox",
                number(v.view_box.x * 1000) + " " + number(v.view_box.y * 1000) + " " +
                    number(v.view_box.width * 1000) + " " + number(v.view_box.height * 1000));
        };
        auto append_aspect = [&] {
            if (!group.viewport->aspect_ratio)
                return;
            auto token = [](scene::ViewportAlignment a) -> std::string_view {
                switch (a) {
                case scene::ViewportAlignment::min:
                    return "Min";
                case scene::ViewportAlignment::mid:
                    return "Mid";
                case scene::ViewportAlignment::max:
                    return "Max";
                }
                throw std::invalid_argument("unknown viewport alignment");
            };
            auto const &a = *group.viewport->aspect_ratio;
            std::string value = "x" + std::string(token(a.x)) + "Y" + std::string(token(a.y));
            switch (a.fit) {
            case scene::ViewportFit::meet:
                break;
            case scene::ViewportFit::slice:
                value += " slice";
                break;
            case scene::ViewportFit::none:
                value = "none";
                break;
            default:
                throw std::invalid_argument("unknown viewport fit");
            }
            detail::append_attribute(out, "preserveAspectRatio", value);
        };
        auto append_viewport_bounds = [&] {
            auto const &b = group.viewport->bounds;
            detail::append_attribute(out, "width", number(b.width * 1000));
            detail::append_attribute(out, "height", number(b.height * 1000));
            if (group.role == scene::ElementRole::vertical_extender_viewport) {
                detail::append_attribute(out, "y", number(b.y * 1000));
                detail::append_attribute(out, "x", number(b.x * 1000));
            } else if (group.role == scene::ElementRole::horizontal_extender_viewport) {
                detail::append_attribute(out, "x", number(b.x * 1000));
                detail::append_attribute(out, "y", number(b.y * 1000));
            } else {
                detail::append_attribute(out, "x", number(b.x * 1000));
                detail::append_attribute(out, "y", number(b.y * 1000));
            }
        };
        if (!group.metadata.source_tag.empty())
            detail::append_attribute(out, "data-mml-node", group.metadata.source_tag);
        auto const style =
            std::ranges::find(group.metadata.ordered_attributes, std::string_view{"style"},
                              &scene::SemanticAttribute::name);
        if (style != group.metadata.ordered_attributes.end())
            detail::append_attribute(out, "style", style->value);
        if (group.metadata.initial_transform && !group.transform.operations().empty())
            append_transform(out, group.transform, group.role);
        if (is_viewport && is_extender)
            append_viewport_bounds();
        append_paint(out, group.paint);
        if (group.clip)
            detail::append_attribute(out, "clip-path",
                                     "url(#clip-" + std::to_string(group.id) + ")");
        detail::append_metadata(out, group.metadata, reserved);
        if (is_viewport) {
            append_aspect();
            if (!is_extender && group.role != scene::ElementRole::table_content_viewport &&
                group.role != scene::ElementRole::table_label_frame)
                append_viewport_bounds();
            append_viewbox();
            detail::append_attribute(out, "xmlns", "http://www.w3.org/2000/svg");
        }
        if (!group.metadata.initial_transform && !group.transform.operations().empty())
            append_transform(out, group.transform, group.role);
        out += self_closing ? "/>" : ">";
    }
    if (!self_closing) {
        if (group.tooltip) {
            if (group.tooltip->text.empty())
                out += "<title/>";
            else
                out += "<title>" + detail::escape_text(group.tooltip->text) + "</title>";
        }
        if (group.clip) {
            out += "<defs><clipPath id=\"clip-" + std::to_string(group.id) + "\"><path d=\"";
            append_path(out, group.clip->geometry);
            out += "\"";
            if (group.clip->fill_rule == scene::FillRule::evenodd)
                detail::append_attribute(out, "clip-rule", "evenodd");
            out += "/></clipPath></defs>";
        }
        append_group_entries(out, group, options, plan, scope);
    }
    if (!sentinel && !self_closing)
        out += is_viewport ? "</svg>" : "</g>";
    if (group.link)
        out += "</a>";
}

void append_font_cache(std::string &out, FontCachePlan const &plan, CacheScopeId scope,
                       SvgWriteOptions const &options) {
    auto const &cache = plan.scope(scope).definitions;
    if (cache.empty()) {
        out += "<defs/>";
        return;
    }
    out += "<defs>";
    for (auto const &definition : cache) {
        if (!definition.glyph)
            throw std::logic_error("FontCachePlan definition has no glyph");
        out += "<path";
        detail::append_attribute(out, "id", definition.id);
        detail::append_attribute(out, "d", path_data(*definition.glyph, options));
        out += "/>";
    }
    out += "</defs>";
}

void append_svg_start(std::string &out, scene::Metrics const &metrics,
                      scene::FontMetrics const &font, SvgWriteOptions const &options,
                      scene::DisplayLayout const &layout, double minimum_extent,
                      bool apply_inline_layout, FontCachePlan const *plan = nullptr,
                      CacheScopeId scope = 0, bool cache_before_group = false,
                      scene::PercentageFrame const *percentage = nullptr) {
    const double width = std::max(metrics.width, minimum_extent);
    const double height = std::max(metrics.height + metrics.depth, minimum_extent);
    out += "<svg";
    if (percentage) {
        std::string style = "vertical-align: " + ex_value(-metrics.depth, font.x_height) + ";";
        if (layout.minimum_width > 0)
            style += " min-width: " + ex_value(layout.minimum_width, font.x_height) + ";";
        if (apply_inline_layout && layout.shift != 0) {
            if (layout.alignment == "center" || layout.alignment == "left")
                style += " margin-left: " + ex_value(layout.shift, font.x_height) + ";";
            if (layout.alignment == "center" || layout.alignment == "right")
                style += " margin-right: " + ex_value(-layout.shift, font.x_height) + ";";
        }
        detail::append_attribute(out, "style", style);
        detail::append_attribute(out, "xmlns", "http://www.w3.org/2000/svg");
        detail::append_attribute(out, "width", percentage->width_text);
        detail::append_attribute(out, "height", ex_value(height, font.x_height));
        detail::append_attribute(out, "role", "img");
        detail::append_attribute(out, "focusable", "false");
        if (options.font_cache != FontCacheMode::none && options.use_xlink)
            detail::append_attribute(out, "xmlns:xlink", "http://www.w3.org/1999/xlink");
        detail::append_attribute(out, "data-mjx-viewBox",
                                 "0 " + number(-metrics.height * 1000) + " " +
                                     number(width * 1000) + " " + number(height * 1000));
        out += ">";
        if (cache_before_group && plan)
            append_font_cache(out, *plan, scope, options);
        out += "<g stroke=\"currentColor\" fill=\"currentColor\" stroke-width=\"0\" "
               "transform=\"scale(";
        out += number(percentage->scale, 6);
        out += ",-";
        out += number(percentage->scale, 6);
        out += ") translate(0, ";
        out += number(-metrics.height * 1000, 1);
        out += ")\">";
        return;
    }
    std::string style = "vertical-align: " + ex_value(-metrics.depth, font.x_height) + ";";
    if (apply_inline_layout) {
        if (layout.minimum_width > 0)
            style += " min-width: " + ex_value(layout.minimum_width, font.x_height) + ";";
        if (layout.shift != 0) {
            if (layout.alignment == "center" || layout.alignment == "left")
                style += " margin-left: " + ex_value(layout.shift, font.x_height) + ";";
            if (layout.alignment == "center" || layout.alignment == "right")
                style += " margin-right: " + ex_value(-layout.shift, font.x_height) + ";";
        }
    }
    detail::append_attribute(out, "style", style);
    detail::append_attribute(out, "xmlns", "http://www.w3.org/2000/svg");
    detail::append_attribute(out, "width", ex_value(width, font.x_height));
    detail::append_attribute(out, "height", ex_value(height, font.x_height));
    detail::append_attribute(out, "role", "img");
    detail::append_attribute(out, "focusable", "false");
    detail::append_attribute(out, "viewBox",
                             "0 " + number(-metrics.height * 1000) + " " + number(width * 1000) +
                                 " " + number(height * 1000));
    if (options.font_cache != FontCacheMode::none && options.use_xlink)
        detail::append_attribute(out, "xmlns:xlink", "http://www.w3.org/1999/xlink");
    out += ">";
    if (cache_before_group && plan)
        append_font_cache(out, *plan, scope, options);
    out += "<g stroke=\"currentColor\" fill=\"currentColor\" stroke-width=\"0\" "
           "transform=\"scale(1,-1)\">";
}

void append_svg_end(std::string &out, FontCachePlan const *plan = nullptr, CacheScopeId scope = 0,
                    bool append_trailing_cache = false, SvgWriteOptions const *options = nullptr) {
    out += "</g>";
    if (append_trailing_cache && plan) {
        if (!options)
            throw std::logic_error("FontCachePlan defs require SVG write options");
        append_font_cache(out, *plan, scope, *options);
    }
    out += "</svg>";
}

scene::Group const &frame_group(scene::Group const &root, scene::NodeId id) {
    for (auto const &entry : root.entries) {
        if (auto child = std::get_if<std::shared_ptr<scene::Group const>>(&entry);
            child && *child && (*child)->id == id)
            return **child;
    }
    throw std::logic_error("validated Scene frame root is missing");
}

void append_inline_break(std::string &out, scene::InlineBreak const &value);

void append_nested_scene_svgs(std::string &out, scene::Scene const &nested,
                              SvgWriteOptions const &options, FontCachePlan const *plan,
                              CacheScopeId scope) {
    auto const &metrics = nested.metrics();
    auto const &font = nested.font_metrics();
    auto const &layout = nested.display_layout();
    auto const &frames = nested.frame_layout();
    if (frames.inline_fragments.empty()) {
        append_svg_start(out, metrics, font, options, layout, frames.minimum_extent, false, plan,
                         scope, true, frames.percentage ? &*frames.percentage : nullptr);
        open_group(out, nested.root(), options, plan, scope);
        append_svg_end(out);
        return;
    }
    for (std::size_t i = 0; i < frames.inline_fragments.size(); ++i) {
        auto const &fragment = frames.inline_fragments[i];
        if (fragment.before)
            append_inline_break(out, *fragment.before);
        append_svg_start(out, fragment.metrics, font, options, layout, frames.minimum_extent, false,
                         plan, scope, false,
                         i == 0 && frames.percentage ? &*frames.percentage : nullptr);
        open_group(out, frame_group(nested.root(), fragment.root), options, plan, scope);
        append_svg_end(out, plan, scope, i == 0, &options);
    }
}

void append_action_region(std::string &out, scene::ActionRegion const &region,
                          SvgWriteOptions const &options, FontCachePlan const *plan,
                          CacheScopeId scope) {
    if (region.tooltip && options.font_cache == FontCacheMode::global)
        throw UnsupportedError(
            "global SVG font caching requires the owning document cache serializer");

    out += "<rect";
    detail::append_attribute(out, "width", number(region.hitbox.bounds.width * 1000));
    detail::append_attribute(out, "height", number(region.hitbox.bounds.height * 1000));
    detail::append_attribute(out, "x", number(region.hitbox.bounds.x * 1000));
    detail::append_attribute(out, "y", number(region.hitbox.bounds.y * 1000));
    detail::append_attribute(out, "fill", "none");
    detail::append_attribute(out, "pointer-events", "all");
    static constexpr std::array<std::string_view, 17> reserved{"x",
                                                               "y",
                                                               "width",
                                                               "height",
                                                               "fill",
                                                               "stroke",
                                                               "fill-rule",
                                                               "fill-opacity",
                                                               "stroke-opacity",
                                                               "stroke-width",
                                                               "opacity",
                                                               "stroke-linecap",
                                                               "stroke-linejoin",
                                                               "stroke-dasharray",
                                                               "pointer-events",
                                                               "rx",
                                                               "ry"};
    detail::append_metadata(out, region.hitbox.metadata, reserved);
    if (!region.tooltip) {
        out += "/>";
        return;
    }
    CacheScopeId nested_scope = 0;
    if (options.font_cache == FontCacheMode::local) {
        if (!plan)
            throw std::logic_error("math tooltip cache scope has no FontCachePlan");
        nested_scope = plan->scope(scope).tooltips.at(&region);
    }
    out += "><foreignObject style=\"display: none;\"><mjx-tool><mjx-tip>";
    append_nested_scene_svgs(out, *region.tooltip, options, plan, nested_scope);
    out += "</mjx-tip></mjx-tool></foreignObject>";
    out += "</rect>";
}

void append_inline_break(std::string &out, scene::InlineBreak const &value) {
    auto node = [&](std::string_view attribute, std::string_view content) {
        out += "<mjx-break";
        detail::append_attribute(out, attribute, content);
        out += "> </mjx-break>";
    };
    if (value.kind == scene::InlineBreakKind::opportunity) {
        node("prebreak", "true");
        node("newline", "true");
        return;
    }
    auto const em = folia::math::lengths::em(value.spacing);
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 6> spaces{
        {{"0", "0"},
         {"0.111em", "1"},
         {"0.167em", "2"},
         {"0.222em", "3"},
         {"0.278em", "4"},
         {"0.333em", "5"}}};
    using SpaceValue = std::pair<std::string_view, std::string_view>;
    if (auto found = std::ranges::find(spaces, std::string_view{em}, &SpaceValue::first);
        found != spaces.end()) {
        node("size", found->second);
    } else {
        node("style", "letter-spacing: " + folia::math::lengths::em(value.spacing - 1));
    }
}
} // namespace

std::string write(scene::Scene const &scene_value, SvgWriteOptions const &options) {
    validate_scene(scene_value.root());
    auto const &metrics = scene_value.metrics();
    auto const &font = scene_value.font_metrics();
    if (!std::isfinite(metrics.width) || !std::isfinite(metrics.height) ||
        !std::isfinite(metrics.depth))
        throw std::invalid_argument("Scene metrics must be finite");
    auto const &layout = scene_value.display_layout();
    std::optional<FontCachePlan> cache_plan;
    if (options.font_cache == FontCacheMode::local)
        cache_plan.emplace(scene_value, options.local_cache_prefixes);
    if (options.font_cache == FontCacheMode::global && has_glyphs(scene_value.root()))
        throw UnsupportedError(
            "global SVG font caching requires the owning document cache serializer");

    std::string out;
    if (options.include_math_container) {
        out += "<mjx-container";
        bool container_style = false;
        const bool scaled_container = layout.font_scale != 1;
        for (auto const &attribute : options.container_attributes) {
            if (attribute.name == "style" && scaled_container) {
                styles::Styles merged(utf16::decode(attribute.value).text);
                if (scaled_container)
                    merged.set(
                        u"font-size",
                        styles::StyleValue(
                            utf16::decode(folia::math::lengths::percent(layout.font_scale)).text));
                if (layout.minimum_width > 0)
                    merged.set(
                        u"min-width",
                        styles::StyleValue(
                            utf16::decode(ex_value(layout.minimum_width, font.x_height)).text));
                detail::append_attribute(out, attribute.name, utf16::encode(merged.cssText()));
                container_style = true;
            } else if (attribute.name == "style" && layout.minimum_width > 0) {
                detail::append_attribute(out, attribute.name,
                                         attribute.value + "; min-width: " +
                                             ex_value(layout.minimum_width, font.x_height) + ";");
                container_style = true;
            } else
                detail::append_attribute(out, attribute.name, attribute.value);
        }
        if (scaled_container && !container_style) {
            styles::Styles generated;
            generated.set(
                u"font-size",
                styles::StyleValue(
                    utf16::decode(folia::math::lengths::percent(layout.font_scale)).text));
            if (layout.minimum_width > 0)
                generated.set(
                    u"min-width",
                    styles::StyleValue(
                        utf16::decode(ex_value(layout.minimum_width, font.x_height)).text));
            detail::append_attribute(out, "style", utf16::encode(generated.cssText()));
            container_style = true;
        }
        if (layout.display)
            detail::append_attribute(out, "display", "true");
        if (layout.full_width)
            detail::append_attribute(out, "width", "full");
        if (layout.display && layout.alignment != "center")
            detail::append_attribute(out, "justify", layout.alignment);
        if (layout.minimum_width > 0 && !container_style)
            detail::append_attribute(
                out, "style", "min-width: " + ex_value(layout.minimum_width, font.x_height) + ";");
        out += '>';
    }
    auto const &frames = scene_value.frame_layout();
    if (frames.inline_fragments.empty()) {
        append_svg_start(out, metrics, font, options, layout, frames.minimum_extent, true,
                         cache_plan ? &*cache_plan : nullptr, 0, true,
                         frames.percentage ? &*frames.percentage : nullptr);
        open_group(out, scene_value.root(), options, cache_plan ? &*cache_plan : nullptr, 0);
        append_svg_end(out);
    } else {
        for (std::size_t i = 0; i < frames.inline_fragments.size(); ++i) {
            auto const &fragment = frames.inline_fragments[i];
            if (fragment.before)
                append_inline_break(out, *fragment.before);
            append_svg_start(out, fragment.metrics, font, options, layout, frames.minimum_extent,
                             false);
            open_group(out, frame_group(scene_value.root(), fragment.root), options,
                       cache_plan ? &*cache_plan : nullptr, 0);
            append_svg_end(out, cache_plan ? &*cache_plan : nullptr, 0, i == 0, &options);
        }
    }
    if (options.include_math_container)
        out += "</mjx-container>";
    return out;
}
} // namespace folia::math::output::svg
