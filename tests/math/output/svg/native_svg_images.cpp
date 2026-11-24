#include "support/folia_test.hpp"

import folia.math.output.scene.builder;
import folia.math.output.svg.writer;

namespace {
using namespace boost::ut;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

suite native_svg_images = [] {
    "image projection preserves Source attribute order and transform spelling"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        scene::Transform transform(std::vector<scene::TransformOperation>{
            scene::Translation{0, 0.75}, scene::Affine{1, 0, 0, -1, 0, 0}});
        builder.add_primitive(
            0, scene::Image{
                   {0.25, 0.5}, "data:image/png;base64,A&B<", "alt & < \"", std::move(transform)});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::none});
        expect(output.find("<image width=\"250\" height=\"500\" "
                           "transform=\"translate(0 750) matrix(1 0 0 -1 0 0)\" "
                           "preserveAspectRatio=\"none\" aria-label=\"alt &amp; &lt; &quot;\" "
                           "href=\"data:image/png;base64,A&amp;B&lt;\"/>") != output.npos);
    };

    "empty image attributes and signed zero dimensions remain owned values"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        builder.add_primitive(0, scene::Image{{-0.0005, 0}, "", "", {}});
        auto output =
            svg::write(std::move(builder).freeze(), {.font_cache = svg::FontCacheMode::local});
        expect(output.find("<defs/>") != output.npos);
        expect(output.find("<image width=\"-0.5\" height=\"0\" transform=\"\" "
                           "preserveAspectRatio=\"none\" aria-label=\"\" href=\"\"/>") !=
               output.npos);
    };

    "image remains after its preceding glyph in local cache mode"_test = [] {
        scene::Builder builder;
        builder.set_metrics({1, 0.5, 0.1});
        builder.set_font_metrics({1, 0.5, 0.5, 0.7});
        auto resource = scene::GlyphResource::make("NewCM-Roman", scene::GlyphOutline{});
        builder.add_primitive(
            0, scene::GlyphRun{{scene::Glyph{resource, {"normal", "NCM-N", 0x78}, {}, 0.5, {}}}});
        builder.add_primitive(0, scene::Image{{0.25, 0.5},
                                              "data:image/png;base64,AA==",
                                              "img",
                                              scene::Transform{scene::Affine{1, 0, 0, -1, 0, 0}}});
        auto source = std::move(builder).freeze();
        svg::SvgFontResourcesBuilder fonts;
        fonts.add({"normal", 0x78}, {true, "M0 0L2 2Z", {}});
        auto font_resources = std::move(fonts).freeze();
        auto output = svg::write(source, {.font_cache = svg::FontCacheMode::local,
                                          .font_resources = &font_resources,
                                          .local_cache_prefixes = {"4-"}});
        auto const use = output.find("<use data-c=\"78\"");
        auto const image = output.find("<image width=\"250\" height=\"500\"");
        expect(use != output.npos && image != output.npos && use < image);
        expect(output.find("href=\"#MJX-4-NCM-N-78\"") != output.npos);
    };
};
} // namespace
