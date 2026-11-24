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

suite native_svg_writer_paint = [] {
    "error background title and children retain ordered typed SVG values"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::Paint root_paint;
        root_paint.stylesheet = scene::PaintColors{scene::CssColor{"red"}, scene::CssColor{"red"}};
        builder.set_paint(0, root_paint);

        scene::Paint authored;
        authored.fill = scene::CssColor{"blue"};
        authored.stroke = scene::CurrentColorPaint{};
        authored.stylesheet = scene::PaintColors{scene::CssColor{"yellow"}, scene::NoPaint{}};
        builder.add_primitive(
            0, scene::Rule{{0, -0.2, 1, 0.8}, authored, {}, scene::ElementRole::error_background});
        // LiteParser.PCDATA includes title; serializeInner copies PCDATA verbatim.
        builder.add_primitive(0, scene::Title{"bad<&>"});
        builder.add_primitive(0, scene::Title{""});
        builder.add_primitive(0, scene::TextRun{"child", {}, {}, {}, {}});
        auto legacy = builder.add_group(0, {}, {}, {}, scene::Tooltip{"legacy"});
        builder.add_primitive(legacy, scene::TextRun{"tooltip child", {}, {}, {}, {}});

        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        auto retained_root_group = output.find("<g");
        auto background = output.find("<rect data-background=\"true\" width=\"1000\" "
                                      "height=\"800\" y=\"-200\" fill=\"yellow\" "
                                      "stroke=\"none\"/>");
        auto title = output.find("<title>bad<&></title>");
        auto empty_title = output.find("<title/>");
        auto child = output.find("<text transform=\"scale(1,-1)\">child</text>");
        expect(retained_root_group < background);
        expect(background < title && title < empty_title && empty_title < child);
        expect(output.find("<g fill=\"red\" stroke=\"red\">") != output.npos);
        expect(output.find("fill=\"blue\"") == output.npos);
        expect(output.find("<title>legacy</title>") != output.npos);
        expect(output.find("tooltip child") != output.npos);
    };

    "stylesheet paint preserves authored inline style and inherited color components"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, .5, .1});
        builder.set_font_metrics({1, .5, .5, .7});
        scene::Paint paint;
        paint.fill = scene::CssColor{"blue"};
        paint.stroke = scene::CssColor{"purple"};
        paint.stylesheet = scene::PaintColors{scene::CssColor{"red"}, scene::InheritedPaint{}};
        scene::SemanticMetadata metadata;
        metadata.ordered_attributes = {{"style", "fill: green; stroke: orange;"}};
        builder.add_group(0, {}, {}, {}, {}, metadata, scene::ElementRole::math_wrapper, paint);
        const auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<g style=\"fill: green; stroke: orange;\" fill=\"red\" "
                           "stroke=\"purple\"/>") != output.npos)
            << "inline style keeps CSS priority over the materialized presentation colors";
    };

    "fraction and radical rule rectangles retain Source width height x y order"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        builder.add_primitive(
            0, scene::Rule{{0.25, 0.2, 0.8, 0.04}, {}, {}, scene::ElementRole::fraction_rule});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<rect width=\"800\" height=\"40\" x=\"250\" y=\"200\"") != output.npos);
    };

    "background rectangle and border polygon preserve Source paint order"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::Paint background;
        background.fill = scene::CssColor{"black"};
        builder.add_primitive(
            0, scene::Rule{{0, 0, 1, 0.4}, background, {}, scene::ElementRole::background_box});
        scene::Path border;
        border.element = scene::PathElement::polygon;
        border.role = scene::ElementRole::background_box;
        border.geometry.segments = {scene::MoveTo{{0, 0}}, scene::LineTo{{1, 0}},
                                    scene::LineTo{{1, 1}}, scene::ClosePath{}};
        border.paint.stroke = scene::NoPaint{};
        border.paint.fill = scene::CssColor{"blue"};
        builder.add_primitive(0, std::move(border));
        const auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(
            output.find("<rect fill=\"black\" x=\"0\" y=\"0\" width=\"1000\" height=\"400\"/>") !=
            output.npos);
        expect(output.find(
                   "<polygon points=\"0,0 1000,0 1000,1000\" stroke=\"none\" fill=\"blue\"/>") !=
               output.npos);
    };

    "dashed border lines preserve empty Source stroke and attribute order"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::Path border;
        border.element = scene::PathElement::line;
        border.role = scene::ElementRole::background_box;
        border.geometry.segments = {scene::MoveTo{{0.1, 0.2}}, scene::LineTo{{0.7, 0.2}}};
        border.paint.stroke = scene::CssColor{""};
        border.paint.stroke_width = 0.07;
        border.paint.line_cap = scene::StrokeCap::square;
        border.paint.dash_array = std::vector<double>{0.2, 0.6};
        border.metadata.ordered_attributes = {{"data-border", "row"}, {"width", "stale"}};
        border.layout_width = 3.049;
        builder.add_primitive(0, std::move(border));
        auto output = svg::write(std::move(builder).freeze(),
                                 {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<line x1=\"100\" y1=\"200\" x2=\"700\" y2=\"200\" "
                           "stroke-width=\"70\" stroke=\"\" stroke-linecap=\"square\" "
                           "stroke-dasharray=\"200 600\" data-border=\"row\" "
                           "width=\"3049\"/>") != output.npos);
    };

    "enclosure line marks serialize geometry and width while inheriting paint"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::Path mark;
        mark.element = scene::PathElement::line;
        mark.role = scene::ElementRole::enclosure_mark;
        mark.geometry.segments = {scene::MoveTo{{0.25, -0.1}}, scene::LineTo{{0.75, 0.3}}};
        mark.paint.stroke_width = 0.08;
        builder.add_primitive(0, std::move(mark));
        scene::Path explicitly_painted_mark;
        explicitly_painted_mark.element = scene::PathElement::line;
        explicitly_painted_mark.role = scene::ElementRole::enclosure_mark;
        explicitly_painted_mark.geometry.segments = {
            scene::MoveTo{{0.1, 0.4}}, scene::LineTo{{0.9, 0.4}}};
        explicitly_painted_mark.paint.fill = scene::CssColor{"red"};
        explicitly_painted_mark.paint.stroke = scene::CssColor{"blue"};
        explicitly_painted_mark.paint.stroke_width = 0.08;
        builder.add_primitive(0, std::move(explicitly_painted_mark));
        auto output = svg::write(std::move(builder).freeze(),
                                 {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<line x1=\"250\" y1=\"-100\" x2=\"750\" y2=\"300\" "
                           "stroke-width=\"80\"/>") != output.npos);
        expect(output.find("<line x1=\"100\" y1=\"400\" x2=\"900\" y2=\"400\" "
                           "stroke-width=\"80\" fill=\"red\" stroke=\"blue\"/>") !=
               output.npos);
    };

    "table frame and lines preserve resolved paint without a document stylesheet"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::SemanticMetadata frame_metadata;
        frame_metadata.ordered_attributes = {{"data-frame", "true"}, {"class", "MJX-TB-frame"}};
        scene::Paint css_paint;
        css_paint.fill = scene::NoPaint{};
        css_paint.stroke = scene::CurrentColorPaint{};
        css_paint.stroke_width = 0.09;
        builder.add_primitive(0, scene::Rule{{0.1, -0.2, 1.3, 0.7},
                                             css_paint,
                                             frame_metadata,
                                             scene::ElementRole::table_frame});
        scene::SemanticMetadata line_metadata;
        line_metadata.ordered_attributes = {{"data-line", "v"}, {"class", "MJX-TB-dotted"}};
        scene::Paint dotted_paint;
        dotted_paint.fill = scene::NoPaint{};
        dotted_paint.stroke = scene::CurrentColorPaint{};
        dotted_paint.stroke_width = 0.07;
        dotted_paint.line_cap = scene::StrokeCap::round;
        dotted_paint.dash_array = std::vector<double>{0, 0.14};
        builder.add_primitive(
            0, scene::Path{
                   scene::PathGeometry{{scene::MoveTo{{0.3, -0.1}}, scene::LineTo{{0.3, 0.6}}}},
                   dotted_paint, line_metadata, scene::ElementRole::table_vertical_line,
                   scene::PathElement::line});
        scene::SemanticMetadata custom_line_metadata;
        custom_line_metadata.ordered_attributes = {{"data-line", "h"},
                                                   {"class", "MJX-TB-dotted"}};
        auto custom_line_paint = dotted_paint;
        custom_line_paint.stroke_width = 0.09;
        builder.add_primitive(
            0, scene::Path{
                   scene::PathGeometry{{scene::MoveTo{{0, 0.4}}, scene::LineTo{{1, 0.4}}}},
                   custom_line_paint, custom_line_metadata,
                   scene::ElementRole::table_horizontal_line, scene::PathElement::line});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<rect data-frame=\"true\" class=\"MJX-TB-frame\" width=\"1300\" "
                           "height=\"700\" x=\"100\" y=\"-200\" fill=\"none\" "
                           "stroke=\"currentColor\" stroke-width=\"90\"/>") !=
               output.npos);
        expect(output.find("<line data-line=\"v\" class=\"MJX-TB-dotted\" x1=\"300\" "
                           "y1=\"-100\" x2=\"300\" y2=\"600\" fill=\"none\" "
                           "stroke=\"currentColor\" stroke-width=\"70\" "
                           "stroke-linecap=\"round\" stroke-dasharray=\"0 140\"/>") != output.npos);
        expect(output.find("<line data-line=\"h\" class=\"MJX-TB-dotted\" x1=\"0\" "
                           "y1=\"400\" x2=\"1000\" y2=\"400\" fill=\"none\" "
                           "stroke=\"currentColor\" stroke-width=\"90\" "
                           "stroke-linecap=\"round\" stroke-dasharray=\"0 140\"/>") != output.npos);
        auto frame = output.find("<rect data-frame=");
        auto line = output.find("<line data-line=");
        expect(frame != output.npos && line != output.npos && frame < line);
        expect(output.find("stroke-thickness=") == output.npos);
        expect(output.find("<g stroke=\"currentColor\" fill=\"currentColor\" stroke-width=\"0\"") !=
               output.npos) << "glyph outlines retain the zero-stroke root";
    };

    "completed glyph and table zero widths suppress a surrounding nonzero stroke"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, .5, .1});
        builder.set_font_metrics({1, .5, .5, .7});
        scene::Paint surrounding;
        surrounding.stroke = scene::CurrentColorPaint{};
        surrounding.stroke_width = .1;
        const auto group = builder.add_group(0, {}, {}, {}, {}, {},
                                            scene::ElementRole::math_wrapper, surrounding);
        const auto resource = scene::GlyphResource::make("NewCM", scene::GlyphOutline{});
        builder.add_primitive(group, scene::GlyphRun{{scene::Glyph{
            resource, {"normal", "NCM-N", 0x78}, {}, .5, {}}}});
        scene::Paint rule_paint;
        rule_paint.fill = scene::NoPaint{};
        scene::SemanticMetadata line_metadata;
        line_metadata.ordered_attributes = {{"data-line", "h"}, {"class", "mjx-solid"}};
        builder.add_primitive(group, scene::Path{
            scene::PathGeometry{{scene::MoveTo{{0, 0}}, scene::LineTo{{1, 0}}}},
            rule_paint, line_metadata, scene::ElementRole::table_horizontal_line,
            scene::PathElement::line});
        scene::SemanticMetadata frame_metadata;
        frame_metadata.ordered_attributes = {{"data-frame", "true"}, {"class", "mjx-solid"}};
        builder.add_primitive(group, scene::Rule{{0, -.1, 1, .6}, rule_paint, frame_metadata,
                                                scene::ElementRole::table_frame});
        const auto source = std::move(builder).freeze();
        svg::SvgFontResourcesBuilder fonts;
        fonts.add({"normal", 0x78}, {true, "M0 0L500 500Z", {}});
        const auto resources = std::move(fonts).freeze();
        for (const auto cache : {svg::FontCacheMode::none, svg::FontCacheMode::local}) {
            const auto output = svg::write(source, {.font_cache = cache, .font_resources = &resources,
                                                    .local_cache_prefixes = {"1-"}});
            expect(output.find("<g stroke=\"currentColor\" stroke-width=\"100\">") != output.npos);
            const auto glyph = cache == svg::FontCacheMode::none ? "<path data-c=" : "<use data-c=";
            for (const auto opening : {glyph, "<line data-line=", "<rect data-frame="}) {
                const auto begin = output.find(opening);
                expect(begin != output.npos) << opening;
                if (begin == output.npos)
                    continue;
                const auto tag = output.substr(begin, output.find('>', begin) - begin);
                expect(tag.find("stroke-width=\"0\"") != tag.npos) << tag;
            }
        }
    };

    "paint preserves inherited none currentColor and explicit color"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::Paint inherited;
        scene::Paint none;
        none.fill = scene::NoPaint{};
        scene::Paint current;
        current.fill = scene::CurrentColorPaint{};
        scene::Paint explicit_color;
        explicit_color.fill = scene::Color{17, 34, 51, 255};
        builder.add_primitive(0,
                              scene::Path{scene::PathGeometry{{scene::MoveTo{{0, 0}}}}, inherited});
        builder.add_primitive(0, scene::Path{scene::PathGeometry{{scene::MoveTo{{0, 0}}}}, none});
        builder.add_primitive(0,
                              scene::Path{scene::PathGeometry{{scene::MoveTo{{0, 0}}}}, current});
        builder.add_primitive(
            0, scene::Path{scene::PathGeometry{{scene::MoveTo{{0, 0}}}}, explicit_color});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<path d=\"M0,0\"/>") != output.npos);
        expect(output.find("<path d=\"M0,0\" fill=\"none\"/>") != output.npos);
        expect(output.find("<path d=\"M0,0\" fill=\"currentColor\"/>") != output.npos);
        expect(output.find("<path d=\"M0,0\" fill=\"#112233\"/>") != output.npos);
    };

    "stroke cap and join preserve inherited and explicit closed values"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::Paint inherited;
        inherited.fill = scene::NoPaint{};
        auto butt_miter = inherited;
        butt_miter.line_cap = scene::StrokeCap::butt;
        butt_miter.line_join = scene::StrokeJoin::miter;
        auto round_round = inherited;
        round_round.line_cap = scene::StrokeCap::round;
        round_round.line_join = scene::StrokeJoin::round;
        auto square_bevel = inherited;
        square_bevel.line_cap = scene::StrokeCap::square;
        square_bevel.line_join = scene::StrokeJoin::bevel;
        builder.add_primitive(0,
                              scene::Path{scene::PathGeometry{{scene::MoveTo{{0, 0}}}}, inherited});
        builder.add_primitive(
            0, scene::Path{scene::PathGeometry{{scene::MoveTo{{1, 0}}}}, butt_miter});
        builder.add_primitive(
            0, scene::Path{scene::PathGeometry{{scene::MoveTo{{2, 0}}}}, round_round});
        builder.add_primitive(
            0, scene::Path{scene::PathGeometry{{scene::MoveTo{{3, 0}}}}, square_bevel});
        const auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<path d=\"M0,0\" fill=\"none\"/>") != output.npos);
        expect(output.find("<path d=\"M1000,0\" fill=\"none\" stroke-linecap=\"butt\" "
                           "stroke-linejoin=\"miter\"/>") != output.npos);
        expect(output.find("<path d=\"M2000,0\" fill=\"none\" stroke-linecap=\"round\" "
                           "stroke-linejoin=\"round\"/>") != output.npos);
        expect(output.find("<path d=\"M3000,0\" fill=\"none\" stroke-linecap=\"square\" "
                           "stroke-linejoin=\"bevel\"/>") != output.npos);
    };


};
