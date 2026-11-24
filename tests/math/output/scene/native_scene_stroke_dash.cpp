#include "support/folia_test.hpp"
import folia.math.output.scene.builder;
import folia.math.output.svg.font_data;
import folia.math.output.svg.writer;

namespace {
using namespace boost::ut;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

std::string write_primitive(scene::Primitive primitive,
    svg::SvgFontResources const* fonts = nullptr) {
    scene::Builder builder;
    builder.set_metrics({2, 0.8, 0.2});
    builder.set_font_metrics({1, 0.5, 0.45, 0.7});
    builder.add_primitive(0, std::move(primitive));
    svg::SvgWriteOptions options;
    options.font_cache = svg::FontCacheMode::none;
    options.font_resources = fonts;
    return svg::write(std::move(builder).freeze(), options);
}

scene::Paint stroke_paint(std::optional<std::vector<scene::Unit>> dash) {
    scene::Paint paint;
    paint.stroke = scene::CurrentColorPaint{};
    paint.stroke_width = 0.1;
    paint.dash_array = std::move(dash);
    return paint;
}
}

suite native_scene_stroke_dash = [] {
    "absent dash inherits while an empty dash array means solid"_test = [] {
        auto inherited = write_primitive(scene::Rule{{0, 0, 1, 0.1},
            stroke_paint(std::nullopt)});
        expect(inherited.find("stroke-width=\"100\"") != inherited.npos);
        expect(inherited.find("stroke-dasharray=") == inherited.npos);

        auto solid = write_primitive(scene::Path{
            scene::PathGeometry{{scene::MoveTo{{0, 0}}, scene::LineTo{{1, 0}}}},
            stroke_paint(std::vector<scene::Unit>{})});
        expect(solid.find("stroke-dasharray=\"none\"") != solid.npos);
    };

    "Source dotted and dashed em arrays serialize on rule and path paints"_test = [] {
        // Pinned MathJax 4.1.3 SvgWrapper.addBorderBroken direct observations:
        // t=.1, horizontal length 1 gives dotted [1,.178] and dashed [.1,.3]
        // after converting Source's 1000-unit SVG strings back to Scene em.
        auto dotted = write_primitive(scene::Rule{{0, 0, 1, 0.1},
            scene::Paint{.stroke = scene::CurrentColorPaint{}, .stroke_width = 0.1,
                .line_cap = scene::StrokeCap::round,
                .dash_array = std::vector<scene::Unit>{0.001, 0.178}}});
        expect(dotted.find("stroke-linecap=\"round\" stroke-dasharray=\"1 178\"") != dotted.npos);

        auto dashed = write_primitive(scene::Path{
            scene::PathGeometry{{scene::MoveTo{{0, 0}}, scene::LineTo{{1, 0}}}},
            scene::Paint{.stroke = scene::CurrentColorPaint{}, .stroke_width = 0.1,
                .line_cap = scene::StrokeCap::square,
                .dash_array = std::vector<scene::Unit>{0.1, 0.3}}});
        expect(dashed.find("stroke-linecap=\"square\" stroke-dasharray=\"100 300\"") != dashed.npos);
    };

    "glyph paint retains dash values and owned copies across builder freeze"_test = [] {
        constexpr std::string_view variant = "normal";
        constexpr std::uint32_t codepoint = 0x78;
        auto resource = scene::GlyphResource::make("NewCM", scene::GlyphOutline{});
        svg::SvgFontResourcesBuilder font_builder;
        font_builder.add({std::string(variant), codepoint}, {true, "M0,0L1,1", {}});
        auto fonts = std::move(font_builder).freeze();

        std::vector<scene::Unit> caller_values{0.1, 0.3};
        auto caller_paint = stroke_paint(caller_values);
        scene::Builder builder;
        builder.set_metrics({2, 0.8, 0.2});
        builder.set_font_metrics({1, 0.5, 0.45, 0.7});
        builder.add_primitive(0, scene::GlyphRun{{scene::Glyph{resource,
            {std::string(variant), std::string(variant), codepoint}, {}, 0.5,
            caller_paint}}});

        // Destroy/change every caller-owned vector/paint before freezing.
        caller_values[0] = 7;
        caller_paint.dash_array->at(1) = 9;
        caller_paint.dash_array.reset();
        auto frozen = std::move(builder).freeze();
        svg::SvgWriteOptions options;
        options.font_cache = svg::FontCacheMode::none;
        options.font_resources = &fonts;
        auto output = svg::write(frozen, options);
        expect(output.find("<path data-c=\"78\" d=\"M0,0L1,1\" stroke=\"currentColor\" stroke-width=\"100\" stroke-dasharray=\"100 300\"/>") != output.npos);
    };

    "finite dash numbers use SVG validation and keep Source short-span values"_test = [] {
        auto short_dotted = write_primitive(scene::Rule{{0, 0, 0, 0.1},
            scene::Paint{.stroke = scene::CurrentColorPaint{},
                .dash_array = std::vector<scene::Unit>{0.001, -0.001}}});
        expect(short_dotted.find("stroke-dasharray=\"1 -1\"") != short_dotted.npos);

        // Source handleBorder skips zero thickness. Calling its helper
        // directly with zero produces NaN; this is outside finite Scene paint.
        bool rejected{};
        try {
            (void)write_primitive(scene::Rule{{0, 0, 1, 0.1},
                scene::Paint{.stroke = scene::CurrentColorPaint{},
                    .dash_array = std::vector<scene::Unit>{0.001,
                        std::numeric_limits<scene::Unit>::quiet_NaN()}}});
        } catch (std::invalid_argument const&) { rejected = true; }
        expect(rejected);
    };
};
