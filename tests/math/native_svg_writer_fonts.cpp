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

suite native_svg_writer_fonts = [] {
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
        expect(empty_path.find("<path data-c=\"20\" d=\"\" stroke-width=\"0\"/>") != empty_path.npos);

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


};
