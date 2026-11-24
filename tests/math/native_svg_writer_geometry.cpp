#include "support/folia_test.hpp"
import folia.math.output.scene.builder;
import folia.math.output.svg.serializer;
import folia.math.output.svg.writer;

namespace {
using namespace boost::ut;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

scene::Scene make_scene() {
    scene::Builder builder;
    builder.set_metrics({2.0, 0.8, 0.2});
    builder.set_font_metrics({1.0, 0.5, 0.45, 0.7});
    return std::move(builder).freeze();
}
} // namespace

suite native_svg_writer_geometry = [] {
    "ordered groups retain SVG-space arc flags under reflection and metadata"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::SemanticMetadata metadata;
        metadata.source_tag = "mrow";
        metadata.ordered_attributes.push_back({"class", "outer"});
        scene::Clip clip{scene::PathGeometry{{scene::MoveTo{{0, 0}}, scene::LineTo{{1, 0}},
                                              scene::LineTo{{1, 1}}, scene::ClosePath{}}},
                         scene::FillRule::evenodd};
        scene::Transform ordered_transform{std::vector<scene::TransformOperation>{
            scene::Translation{0.719, 0.363}, scene::Scaling{0.707, 0.707}}};
        auto parent =
            builder.add_group(0, ordered_transform, clip,
                              scene::Link{"https://example.test/?a&b", "_blank", "noopener",
                                           {scene::CssColor{"blue"}, scene::CssColor{"blue"}}},
                              scene::Tooltip{"x<&"}, metadata);
        builder.add_primitive(
            parent,
            scene::Rule{{0, 0.2, 0.25, 0.05}, scene::Paint{.fill = scene::Color{255, 0, 0, 255}}});
        builder.add_primitive(parent,
                              scene::Rule{{0.1, -0.2, 0.3, 0.4}, {}, {},
                                          scene::ElementRole::link_hitbox});
        scene::Paint path_paint;
        path_paint.fill_rule = scene::FillRule::evenodd;
        builder.add_primitive(
            parent,
            scene::Path{scene::PathGeometry{{scene::MoveTo{{0, 0}},
                                             scene::ArcTo{0.1, 0.2, 30, true, false, {0.4, 0.3}}}},
                        path_paint});
        builder.add_group(parent, scene::Transform{scene::Affine{.e = 0.5, .f = 0.25}});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        auto anchor = output.find(
            "<a href=\"https://example.test/?a&amp;b\" target=\"_blank\" rel=\"noopener\" "
            "fill=\"blue\" stroke=\"blue\">");
        auto rule = output.find("<rect x=\"0\" y=\"200\" width=\"250\" height=\"50\"");
        auto hitbox = output.find("<rect data-hitbox=\"true\" fill=\"none\" stroke=\"none\" "
                                  "pointer-events=\"all\" width=\"300\" height=\"400\" "
                                  "x=\"100\" y=\"-200\"/>");
        auto child = output.find("transform=\"matrix(1 0 0 1 500 250)\"");
        expect(output.find("<title>x&lt;&amp;</title>") != output.npos);
        expect(output.find("clip-path=\"url(#clip-") != output.npos);
        expect(output.find("<clipPath id=\"clip-") != output.npos);
        expect(output.find("clip-rule=\"evenodd\"") != output.npos);
        expect(output.find("fill-rule=\"evenodd\"") != output.npos);
        expect(output.find("A100 200 30 1 0 400,300") != output.npos);
        expect(output.find("transform=\"translate(719,363) scale(0.707)\"") != output.npos);
        expect(output.find("transform=\"scale(1,-1)\"") != output.npos);
        expect(anchor < rule && rule < child);
        expect(anchor < hitbox);
        expect(output.find("class=\"outer\"") != output.npos);
    };

    "empty source tags stay absent and typed primitive roles preserve Source SVG elements"_test =
        [] {
            scene::Builder builder;
            builder.set_metrics({2, 1, 0});
            builder.set_font_metrics({1, 0.5, 0.5, 0.7});
            scene::Paint group_paint;
            group_paint.fill = scene::CssColor{"rebeccapurple"};
            group_paint.opacity = 0.75;
            auto group = builder.add_group(0, {}, {}, {}, {}, {},
                                           scene::ElementRole::anonymous_group, group_paint);
            scene::SemanticMetadata line_meta;
            line_meta.ordered_attributes.push_back({"class", "rule"});
            scene::Paint line_paint;
            line_paint.fill = scene::NoPaint{};
            line_paint.stroke = scene::CurrentColorPaint{};
            builder.add_primitive(
                group, scene::Path{scene::PathGeometry{
                                       {scene::MoveTo{{0.1, 0.2}}, scene::LineTo{{0.8, 0.9}}}},
                                   line_paint, line_meta, scene::ElementRole::fraction_rule,
                                   scene::PathElement::line});
            scene::SemanticMetadata polygon_meta;
            polygon_meta.ordered_attributes.push_back({"data-kind", "piece"});
            scene::Paint polygon_paint;
            polygon_paint.fill = scene::NoPaint{};
            polygon_paint.stroke = scene::CurrentColorPaint{};
            builder.add_primitive(
                group,
                scene::Path{scene::PathGeometry{{scene::MoveTo{{0, 0}}, scene::LineTo{{1, 0}},
                                                 scene::LineTo{{0.5, 1}}, scene::ClosePath{}}},
                            polygon_paint, polygon_meta, scene::ElementRole::glyph_composition,
                            scene::PathElement::polygon});
            auto output =
                svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
            expect(output.find("data-mml-node=\"mrow\"") == output.npos);
            expect(output.find("fill=\"rebeccapurple\" opacity=\"0.75\"") != output.npos);
            expect(output.find("<line x1=\"100\" y1=\"200\" x2=\"800\" y2=\"900\" fill=\"none\" "
                               "stroke=\"currentColor\" class=\"rule\"") != output.npos);
            expect(output.find("<polygon points=\"0,0 1000,0 500,1000\" fill=\"none\" "
                               "stroke=\"currentColor\" data-kind=\"piece\"") != output.npos);
        };

    "Source initial transforms bracket metadata at the recorded phase"_test = [] {
        scene::Builder initial;
        initial.set_metrics({1, 0.5, 0.1});
        initial.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::SemanticMetadata metadata;
        metadata.source_tag = "mstyle";
        metadata.ordered_attributes = {{"style", "color: red;"}, {"data-latex", "x"}};
        metadata.initial_transform = true;
        scene::Paint paint;
        paint.opacity = 0.5;
        auto group = initial.add_group(
            0, scene::Transform{std::vector<scene::TransformOperation>{scene::Scaling{0.75, 0.75}}},
            {}, {}, {}, metadata, scene::ElementRole::math_wrapper, paint);
        initial.add_primitive(group, scene::TextRun{"x", {}, {}, {}, {}});
        auto output =
            svg::write(std::move(initial).freeze(), {.font_cache = svg::FontCacheMode::none});
        auto tag = output.find("data-mml-node=\"mstyle\"");
        auto style = output.find("style=\"color: red;\"");
        auto transform = output.find("transform=\"scale(0.75)\"");
        auto opacity = output.find("opacity=\"0.5\"");
        auto latex = output.find("data-latex=\"x\"");
        expect(tag < style && style < transform && transform < opacity && opacity < latex);

        scene::Builder placed;
        placed.set_metrics({1, 0.5, 0.1});
        placed.set_font_metrics({1, 0.5, 0.5, 0.7});
        metadata.initial_transform = false;
        auto placed_group = placed.add_group(
            0,
            scene::Transform{std::vector<scene::TransformOperation>{scene::Translation{0.2, 0.3}}},
            {}, {}, {}, metadata);
        placed.add_primitive(placed_group, scene::TextRun{"x", {}, {}, {}, {}});
        auto placed_output =
            svg::write(std::move(placed).freeze(), {.font_cache = svg::FontCacheMode::none});
        auto placed_latex = placed_output.find("data-latex=\"x\"");
        auto placed_transform = placed_output.find("transform=\"translate(200,300)\"");
        expect(placed_latex < placed_transform);
    };

    "unannotated root sentinel writes entries without adding a group"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        builder.add_primitive(0, scene::TextRun{"direct", {}, {}, {}, {}});
        auto child = builder.add_group(0, {}, {}, {}, {}, {}, scene::ElementRole::anonymous_group);
        builder.add_primitive(child, scene::TextRun{"nested", {}, {}, {}, {}});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        auto first_text = output.find("<text");
        auto child_group = output.find("<g", first_text);
        expect(output.find("data-mml-node=\"mrow\"") == output.npos);
        expect(first_text < child_group);
        expect(output.find("<g", child_group + 2) == output.npos);
    };

    "empty source groups self close only when they have no serialized children"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        builder.add_group(0, {}, {}, scene::Link{"https://example.test"});
        auto empty_text_group = builder.add_group();
        builder.add_primitive(empty_text_group, scene::TextRun{"", {}, {}, {}, {}});
        builder.add_group(0, {}, {}, {}, scene::Tooltip{""});
        builder.add_group(0, {}, scene::Clip{scene::PathGeometry{{scene::MoveTo{{0, 0}}}}});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<a href=\"https://example.test\"><g/></a>") != output.npos);
        expect(output.find("<g><text transform=\"scale(1,-1)\"/></g>") != output.npos);
        expect(output.find("<g><title/></g>") != output.npos);
        expect(output.find("<g clip-path=\"url(#clip-") != output.npos);
        expect(output.find("</clipPath></defs></g>") != output.npos);
        expect(output.find("<g stroke=\"currentColor\"", output.find("<svg")) != output.npos);
        expect(output.find("</g></svg>") != output.npos);
    };

    "typed translation rotation scale and affine operations stay ordered"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::Transform transform{std::vector<scene::TransformOperation>{
            scene::Translation{0, 0.1}, scene::Rotation{40.575, {0.519, 0.2155}},
            scene::Scaling{0.745, 1.0}, scene::Affine{1, 0, 0, -1, 0, 0}}};
        builder.add_group(0, transform);
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("transform=\"translate(0,100) rotate(40.575 519 215.5) scale(0.745,1) "
                           "matrix(1 0 0 -1 0 0)\"") != output.npos);
        expect(output.find("transform=\"scale(1,-1)\"") != output.npos);
    };

    "empty id alignment marker matches Source without a text reflection transform"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::TextRun marker;
        marker.role = scene::ElementRole::id_alignment_marker;
        builder.add_primitive(0, marker);
        auto output = svg::write(std::move(builder).freeze(),
                                 {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<text data-id-align=\"true\"/>") != output.npos);
        expect(output.find("<text data-id-align=\"true\" transform=") == output.npos);
    };

    "per-translation separators preserve spelling without changing composed geometry"_test = [] {
        auto spelled = scene::Transform{std::vector<scene::TransformOperation>{
            scene::Translation{0.1, 0.2, scene::TranslationSeparator::comma_space},
            scene::Translation{0.3, 0.4, scene::TranslationSeparator::space},
            scene::Translation{0.5, 0.6, scene::TranslationSeparator::comma}}};
        auto defaults = scene::Transform{std::vector<scene::TransformOperation>{
            scene::Translation{0.1, 0.2}, scene::Translation{0.3, 0.4},
            scene::Translation{0.5, 0.6}}};
        expect(spelled.matrix() == defaults.matrix());
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        builder.add_group(0, spelled, {}, {}, {}, {}, scene::ElementRole::operator_adjustment);
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find(
                   "transform=\"translate(100, 200) translate(300 400) translate(500,600)\"") !=
               output.npos);
    };

    "enclosure primitives preserve Source element syntax order and typed geometry"_test = [] {
        scene::Builder builder;
        builder.set_metrics({3, 1, 0.5});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});

        scene::Paint rounded_paint;
        rounded_paint.fill = scene::NoPaint{};
        rounded_paint.stroke_width = 0.067;
        scene::Rule rounded{
            {0.1, 0.2, 0.467, 0.8}, rounded_paint, {}, scene::ElementRole::enclosure_mark};
        rounded.corner_radius = 0.267;
        builder.add_primitive(0, rounded);

        scene::Paint ellipse_paint;
        ellipse_paint.fill = scene::CssColor{"#123456"};
        ellipse_paint.stroke = scene::CurrentColorPaint{};
        ellipse_paint.stroke_width = 0.04;
        builder.add_primitive(
            0, scene::Ellipse{
                   {0.5, -0.1}, 0.4, 0.2, ellipse_paint, {}, scene::ElementRole::enclosure_mark});

        scene::Paint phasor_paint;
        phasor_paint.fill = scene::NoPaint{};
        phasor_paint.stroke_width = 0.067;
        phasor_paint.line_cap = scene::StrokeCap::round;
        phasor_paint.line_join = scene::StrokeJoin::miter;
        scene::Path phasor;
        phasor.geometry.segments = {scene::MoveTo{{0, 0}}, scene::LineTo{{1, 1}}};
        phasor.paint = phasor_paint;
        phasor.role = scene::ElementRole::enclosure_mark;
        phasor.syntax = scene::PathSyntax::spaced;
        phasor.paint_encoding = scene::PathPaintEncoding::stroke_width_style;
        phasor.join_spelling = scene::StrokeJoinSpelling::mitre;
        builder.add_primitive(0, phasor);

        scene::Path longdiv;
        longdiv.geometry.segments = {scene::MoveTo{{0, 0}},
                                     scene::ArcTo{0.1,
                                                  0.1,
                                                  0,
                                                  false,
                                                  true,
                                                  {0, 0.5},
                                                  scene::CoordinateMode::relative,
                                                  scene::ArcFlagSeparator::comma},
                                     scene::LineTo{{1, 0.5}}};
        longdiv.paint.fill = scene::NoPaint{};
        longdiv.role = scene::ElementRole::enclosure_mark;
        longdiv.syntax = scene::PathSyntax::spaced;
        longdiv.paint_encoding = scene::PathPaintEncoding::stroke_width_style;
        builder.add_primitive(0, longdiv);

        scene::Path arrow;
        arrow.geometry.segments = {scene::MoveTo{{0, 0}},
                                   scene::LineTo{{1, 0}, scene::CoordinateMode::relative},
                                   scene::LineTo{{0.5, 1}}, scene::ClosePath{}};
        arrow.role = scene::ElementRole::enclosure_mark;
        arrow.syntax = scene::PathSyntax::spaced;
        arrow.transform = scene::Transform{std::vector<scene::TransformOperation>{
            scene::Translation{0.1, 0}, scene::Rotation{30, {0.5, 0.5}}}};
        builder.add_primitive(0, arrow);

        scene::Path zero_stroke;
        zero_stroke.geometry.segments = {scene::MoveTo{{0, 0}}};
        zero_stroke.role = scene::ElementRole::enclosure_mark;
        zero_stroke.paint_encoding = scene::PathPaintEncoding::stroke_width_style;
        builder.add_primitive(0, zero_stroke);

        scene::Rule rounded_fraction{
            {0.1, 0.2, 0.3, 0.4}, {}, {}, scene::ElementRole::fraction_rule};
        rounded_fraction.corner_radius = 0.15;
        builder.add_primitive(0, rounded_fraction);

        scene::Path relative_line;
        relative_line.element = scene::PathElement::line;
        relative_line.geometry.segments = {
            scene::MoveTo{{0.2, 0.3}}, scene::LineTo{{0.1, 0.2}, scene::CoordinateMode::relative}};
        relative_line.paint.stroke = scene::CssColor{"red"};
        builder.add_primitive(0, relative_line);

        const auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<rect x=\"100\" y=\"200\" width=\"467\" height=\"800\" "
                           "fill=\"none\" stroke-width=\"67\" rx=\"267\"/>") != output.npos);
        expect(output.find("<ellipse rx=\"400\" ry=\"200\" cx=\"500\" cy=\"-100\" "
                           "fill=\"#123456\" stroke=\"currentColor\" stroke-width=\"40\"/>") !=
               output.npos);
        expect(output.find("<path style=\"stroke-width: 67;\" d=\"M 0 0 L 1000 1000\" "
                           "stroke-linecap=\"round\" stroke-linejoin=\"mitre\" fill=\"none\"/>") !=
               output.npos);
        expect(output.find("<path style=\"stroke-width: 0;\" d=\"M0,0\"/>") != output.npos);
        expect(output.find("<rect width=\"300\" height=\"400\" x=\"100\" y=\"200\" "
                           "rx=\"150\"/>") != output.npos);
        expect(output.find("<line x1=\"200\" y1=\"300\" x2=\"300\" y2=\"500\" "
                           "stroke=\"red\"/>") != output.npos);
        expect(output.find("d=\"M 0 0 a 100 100 0 0,1 0 500 L 1000 500\"") != output.npos);
        expect(output.find("d=\"M 0 0 l 1000 0 L 500 1000 Z\" "
                           "transform=\"translate(100 0) rotate(30 500 500)\"/>") != output.npos);
    };
};
