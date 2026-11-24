#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include "support/source_svg_paint.hpp"
#include "support/folia_test.hpp"

import folia.math.output.scene;
import folia.math.output.svg.font_data;
import folia.math.output.svg.writer;

#include "support/tex_scene.hpp"

using namespace boost::ut;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

#include "math/support/scene_fixtures.hpp"
using math_test::fixtures::CanonicalSceneCase;
using math_test::fixtures::CanonicalSceneMode;
using math_test::fixtures::CanonicalSceneMetrics;
using math_test::fixtures::sourceCanonicalSceneCases;

namespace {
bool close(double actual, double expected) {
    return std::abs(actual - expected) <= 1e-9;
}

void compareMetrics(scene::Metrics const &actual, CanonicalSceneMetrics const &expected,
                    std::string_view name) {
    expect(close(actual.width, expected.width))
        << name << ": Source outer bbox width actual=" << actual.width
        << " expected=" << expected.width;
    expect(close(actual.height, expected.height))
        << name << ": Source outer bbox height actual=" << actual.height
        << " expected=" << expected.height;
    expect(close(actual.depth, expected.depth))
        << name << ": Source outer bbox depth actual=" << actual.depth
        << " expected=" << expected.depth;
}

void reportDifference(std::string_view name, std::string_view expected, std::string_view actual) {
    if (expected == actual)
        return;
    std::size_t first = 0;
    while (first < expected.size() && first < actual.size() && expected[first] == actual[first])
        ++first;
    const auto begin = first > 48 ? first - 48 : 0;
    std::cerr << "hosted canonical SVG mismatch case=" << name << " first_byte=" << first
              << " expected_size=" << expected.size() << " actual_size=" << actual.size()
              << "\nexpected: " << expected.substr(begin, 112)
              << "\nactual:   " << actual.substr(begin, 112) << '\n';
}

tex_scene_test::TexSceneInput nativeInput(CanonicalSceneCase const &source) {
    return {source.tex,   source.requested_display, source.em, source.ex,
            source.width, source.tag_side};
}

svg::SvgWriteOptions writeOptions(CanonicalSceneMode const &mode, std::size_t modeIndex,
                                  tex_scene_test::InputOwner const &owner) {
    svg::SvgWriteOptions options;
    options.font_resources = &owner.fonts.svg_resources;
    if (modeIndex == 0) {
        options.font_cache = svg::FontCacheMode::none;
        expect(mode.local_cache_prefixes.empty())
            << "none cache mode must not carry local cache prefixes";
    } else {
        options.font_cache = svg::FontCacheMode::local;
        options.local_cache_prefixes.reserve(mode.local_cache_prefixes.size());
        for (const auto prefix : mode.local_cache_prefixes)
            options.local_cache_prefixes.emplace_back(prefix);
    }
    return options;
}
} // namespace

suite native_canonical_scene_source = [] {
    "pinned_TeX_hosted_canonical_75_layout_and_150_svg_modes_match_source"_test = [] {
        expect(std::size(sourceCanonicalSceneCases()) == 75u)
            << "hosted Source fixture contains the canonical 75 ordered inputs";
        if (std::size(sourceCanonicalSceneCases()) != 75)
            return;

        std::set<std::string_view> names;
        std::set<std::tuple<std::string_view, std::string_view, bool, double, double, double>>
            inputs;
        for (const auto &source : sourceCanonicalSceneCases()) {
            expect(names.insert(source.name).second) << source.name << ": case name is unique";
            expect(inputs
                       .emplace(source.tex, source.tag_side, source.requested_display, source.em,
                                source.ex, source.width)
                       .second)
                << source.name << ": ordered native input is unique";
            expect(source.modes[0].display == source.modes[1].display)
                << source.name << ": actual Source display is stable across cache modes";
            expect(source.modes[0].initial_allow_tex_html == source.modes[1].initial_allow_tex_html)
                << source.name << ": Source initialAllowTexHTML is stable across cache modes";
            expect(source.modes[0].listener_count == source.modes[1].listener_count)
                << source.name << ": Source listener count is stable across cache modes";
            expect(source.modes[0].listener_dom_unchanged && source.modes[1].listener_dom_unchanged)
                << source.name << ": Source listener registration preserves DOM serialization";
        }

        std::array<std::array<bool, 2>, 75> completedLayouts{};
        std::size_t layoutComparisons = 0;
        std::size_t svgComparisons = 0;
        for (std::size_t modeIndex = 0; modeIndex < 2; ++modeIndex) {
            tex_scene_test::InputOwner owner;
            tex_scene_test::LiteUnknownTextMeasurement measurement(owner.adaptor);
            for (std::size_t caseIndex = 0; caseIndex < std::size(sourceCanonicalSceneCases());
                 ++caseIndex) {
                const auto &source = sourceCanonicalSceneCases()[caseIndex];
                const auto &mode = source.modes[modeIndex];
                const std::string modeName =
                    std::string(source.name) + (modeIndex == 0 ? " [none]" : " [local]");
                expect(mode.error_name.empty())
                    << modeName << ": Source mode completed without error (" << mode.error_name
                    << ": " << mode.error_message << ')';
                try {
                    const auto input = nativeInput(source);
                    auto value = tex_scene_test::buildScene(input, owner, measurement, true);
                    compareMetrics(value.metrics(), mode.metrics, modeName);
                    completedLayouts[caseIndex][modeIndex] = true;
                    ++layoutComparisons;

                    auto options = writeOptions(mode, modeIndex, owner);
                    const auto serialized = svg::write(value, options);
                    const auto expectedSvg = source_svg_paint::materialize(mode.svg);
                    if (serialized != expectedSvg)
                        reportDifference(modeName, expectedSvg, serialized);
                    expect(serialized == expectedSvg)
                        << modeName << ": complete SVG matches Source with explicit CSS paint after request teardown";
                    ++svgComparisons;
                } catch (std::exception const &error) {
                    expect(false) << modeName << ": native hosted pipeline threw: " << error.what();
                }
            }
        }
        std::size_t layoutPairs = 0;
        for (const auto &modes : completedLayouts)
            if (modes[0] && modes[1])
                ++layoutPairs;
        expect(layoutPairs == 75u) << "native completed all 75 layout pairs across cache modes";
        expect(layoutComparisons == 150u) << "native compared 150 completed Scene metrics";
        expect(svgComparisons == 150u) << "native compared 150 complete Source SVG outputs";
        std::cout << "hosted canonical layout_pairs=" << layoutPairs
                  << " metric_comparisons=" << layoutComparisons
                  << " svg_comparisons=" << svgComparisons << '\n';
    };
};
