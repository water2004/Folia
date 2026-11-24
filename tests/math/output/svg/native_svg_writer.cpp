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

suite native_svg_writer = [] {
    "standalone SVG viewport resolves font metrics and display scale in pixels"_test = [] {
        scene::Builder builder;
        builder.set_metrics({2, .8, .2});
        builder.set_font_metrics({16, 8, .4, .7});
        builder.set_display_layout({.display = true, .font_scale = 1.5});
        const auto source = std::move(builder).freeze();
        const auto metrics = svg::pixel_metrics(source);
        expect(metrics.width == 60.0 && metrics.height == 30.0 && metrics.baseline == 24.0);
        const auto output = svg::write(
            source, {.font_cache = svg::FontCacheMode::none, .pixel_viewport = true});
        expect(output.starts_with("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" "
                                  "height=\"30\" role=\"img\" focusable=\"false\" "
                                  "viewBox=\"0 -800 2000 1000\" overflow=\"visible\">"));
        expect(output.ends_with("</g></svg>"));
        expect(output.find("mjx-container") == output.npos);
    };

    "standalone SVG percentage frame retains its resolved viewport"_test = [] {
        scene::Builder builder;
        builder.set_metrics({4, .8, .2});
        builder.set_font_metrics({16, 8, .4, .7});
        builder.set_display_layout({.display = true, .font_scale = 1.5});
        scene::FrameLayout frame;
        frame.percentage = scene::PercentageFrame{50, "50%", {0, 0, .2, .02}, .02};
        builder.set_frame_layout(frame);
        const auto source = std::move(builder).freeze();
        const auto metrics = svg::pixel_metrics(source);
        expect(metrics.width == 300.0 && metrics.height == 30.0 && metrics.baseline == 24.0);
        const auto output = svg::write(
            source, {.font_cache = svg::FontCacheMode::none, .pixel_viewport = true});
        expect(output.find("width=\"300\" height=\"30\"") != output.npos);
        expect(output.find("viewBox=\"0 0 200 20\"") != output.npos);
        expect(output.find("transform=\"scale(0.02,-0.02) translate(0, -800)\"") != output.npos);
        expect(output.find("data-mjx-viewBox") == output.npos);
    };

    "standalone SVG scopes generated clip references without changing external links"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, .5, .1});
        builder.set_font_metrics({16, 8, .5, .7});
        const auto group = builder.add_group(
            0, {}, scene::Clip{scene::PathGeometry{{scene::MoveTo{{0, 0}},
                                                    scene::LineTo{{1, 1}}}}},
            scene::Link{"https://example.test/#figure"});
        const auto source = std::move(builder).freeze();
        for (const auto prefix : {"first-", "second-"}) {
            svg::SvgWriteOptions options;
            options.font_cache = svg::FontCacheMode::none;
            options.pixel_viewport = true;
            options.id_prefix = prefix;
            const auto output = svg::write(source, options);
            const auto id = std::string(prefix) + "clip-" + std::to_string(group);
            expect(output.find("clip-path=\"url(#" + id + ")\"") != output.npos);
            expect(output.find("<clipPath id=\"" + id + "\"") != output.npos);
            expect(output.find("href=\"https://example.test/#figure\"") != output.npos);
        }
    };

    "root preserves MathJax SVG attribute order and fixed dimensions"_test = [] {
        auto source = make_scene();
        auto output = svg::write(source, {.font_cache = svg::FontCacheMode::none});
        expect(output.starts_with(
            "<mjx-container class=\"MathJax\" jax=\"SVG\" overflow=\"linebreak\"><svg "
            "style=\"vertical-align: -0.444ex;\" xmlns=\"http://www.w3.org/2000/svg\" "
            "width=\"4.444ex\" height=\"2.222ex\" role=\"img\" focusable=\"false\" viewBox=\"0 "
            "-800 2000 1000\" overflow=\"visible\">"));
        expect(output.ends_with("</g></svg></mjx-container>"));
    };

    "display container values follow the default Source attribute order"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        builder.set_display_layout({true, "right", 0, 0, true});
        auto output =
            svg::write(std::move(builder).freeze(),
                       {.font_cache = svg::FontCacheMode::none,
                        .container_attributes = {
                            {"class", "MathJax"}, {"jax", "SVG"}, {"overflow", "linebreak"}}});
        expect(output.starts_with(
            "<mjx-container class=\"MathJax\" jax=\"SVG\" overflow=\"linebreak\" display=\"true\" "
            "width=\"full\" justify=\"right\"><svg"));
    };

    "scaled display container merges generated font size and minimum width"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        builder.set_display_layout({true, "right", 0, 1.25, true, 2});
        auto output =
            svg::write(std::move(builder).freeze(),
                       {.font_cache = svg::FontCacheMode::none,
                        .container_attributes = {{"class", "MathJax"},
                                                 {"style", "color: red; font-size: 150%;"}}});
        expect(output.starts_with(
            "<mjx-container class=\"MathJax\" style=\"color: red; font-size: 200%; min-width: "
            "2.5ex;\" display=\"true\" width=\"full\" justify=\"right\"><svg"));
    };

    "typed viewport roles preserve Source attribute order and nesting"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::Viewport vertical{{0.1, 0.2, 0.6, 0.8},
                                 {-0.5, -0.4, 1.2, 1.3},
                                 scene::ViewportAspectRatio{scene::ViewportAlignment::max,
                                                            scene::ViewportAlignment::min,
                                                            scene::ViewportFit::slice}};
        auto vertical_id = builder.add_group(
            0, {}, {}, {}, {}, {}, scene::ElementRole::vertical_extender_viewport, {}, vertical);
        builder.add_primitive(
            vertical_id,
            scene::Path{scene::PathGeometry{{scene::MoveTo{{0, 0}}, scene::LineTo{{0.2, 0.3}}}}});
        scene::Viewport horizontal{{0.3, 0.4, 0.7, 0.9},
                                   {-0.1, -0.2, 1.1, 1.2},
                                   scene::ViewportAspectRatio{scene::ViewportAlignment::min,
                                                              scene::ViewportAlignment::mid,
                                                              scene::ViewportFit::meet}};
        auto horizontal_id =
            builder.add_group(0, {}, {}, {}, {}, {},
                              scene::ElementRole::horizontal_extender_viewport, {}, horizontal);
        builder.add_primitive(horizontal_id,
                              scene::Path{scene::PathGeometry{{scene::MoveTo{{0, 0}}}}});
        scene::SemanticMetadata table_metadata;
        table_metadata.ordered_attributes.push_back({"data-table", "true"});
        auto table_viewport = vertical;
        table_viewport.clip_content = false;
        (void)builder.add_group(0, {}, {}, {}, {}, table_metadata,
                                scene::ElementRole::table_content_viewport, {}, table_viewport);
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        auto vertical_start = output.find("<svg width=\"600\" height=\"800\" y=\"200\" x=\"100\"");
        expect(vertical_start != output.npos);
        expect(output.find("preserveAspectRatio=\"xMaxYMin slice\"", vertical_start) !=
               output.npos);
        expect(output.find("viewBox=\"-500 -400 1200 1300\"", vertical_start) != output.npos);
        expect(output.find("viewBox=\"-500 -400 1200 1300\" xmlns=\"http://www.w3.org/2000/svg\"",
                           vertical_start) != output.npos);
        const auto vertical_tag = output.substr(vertical_start, output.find('>', vertical_start) - vertical_start);
        expect(vertical_tag.find("overflow=") == vertical_tag.npos)
            << "stretch extenders keep their nested SVG clipping";
        expect(output.find("<svg", vertical_start) < output.find("<path", vertical_start));
        expect(output.find("<g", vertical_start) == output.npos);
        expect(output.find("<svg width=\"700\" height=\"900\" x=\"300\" y=\"400\"") != output.npos);
        expect(output.find("preserveAspectRatio=\"xMinYMid\"") != output.npos);
        expect(output.find("preserveAspectRatio=\"xMinYMid meet\"") == output.npos);
        auto table_start = output.find("<svg data-table=\"true\"");
        expect(table_start != output.npos);
        auto table_end = output.find('>', table_start);
        auto table_tag = output.substr(table_start, table_end - table_start);
        expect(table_tag.find("width=") == table_tag.npos &&
               table_tag.find("height=") == table_tag.npos &&
               table_tag.find(" x=") == table_tag.npos && table_tag.find(" y=") == table_tag.npos);
        expect(table_tag.find("preserveAspectRatio=\"xMaxYMin slice\"") != table_tag.npos);
        expect(table_tag.find("viewBox=\"-500 -400 1200 1300\"") != table_tag.npos);
        expect(table_tag.find("overflow=\"visible\"") != table_tag.npos);
    };

    "percentage frames keep Source SVG attributes and scaled root transform"_test = [] {
        scene::Builder builder;
        builder.set_metrics({2, 0.8, 0.2});
        builder.set_font_metrics({1, 0.5, 0.45, 0.7});
        builder.set_frame_layout(
            {0.001, {}, scene::PercentageFrame{80, "80%", {4.5, 0, 2.2, 0.8}, 0.000226}});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find(
                   "<svg style=\"vertical-align: -0.444ex;\" "
                   "xmlns=\"http://www.w3.org/2000/svg\" width=\"80%\" height=\"2.222ex\" "
                   "role=\"img\" focusable=\"false\" data-mjx-viewBox=\"0 -800 2000 1000\" "
                   "overflow=\"visible\">") !=
               output.npos);
        expect(output.find(" viewBox=", output.find("<svg")) == output.npos);
        expect(output.find("min-width:") == output.npos);
        expect(output.find("<g stroke=\"currentColor\" fill=\"currentColor\" stroke-width=\"0\" "
                           "transform=\"scale(0.000226,-0.000226) translate(0, -800)\">") !=
               output.npos);
    };

    "inline frame metrics and Source break nodes serialize without relayout"_test = [] {
        scene::Builder builder;
        builder.set_metrics({0.8, 0.6, 0.2});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        auto first = builder.add_group();
        builder.add_primitive(first, scene::TextRun{"one", {}, {}, {}, {}});
        auto second = builder.add_group();
        builder.add_primitive(second, scene::TextRun{"two", {}, {}, {}, {}});
        auto third = builder.add_group();
        builder.add_primitive(third, scene::TextRun{"three", {}, {}, {}, {}});
        builder.set_frame_layout(
            {0.05,
             {{first, {0, 0, 0}, {}},
              {second,
               {0.2, 0.1, 0.05},
               scene::InlineBreak{scene::InlineBreakKind::opportunity, 0}},
              {third,
               {0.3, 0.2, 0.1},
               scene::InlineBreak{scene::InlineBreakKind::forced, 4.0 / 18.0}}}});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.starts_with("<mjx-container"));
        expect(output.find("<mjx-container", 1) == output.npos);
        expect(output.ends_with("</mjx-container>"));
        auto first_svg = output.find("<svg");
        auto second_svg = output.find("<svg", first_svg + 4);
        auto third_svg = output.find("<svg", second_svg + 4);
        expect(first_svg < second_svg && second_svg < third_svg);
        expect(output.find("width=\"0.1ex\" height=\"0.1ex\"", first_svg) != output.npos);
        auto opportunity = output.find(
            "<mjx-break prebreak=\"true\"> </mjx-break><mjx-break newline=\"true\"> </mjx-break>");
        auto forced = output.find("<mjx-break size=\"3\"> </mjx-break>");
        expect(first_svg < opportunity && opportunity < second_svg);
        expect(second_svg < forced && forced < third_svg);
        expect(output.find("one") < output.find("two") &&
               output.find("two") < output.find("three"));
    };

    "forced inline break uses Source letter spacing fallback"_test = [] {
        scene::Builder builder;
        builder.set_metrics({0.8, 0.6, 0.2});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        auto first = builder.add_group();
        auto second = builder.add_group();
        builder.set_frame_layout({0.001,
                                  {{first, {0.4, 0.2, 0.1}, {}},
                                   {second,
                                    {0.4, 0.2, 0.1},
                                    scene::InlineBreak{scene::InlineBreakKind::forced, 0.41}}}});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<mjx-break style=\"letter-spacing: -0.59em\"> </mjx-break>") !=
               output.npos);
    };

    "SVG rejects zero x-height before converting dimensions to ex"_test = [] {
        scene::Builder bad_metrics;
        bad_metrics.set_metrics({1, 0.5, 0.1});
        bad_metrics.set_font_metrics({1, 0.5, 0, 0.7});
        bool metric_error = false;
        try {
            (void)svg::write(std::move(bad_metrics).freeze());
        } catch (std::invalid_argument const &) {
            metric_error = true;
        }
        expect(metric_error);
    };


};
