#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "fixtures/canonical_scene_modes_source.hpp"
#include "support/source_svg_paint.hpp"
#include "support/folia_test.hpp"

import folia.math;
import folia.math.output.scene;
import folia.math.util.utf16;

using namespace boost::ut;
namespace scene = folia::math::output::scene;
using folia::math::Renderer;
using folia::math::RenderOptions;
using folia::math::RenderResult;

namespace {
bool close(double actual, double expected) {
    return std::abs(actual - expected) <= 1e-9;
}

RenderOptions optionsFor(CanonicalSceneCase const &source) {
    RenderOptions options;
    options.display = source.requested_display;
    options.em = source.em;
    options.containerWidth = source.width;
    return options;
}

void compareMetrics(scene::Metrics const &actual, CanonicalSceneMetrics const &expected,
                    std::string_view name) {
    expect(close(actual.width, expected.width)) << name << ": width matches Source";
    expect(close(actual.height, expected.height)) << name << ": height matches Source";
    expect(close(actual.depth, expected.depth)) << name << ": depth matches Source";
}

struct GeometrySignature {
    scene::Metrics metrics;
    scene::FontMetrics fontMetrics;
    std::vector<scene::Transform> groupTransforms;
    std::vector<std::size_t> entryKinds;
    std::vector<scene::GlyphKey> glyphKeys;
    std::vector<scene::Point> glyphOrigins;
    std::vector<double> glyphAdvances;
    std::vector<scene::Rect> ruleBounds;
    std::vector<std::vector<scene::PathSegment>> paths;
    std::vector<scene::Transform> pathTransforms;

    friend bool operator==(GeometrySignature const &, GeometrySignature const &) = default;
};

void appendGlyphRun(GeometrySignature &signature, scene::GlyphRun const &run) {
    for (auto const &glyph : run.glyphs) {
        signature.glyphKeys.push_back(glyph.key);
        signature.glyphOrigins.push_back(glyph.origin);
        signature.glyphAdvances.push_back(glyph.advance);
    }
}

void appendGroupGeometry(GeometrySignature &signature, scene::Group const &group) {
    signature.groupTransforms.push_back(group.transform);
    for (auto const &entry : group.entries) {
        if (auto const *primitive = std::get_if<scene::Primitive>(&entry)) {
            signature.entryKinds.push_back(primitive->index());
            if (auto const *run = std::get_if<scene::GlyphRun>(primitive))
                appendGlyphRun(signature, *run);
            else if (auto const *rule = std::get_if<scene::Rule>(primitive))
                signature.ruleBounds.push_back(rule->bounds);
            else if (auto const *path = std::get_if<scene::Path>(primitive)) {
                signature.paths.push_back(path->geometry.segments);
                signature.pathTransforms.push_back(path->transform);
            } else if (auto const *text = std::get_if<scene::TextRun>(primitive)) {
                appendGlyphRun(signature, text->measured_glyphs);
            }
        } else if (auto const *child =
                       std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            signature.entryKinds.push_back(std::numeric_limits<std::size_t>::max());
            if (*child)
                appendGroupGeometry(signature, **child);
        } else {
            signature.entryKinds.push_back(std::numeric_limits<std::size_t>::max() - 1u);
            auto const &action = std::get<scene::ActionRegion>(entry);
            signature.ruleBounds.push_back(action.hitbox.bounds);
            if (action.tooltip)
                appendGroupGeometry(signature, action.tooltip->root());
        }
    }
}

GeometrySignature geometrySignature(scene::Scene const &value) {
    GeometrySignature result;
    result.metrics = value.metrics();
    result.fontMetrics = value.font_metrics();
    appendGroupGeometry(result, value.root());
    return result;
}

bool hasGlyph(scene::Group const &group, std::uint32_t codepoint) {
    for (auto const &entry : group.entries) {
        if (auto const *primitive = std::get_if<scene::Primitive>(&entry)) {
            const auto contains = [codepoint](scene::GlyphRun const &run) {
                return std::ranges::any_of(run.glyphs, [codepoint](scene::Glyph const &glyph) {
                    return glyph.key.codepoint == codepoint;
                });
            };
            if (auto const *run = std::get_if<scene::GlyphRun>(primitive); run && contains(*run))
                return true;
            if (auto const *text = std::get_if<scene::TextRun>(primitive);
                text && contains(text->measured_glyphs))
                return true;
        } else if (auto const *child =
                       std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child && hasGlyph(**child, codepoint))
                return true;
        } else if (auto const &tooltip = std::get<scene::ActionRegion>(entry).tooltip;
                   tooltip && hasGlyph(tooltip->root(), codepoint)) {
            return true;
        }
    }
    return false;
}

bool completeSvg(RenderResult const &result) {
    return result.svg.find("<svg") != std::string::npos &&
           result.svg.find("</svg>") != std::string::npos;
}

bool hasMathError(RenderResult const &result) {
    return result.svg.find("data-mml-node=\"merror\"") != std::string::npos;
}

void reportDifference(std::string_view name, std::string_view expected, std::string_view actual) {
    if (expected == actual)
        return;
    std::size_t first = 0;
    while (first < expected.size() && first < actual.size() && expected[first] == actual[first])
        ++first;
    const auto begin = first > 48 ? first - 48 : 0;
    std::cerr << "Renderer Source mismatch " << name << " first_byte=" << first
              << " expected_size=" << expected.size() << " actual_size=" << actual.size()
              << "\nexpected: " << expected.substr(begin, 112)
              << "\nactual:   " << actual.substr(begin, 112) << '\n';
}
} // namespace

suite mathjax_full_profile_package_bootstrap = [] {
    "renderer_public_sessions_keep_proofs_formula_sequence_and_scene_geometry"_test = [] {
        constexpr std::string_view proofA =
            R"(\begin{prooftree}\AxiomC{L}\UnaryInfC{R}\end{prooftree})";
        constexpr std::string_view proofB =
            R"(\begin{prooftree}\AxiomC{P}\AxiomC{Q}\BinaryInfC{$P\land Q$}\end{prooftree})";

        Renderer first;
        const auto firstProof = first.render(proofA);
        const auto ordinary = first.render("x+1");
        const auto secondProof = first.render(proofB);
        Renderer independent;
        const auto independentProof = independent.render(proofA);

        for (const auto *result : {&firstProof, &independentProof}) {
            expect(!result->scene.root().entries.empty());
            expect(!hasMathError(*result));
            expect(completeSvg(*result));
            expect(hasGlyph(result->scene.root(), static_cast<std::uint32_t>('L')));
            expect(hasGlyph(result->scene.root(), static_cast<std::uint32_t>('R')));
        }
        expect(firstProof.svg == independentProof.svg)
            << "separate real Renderer instances produce the same complete proof SVG";
        expect(firstProof.scene.metrics() == independentProof.scene.metrics());
        expect(geometrySignature(firstProof.scene) == geometrySignature(independentProof.scene));

        expect(!ordinary.scene.root().entries.empty() && !hasMathError(ordinary) &&
               completeSvg(ordinary));
        expect(hasGlyph(ordinary.scene.root(), 0x1D465u));
        expect(hasGlyph(ordinary.scene.root(), static_cast<std::uint32_t>('1')));
        expect(!secondProof.scene.root().entries.empty() && !hasMathError(secondProof) &&
               completeSvg(secondProof));
        expect(hasGlyph(secondProof.scene.root(), static_cast<std::uint32_t>('P')));
        expect(hasGlyph(secondProof.scene.root(), static_cast<std::uint32_t>('Q')));
        expect(firstProof.svg != ordinary.svg && ordinary.svg != secondProof.svg &&
               firstProof.svg != secondProof.svg)
            << "each result in the same Renderer sequence reflects its TeX input";

        constexpr std::string_view formula = R"(\frac{a}{b})";
        const auto sceneOnly = first.renderScene(formula);
        const auto rendered = first.render(formula);
        expect(!sceneOnly.root().entries.empty() && !rendered.scene.root().entries.empty());
        expect(sceneOnly.metrics() == rendered.scene.metrics())
            << "renderScene and render retain equal completed bounding-box metrics";
        expect(geometrySignature(sceneOnly) == geometrySignature(rendered.scene))
            << "renderScene and render retain the same typed geometry";
    };

    "renderer_public_options_match_all_75_none_profile_cases_in_both_encodings"_test = [] {
        expect(std::size(sourceCanonicalSceneCases) == 75u);
        if (std::size(sourceCanonicalSceneCases) != 75u)
            return;

        std::size_t utf8Metrics = 0, utf8Svg = 0, utf16Metrics = 0, utf16Svg = 0;
        std::size_t pairedSceneGeometry = 0;
        for (const auto &source : sourceCanonicalSceneCases) {
            expect(source.tag_side.empty()) << source.name << ": tag-side is default profile";
            expect(source.em == 16.0 && source.ex == 8.0)
                << source.name << ": public renderer metrics use default em/ex profile";
            expect(source.ex == source.em * .5) << source.name << ": ex follows public em";
            expect(source.width == 1280.0 || source.width == 100.0 || source.width == 80.0 ||
                   source.width == 120.0)
                << source.name << ": container width is an exposed Renderer option";
            const auto &expected = source.modes[0];
            expect(expected.error_name.empty()) << source.name << ": Source none mode succeeded";
            expect(expected.local_cache_prefixes.empty())
                << source.name << ": none mode has no cache prefixes";
        }

        {
            Renderer renderer;
            Renderer sceneRenderer;
            for (const auto &source : sourceCanonicalSceneCases) {
                const auto &expected = source.modes[0];
                try {
                    auto result = renderer.render(source.tex, optionsFor(source));
                    compareMetrics(result.scene.metrics(), expected.metrics, source.name);
                    ++utf8Metrics;
                    const auto expectedSvg = source_svg_paint::materialize(expected.svg);
                    if (result.svg != expectedSvg)
                        reportDifference(source.name, expectedSvg, result.svg);
                    expect(result.svg == expectedSvg) << source.name << ": UTF-8 full SVG with explicit table CSS paint";
                    ++utf8Svg;
                    const auto sceneOnly = sceneRenderer.renderScene(source.tex, optionsFor(source));
                    compareMetrics(sceneOnly.metrics(), expected.metrics, source.name);
                    expect(geometrySignature(sceneOnly) == geometrySignature(result.scene))
                        << source.name << ": scene-only and SVG routes retain the same geometry";
                    ++pairedSceneGeometry;
                } catch (std::exception const &error) {
                    expect(false) << source.name << ": UTF-8 Renderer threw: " << error.what();
                }
            }
        }
        {
            Renderer renderer;
            for (const auto &source : sourceCanonicalSceneCases) {
                const auto &expected = source.modes[0];
                try {
                    const auto utf16 = folia::math::utf16::decode(source.tex).text;
                    auto result = renderer.render(std::u16string_view(utf16), optionsFor(source));
                    compareMetrics(result.scene.metrics(), expected.metrics, source.name);
                    ++utf16Metrics;
                    const auto expectedSvg = source_svg_paint::materialize(expected.svg);
                    if (result.svg != expectedSvg)
                        reportDifference(source.name, expectedSvg, result.svg);
                    expect(result.svg == expectedSvg) << source.name << ": UTF-16 full SVG with explicit table CSS paint";
                    ++utf16Svg;
                } catch (std::exception const &error) {
                    expect(false) << source.name << ": UTF-16 Renderer threw: " << error.what();
                }
            }
        }
        expect(utf8Metrics == 75u && utf8Svg == 75u)
            << "UTF-8 Renderer completed all profile metrics and SVGs";
        expect(utf16Metrics == 75u && utf16Svg == 75u)
            << "UTF-16 Renderer completed all profile metrics and SVGs";
        expect(pairedSceneGeometry == 75u)
            << "all ordered profile cases compare geometry across the two public output routes";
    };

    "renderer_unicode_malformed_recovery_and_scene_result_survive_owner_teardown"_test = [] {
        std::optional<RenderResult> retained;
        std::optional<scene::Scene> retainedScene;
        {
            Renderer renderer;
            const std::string unicode = "\\text{漢字😀 é}";
            const auto utf8 = renderer.render(std::string_view(unicode));
            const auto utf16Text = folia::math::utf16::decode(unicode).text;
            const auto utf16 = renderer.render(std::u16string_view(utf16Text));
            expect(utf8.svg == utf16.svg) << "UTF-8 and UTF-16 Unicode output agrees";

            const auto fraction = renderer.render(std::string_view("\\frac{a}{b}"));
            const auto sceneOnly = renderer.renderScene(std::string_view("\\frac{a}{b}"));
            const auto serialized = renderer.toSvg(std::string_view("\\frac{a}{b}"));
            expect(fraction.svg == serialized) << "toSvg agrees with rendered SVG";
            expect(close(sceneOnly.metrics().width, fraction.scene.metrics().width) &&
                   close(sceneOnly.metrics().height, fraction.scene.metrics().height) &&
                   close(sceneOnly.metrics().depth, fraction.scene.metrics().depth))
                << "renderScene retains the same completed layout metrics";

            const auto malformed = renderer.render(std::string_view("\\frac{a}{"));
            expect(malformed.svg.find("data-mjx-error=") != std::string::npos)
                << "malformed TeX remains a successful Source merror output";
            expect(!renderer.render(std::string_view("x+1")).svg.empty())
                << "renderer session recovers after malformed TeX";
            retained.emplace(renderer.render(std::string_view("\\mathbb{R}")));
            retainedScene.emplace(renderer.renderScene(std::string_view("\\sqrt{x}")));
        }
        expect(retained.has_value() && !retained->svg.empty() &&
               !retained->scene.root().entries.empty())
            << "RenderResult owns SVG and Scene after Renderer teardown";
        expect(retainedScene.has_value() && !retainedScene->root().entries.empty() &&
               retainedScene->metrics().width > 0)
            << "Scene-only result remains readable after Renderer teardown";
    };
};
