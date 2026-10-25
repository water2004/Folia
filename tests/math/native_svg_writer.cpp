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
    "root preserves MathJax SVG attribute order and fixed dimensions"_test = [] {
        auto source = make_scene();
        auto output = svg::write(source, {.font_cache = svg::FontCacheMode::none});
        expect(output.starts_with(
            "<mjx-container class=\"MathJax\" jax=\"SVG\" overflow=\"linebreak\"><svg "
            "style=\"vertical-align: -0.444ex;\" xmlns=\"http://www.w3.org/2000/svg\" "
            "width=\"4.444ex\" height=\"2.222ex\" role=\"img\" focusable=\"false\" viewBox=\"0 "
            "-800 2000 1000\">"));
        expect(output.ends_with("</g></svg></mjx-container>"));
    };

    "XML text and attributes escape markup characters"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::SemanticMetadata metadata;
        metadata.source_tag = "mtext";
        metadata.ordered_attributes.push_back({"aria-label", "a&\"<b>"});
        metadata.ordered_attributes.push_back({"data-origin", "source"});
        auto group = builder.add_group(0, {}, {}, {}, {}, metadata);
        builder.add_primitive(group, scene::TextRun{"<&>"});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("aria-label=\"a&amp;&quot;&lt;b&gt;\"") != output.npos);
        expect(output.find("aria-label=") < output.find("data-origin=\"source\""));
        expect(output.find("><text transform=\"scale(1,-1)\">&lt;&amp;&gt;</text>") != output.npos);
    };

    "SVG attributes preserve long spans adjacent escapes empty and UTF8 values"_test = [] {
        const std::string plain(4096, 'x');
        const std::string utf8 = "\xCE\xB1\xF0\x9F\x98\x80";
        svg::SvgWriteOptions options;
        options.font_cache = svg::FontCacheMode::none;
        options.container_attributes = {{"data-long", "&\"<>" + plain + utf8 + "'&"},
                                        {"data-empty", ""}};
        const auto output = svg::write(make_scene(), options);
        expect(output.starts_with("<mjx-container data-long=\"&amp;&quot;&lt;&gt;" + plain +
                                  utf8 + "'&amp;\" data-empty=\"\">"));
        expect(options.container_attributes[0].value == "&\"<>" + plain + utf8 + "'&");
    };

    "SVG repeated long glyph paths keep local and uncached source bytes"_test = [] {
        std::string path = "M0 0";
        for (unsigned i = 0; i < 256; ++i) path += "L10 20L30 40";
        path += "Z";
        const auto resource = scene::GlyphResource::make("test", {});
        scene::Builder builder;
        builder.set_metrics({1, .5, .1});
        builder.set_font_metrics({1, .5, .5, .7});
        const scene::Glyph glyph{resource, {"normal", "NCM-N", 0x41}};
        builder.add_primitive(0, scene::GlyphRun{{glyph, glyph}});
        const auto source = std::move(builder).freeze();
        svg::SvgFontResourcesBuilder fonts;
        fonts.add({"normal", 0x41}, {true, path, {}});
        const auto catalog = std::move(fonts).freeze();
        const auto emptyCatalog = svg::SvgFontResources{};
        for (const auto mode : {svg::FontCacheMode::local, svg::FontCacheMode::none}) {
            svg::SvgWriteOptions options{.font_cache = mode, .font_resources = &catalog,
                                         .local_cache_prefixes = {"1-"}};
            const auto output = svg::write(source, options);
            const auto first = output.find(path);
            expect(first != output.npos);
            const auto second = output.find(path, first + path.size());
            expect((second != output.npos) == (mode == svg::FontCacheMode::none));
            expect(svg::write(source, options) == output);
            expect(catalog.find({"normal", 0x41})->path_data == path);
            options.font_resources = &emptyCatalog;
            expect(throws<svg::UnsupportedError>([&] { (void)svg::write(source, options); }));
        }
    };

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
                              scene::Link{"https://example.test/?a&b", "_blank", "noopener"},
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
            "<a href=\"https://example.test/?a&amp;b\" target=\"_blank\" rel=\"noopener\">");
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

    "typed text font serializes source font values and y reflection"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::TextRun run{"unknown", {0.2, 0.3}, {1, 0.5, 0.1}, {}, {}};
        run.font = scene::TextFont{"MathJax_AMS", 0.75, true, true, "normal"};
        run.metadata.math_role = "unknown";
        run.role = scene::ElementRole::unknown_text;
        builder.add_primitive(0, run);
        scene::TextRun empty_family{"fallback", {}, {1, 0.5, 0.1}, {}, {}};
        empty_family.font = scene::TextFont{"", 0.75, false, false, "normal"};
        empty_family.role = scene::ElementRole::unknown_text;
        builder.add_primitive(0, empty_family);
        scene::TextRun explicit_font{"explicit", {}, {1, 0.5, 0.1}, {}, {}};
        explicit_font.font = scene::TextFont{"serif", 0.75, true, true, "-explicitFont"};
        explicit_font.role = scene::ElementRole::unknown_text;
        builder.add_primitive(0, explicit_font);
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("data-variant=\"normal\" "
                           "transform=\"translate(200,300) scale(1,-1)\" "
                           "font-size=\"750px\" font-family=\"MathJax_AMS\" "
                           "font-style=\"italic\" font-weight=\"bold\"") != output.npos);
        expect(output.find("<text data-variant=\"normal\" transform=\"scale(1,-1)\" "
                           "font-size=\"750px\">fallback</text>") != output.npos);
        expect(output.find("<text data-variant=\"-explicitFont\" transform=\"scale(1,-1)\" "
                           "font-size=\"750px\">explicit</text>") != output.npos);
        expect(output.find("data-math-role=\"unknown\"") != output.npos);
    };

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
                                      "height=\"800\" y=\"-200\" fill=\"blue\" "
                                      "stroke=\"currentColor\"/>");
        auto title = output.find("<title>bad<&></title>");
        auto empty_title = output.find("<title/>");
        auto child = output.find("<text transform=\"scale(1,-1)\">child</text>");
        expect(retained_root_group < background);
        expect(background < title && title < empty_title && empty_title < child);
        expect(output.find("fill=\"yellow\"") == output.npos);
        expect(output.find("fill=\"red\"") == output.npos);
        expect(output.find("<title>legacy</title>") != output.npos);
        expect(output.find("tooltip child") != output.npos);
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
        (void)builder.add_group(0, {}, {}, {}, {}, table_metadata,
                                scene::ElementRole::table_content_viewport, {}, vertical);
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        auto vertical_start = output.find("<svg width=\"600\" height=\"800\" y=\"200\" x=\"100\"");
        expect(vertical_start != output.npos);
        expect(output.find("preserveAspectRatio=\"xMaxYMin slice\"", vertical_start) !=
               output.npos);
        expect(output.find("viewBox=\"-500 -400 1200 1300\"", vertical_start) != output.npos);
        expect(output.find("viewBox=\"-500 -400 1200 1300\" xmlns=\"http://www.w3.org/2000/svg\"",
                           vertical_start) != output.npos);
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
    };

    "glyph transforms merge after origin in one transform attribute"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        auto resource = scene::GlyphResource::make(
            "font", scene::GlyphOutline{scene::PathGeometry{
                        {scene::MoveTo{{0, 0}}, scene::LineTo{{1, 0}}, scene::ClosePath{}}}});
        scene::Glyph glyph{resource,
                           {"normal", "font", 65},
                           {0.1, 0.2},
                           0.5,
                           {},
                           scene::Transform{std::vector<scene::TransformOperation>{
                               scene::Scaling{1, 1, true}, scene::Translation{0.3, 0.4}}}};
        builder.add_primitive(0, scene::GlyphRun{{glyph}});
        svg::SvgFontResourcesBuilder fonts;
        fonts.add({"normal", 65}, {true, "M0 0L1000 0Z", {}});
        auto resources = std::move(fonts).freeze();
        auto output =
            svg::write(std::move(builder).freeze(),
                       {.font_cache = svg::FontCacheMode::none, .font_resources = &resources});
        auto glyph_start = output.find("<path data-c=\"41\"");
        auto transform = output.find(
            "transform=\"translate(100,200) scale(1,1) translate(300,400)\"", glyph_start);
        expect(glyph_start != output.npos && transform != output.npos);
        expect(output.find("transform=", transform + 1) == output.npos);
        expect(glyph.transform.matrix() ==
               scene::Transform{std::vector<scene::TransformOperation>{
                                    scene::Scaling{1, 1, false}, scene::Translation{0.3, 0.4}}}
                   .matrix());
    };

    "table frame and line attributes precede geometry without duplicating CSS paint"_test = [] {
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
                           "height=\"700\" x=\"100\" y=\"-200\" stroke-thickness=\"90\"/>") !=
               output.npos);
        expect(output.find("<line data-line=\"v\" class=\"MJX-TB-dotted\" x1=\"300\" "
                           "y1=\"-100\" x2=\"300\" y2=\"600\"/>") != output.npos);
        expect(output.find("<line data-line=\"h\" class=\"MJX-TB-dotted\" x1=\"0\" "
                           "y1=\"400\" x2=\"1000\" y2=\"400\" stroke-thickness=\"90\" "
                           "stroke-dasharray=\"0,140\"/>") != output.npos);
        auto frame = output.find("<rect data-frame=");
        auto line = output.find("<line data-line=");
        expect(frame != output.npos && line != output.npos && frame < line);
        auto frame_end = output.find("/>", frame);
        auto line_end = output.find("/>", line);
        expect(output.substr(frame, frame_end - frame).find("fill=") == std::string::npos);
        expect(output.substr(line, line_end - line).find("stroke=") == std::string::npos);
        expect(output.substr(line, line_end - line).find("stroke-dasharray=") ==
               std::string::npos);
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
                   "role=\"img\" focusable=\"false\" data-mjx-viewBox=\"0 -800 2000 1000\">") !=
               output.npos);
        expect(output.find(" viewBox=", output.find("<svg")) == output.npos);
        expect(output.find("min-width:") == output.npos);
        expect(output.find("<g stroke=\"currentColor\" fill=\"currentColor\" stroke-width=\"0\" "
                           "transform=\"scale(0.000226,-0.000226) translate(0, -800)\">") !=
               output.npos);
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

    "inline frames keep the Source local font cache only in the first SVG"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        auto resource = scene::GlyphResource::make(
            "NewCM-Roman", scene::GlyphOutline{scene::PathGeometry{
                               {scene::MoveTo{{0, 0}}, scene::LineTo{{0.5, 0}},
                                scene::LineTo{{0.5, 0.7}}, scene::ClosePath{}}}});
        auto first = builder.add_group();
        auto second = builder.add_group();
        scene::Glyph glyph{resource, {"normal", "NCM-N", 0x5F}, {}, 0.6, {}};
        builder.add_primitive(first, scene::GlyphRun{{glyph}});
        builder.add_primitive(second, scene::GlyphRun{{glyph}});
        builder.set_frame_layout({0.001,
                                  {{first, {0.5, 0.4, 0.1}, {}},
                                   {second,
                                    {0.5, 0.4, 0.1},
                                    scene::InlineBreak{scene::InlineBreakKind::opportunity, 0}}}});
        svg::SvgFontResourcesBuilder font_builder;
        font_builder.add({"normal", 0x5F}, {true, "M0-140L333-140L333-100L0-100Z", {}});
        auto font_resources = std::move(font_builder).freeze();
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::local,
                                                     .font_resources = &font_resources,
                                                     .local_cache_prefixes = {"1-"}});
        auto const first_svg = output.find("<svg");
        auto const second_svg = output.find("<svg", first_svg + 4);
        auto const cache = output.find("<defs><path id=\"MJX-1-NCM-N-5F\"");
        auto const first_group = output.find("<g stroke=\"currentColor\"", first_svg);
        auto const first_use = output.find("<use", first_svg);
        auto const first_svg_end = output.find("</svg>", first_svg);
        expect(cache != output.npos && first_svg < first_group && first_group < first_use &&
               first_use < cache && cache < first_svg_end);
        expect(output.find("<defs>", cache + 1) == output.npos);
        expect(output.find("<defs>", second_svg) == output.npos);
        expect(output.find("<use", second_svg) != output.npos);
    };

    "pinned SVG font paths serialize from exact variant keys and cache ids"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        auto resource = scene::GlyphResource::make(
            "NewCM-Roman", scene::GlyphOutline{scene::PathGeometry{
                               {scene::MoveTo{{0, 0}}, scene::LineTo{{0.5, 0}},
                                scene::LineTo{{0.5, 0.7}}, scene::ClosePath{}}}});
        builder.add_primitive(
            0, scene::GlyphRun{
                   {scene::Glyph{resource, {"normal", "NCM-N", 0x5F}, {0.125, 0.25}, 0.6, {}}}});
        auto source = std::move(builder).freeze();
        svg::SvgFontResourcesBuilder font_builder;
        font_builder.add({"normal", 0x5F}, {true, "M0-140L333-140L333-100L0-100Z", {}});
        auto font_resources = std::move(font_builder).freeze();
        auto output = svg::write(source, {.font_cache = svg::FontCacheMode::local,
                                          .font_resources = &font_resources,
                                          .local_cache_prefixes = {"1-"}});
        expect(
            output.find(
                "<defs><path id=\"MJX-1-NCM-N-5F\" d=\"M0-140L333-140L333-100L0-100Z\"/></defs>") !=
            output.npos);
        auto const svg_start = output.find("<svg");
        auto const cache = output.find("<defs>", svg_start);
        auto const root_group = output.find("<g stroke=\"currentColor\"", svg_start);
        expect(svg_start < cache && cache < root_group);
        expect(output.find("<use data-c=\"5F\" xlink:href=\"#MJX-1-NCM-N-5F\" "
                           "transform=\"translate(125,250)\"") != output.npos);
        auto uncached = svg::write(
            source, {.font_cache = svg::FontCacheMode::none, .font_resources = &font_resources});
        expect(uncached.find("<path data-c=\"5F\" d=\"M0-140L333-140L333-100L0-100Z\"") !=
               uncached.npos);
    };

    "defined empty Source paths stay empty and missing catalog keys fail"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        auto resource = scene::GlyphResource::make("NewCM-Roman", scene::GlyphOutline{});
        builder.add_primitive(
            0, scene::GlyphRun{{scene::Glyph{resource, {"normal", "NCM-N", 0x20}, {}, 0.332, {}}}});
        auto source = std::move(builder).freeze();
        svg::SvgFontResourcesBuilder font_builder;
        font_builder.add({"normal", 0x20}, {true, "", {}});
        auto font_resources = std::move(font_builder).freeze();
        auto empty_path = svg::write(
            source, {.font_cache = svg::FontCacheMode::none, .font_resources = &font_resources});
        expect(empty_path.find("<path data-c=\"20\" d=\"\"/>") != empty_path.npos);

        bool missing_failed = false;
        try {
            (void)svg::write(source, {.font_cache = svg::FontCacheMode::none});
        } catch (svg::UnsupportedError const &) {
            missing_failed = true;
        }
        expect(missing_failed);

        svg::SvgFontResourcesBuilder absent_path_builder;
        absent_path_builder.add({"normal", 0x20}, {false, "", {}});
        auto absent_path = std::move(absent_path_builder).freeze();
        bool absent_path_failed = false;
        try {
            (void)svg::write(
                source, {.font_cache = svg::FontCacheMode::none, .font_resources = &absent_path});
        } catch (svg::UnsupportedError const &) {
            absent_path_failed = true;
        }
        expect(absent_path_failed);
    };

    "owned HTML materialization follows Lite XML serialization rules"_test = [] {
        scene::HtmlNodePrimitive root;
        root.kind = u"div";
        root.namespaceHint = u"http://www.w3.org/1999/xhtml";
        root.attributes.push_back({u"title", u"a\"<&"});
        root.children.push_back({u"#text", {}, {}, u"before < &", {}});
        root.children.push_back({u"#comment", {}, {}, u"<!--kept-->", {}});
        root.children.push_back({u"br", u"http://www.w3.org/1999/xhtml", {}, {}, {}});
        root.children.push_back({u"span", {}, {}, {}, {}});
        root.children.push_back({u"script", {}, {}, {}, {{u"#text", {}, {}, u"x < y && z", {}}}});
        root.children.push_back({u"svg:svg",
                                 u"http://www.w3.org/2000/svg",
                                 {{u"xmlns:svg", u"http://www.w3.org/2000/svg"}},
                                 {},
                                 {{u"svg:rect",
                                   u"http://www.w3.org/2000/svg",
                                   {{u"xmlns:svg", u"http://www.w3.org/2000/svg"}},
                                   {},
                                   {}}}});

        auto serialized = svg::serializeHtml(root);
        expect(serialized ==
               "<div title=\"a&quot;&lt;&amp;\">before &lt; &amp;<!--kept--><br/><span/>"
               "<script>x < y && z</script><svg:svg xmlns:svg=\"http://www.w3.org/2000/svg\">"
               "<svg:rect xmlns:svg=\"http://www.w3.org/2000/svg\"/></svg:svg></div>");
    };

    "foreignObject uses CSS pixel projection and ordered owned HTML"_test = [] {
        scene::Builder builder;
        builder.set_metrics({2, 0.8, 0.2});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::HtmlNodePrimitive content{u"div",
                                         u"http://www.w3.org/1999/xhtml",
                                         {},
                                         {},
                                         {{u"#text", {}, {}, u"foreign<&>", {}}}};
        scene::ForeignContent foreign{
            std::move(content), {8.1234, 4.5678, 24.9876, 12.3456, 0.062345}, {}};
        auto const group =
            builder.add_group(0,
                              scene::Transform{std::vector<scene::TransformOperation>{
                                  scene::Translation{0.25, -0.5}}},
                              {}, {}, {}, {}, scene::ElementRole::foreign_content_viewport, {}, {},
                              std::move(foreign));
        (void)group;
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<foreignObject data-mjx-xml=\"true\" x=\"8.123px\" y=\"4.568px\" "
                           "width=\"24.988px\" height=\"12.346px\" transform=\"translate(250 -500) "
                           "scale(62.345) matrix(1 0 0 -1 0 0)\"><div>foreign&lt;&amp;&gt;</div>"
                           "</foreignObject>") != output.npos);

        scene::Builder boundary_builder;
        boundary_builder.set_metrics({1, 0.5, 0.1});
        boundary_builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::HtmlNodePrimitive empty{u"div", u"http://www.w3.org/1999/xhtml", {}, {}, {}};
        scene::ForeignContent boundary{
            std::move(empty), {-0.00055, 0.00055, 0.00055, 0.00155, 0.00000055}, {}};
        (void)boundary_builder.add_group(0, {}, {}, {}, {}, {},
                                         scene::ElementRole::foreign_content_viewport, {}, {},
                                         std::move(boundary));
        auto boundary_output = svg::write(std::move(boundary_builder).freeze(),
                                          {.font_cache = svg::FontCacheMode::none});
        expect(boundary_output.find("<foreignObject data-mjx-xml=\"true\" x=\"0px\" y=\"0px\" "
                                    "width=\"0px\" height=\"0.002px\" transform=\"scale(0) "
                                    "matrix(1 0 0 -1 0 0)\"><div/></foreignObject>") !=
               boundary_output.npos);
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

    "SVG fixed numbers use ECMAScript tie rounding"_test = [] {
        scene::Builder builder;
        builder.set_metrics({0.00125, 0.00125, 0});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("viewBox=\"0 -1.3 1.3 1.3\"") != output.npos);
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

    "SVG selects semantic paths and preserves first use of shared cache ids"_test = [] {
        constexpr std::string_view first_path = "M0 0L10 10Z";
        constexpr std::string_view second_path = "M20 20L30 30Z";
        constexpr std::string_view first_variant = "variant-a";
        constexpr std::string_view second_variant = "variant-b";
        auto resource = scene::GlyphResource::make("synthetic", scene::GlyphOutline{});
        auto make_scene = [&](std::string first, std::string second) {
            scene::Builder builder;
            builder.set_metrics({1, 0.5, 0.1});
            builder.set_font_metrics({1, 0.5, 0.5, 0.7});
            builder.add_primitive(
                0, scene::GlyphRun{
                       {scene::Glyph{resource, {std::move(first), "shared", 0x21}, {}, 0.5, {}},
                        scene::Glyph{
                            resource, {std::move(second), "shared", 0x21}, {0.5, 0}, 0.5, {}}}});
            return std::move(builder).freeze();
        };
        svg::SvgFontResourcesBuilder catalog_builder;
        catalog_builder.add({std::string(first_variant), 0x21},
                            {true, std::string(first_path), {}});
        catalog_builder.add({std::string(second_variant), 0x21},
                            {true, std::string(second_path), {}});
        auto catalog = std::move(catalog_builder).freeze();
        auto write_none = [&](std::string first, std::string second) {
            return svg::write(make_scene(std::move(first), std::move(second)),
                              {.font_cache = svg::FontCacheMode::none, .font_resources = &catalog});
        };
        auto none_ab = write_none(std::string(first_variant), std::string(second_variant));
        auto none_ba = write_none(std::string(second_variant), std::string(first_variant));
        auto first_ab = none_ab.find(first_path), second_ab = none_ab.find(second_path);
        auto first_ba = none_ba.find(first_path), second_ba = none_ba.find(second_path);
        expect(first_ab != none_ab.npos && second_ab != none_ab.npos && first_ab < second_ab);
        expect(first_ba != none_ba.npos && second_ba != none_ba.npos && second_ba < first_ba);

        auto check_local = [&](std::string const &output, std::string_view winner,
                               std::string_view loser) {
            expect(output.find("<defs><path id=\"MJX-1-shared-21\" d=\"" + std::string(winner) +
                               "\"/></defs>") != output.npos);
            expect(output.find(loser) == output.npos);
            auto use = output.find("<use data-c=\"21\" xlink:href=\"#MJX-1-shared-21\"");
            expect(use != output.npos);
            auto second_use =
                output.find("<use data-c=\"21\" xlink:href=\"#MJX-1-shared-21\"", use + 1);
            expect(second_use != output.npos);
            if (second_use != output.npos)
                expect(output.find("<use data-c=\"21\" xlink:href=\"#MJX-1-shared-21\"",
                                   second_use + 1) == output.npos);
        };
        auto local_ab =
            svg::write(make_scene(std::string(first_variant), std::string(second_variant)),
                       {.font_cache = svg::FontCacheMode::local,
                        .font_resources = &catalog,
                        .local_cache_prefixes = {"1-"}});
        auto local_ba =
            svg::write(make_scene(std::string(second_variant), std::string(first_variant)),
                       {.font_cache = svg::FontCacheMode::local,
                        .font_resources = &catalog,
                        .local_cache_prefixes = {"1-"}});
        check_local(local_ab, first_path, second_path);
        check_local(local_ba, second_path, first_path);
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
