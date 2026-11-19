#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>
#include "support/source_svg_paint.hpp"
#include "support/folia_test.hpp"
#include "support/tex_scene.hpp"
#include "fixtures/common_scene.inc"
import folia.math.output.svg.writer;

using namespace boost::ut;
namespace mml = folia::math::mml;
namespace tex = folia::math::tex;
namespace common = folia::math::output::common;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;
using tex_scene_test::buildScene;
using tex_scene_test::InputOwner;
using tex_scene_test::LiteUnknownTextMeasurement;
using tex_scene_test::NoUnexpectedMeasurement;
using tex_scene_test::TexSceneInput;

inline TexSceneInput texInput(CommonSceneCase const &item) {
    return {item.tex,     item.display, item.em, item.ex, static_cast<double>(item.width),
            item.tag_side};
}

namespace {
std::string serialize(scene::Scene const &value, InputOwner const &owner) {
    svg::SvgWriteOptions options;
    options.font_cache = svg::FontCacheMode::none;
    options.font_resources = &owner.fonts.svg_resources;
    return svg::write(value, options);
}

void reportFirstDifference(std::string_view name, std::string_view expected,
                           std::string_view actual) {
    if (expected == actual)
        return;
    if (const char *dumpDirectory = std::getenv("FOLIA_MATH_SCENE_DUMP_DIR")) {
        const auto base = std::filesystem::path{dumpDirectory} / std::string{name};
        const auto writeSvg = [&](std::filesystem::path const &path, std::string_view value) {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output) {
                std::cerr << "Common-to-Scene could not open SVG dump " << path.string() << '\n';
                return;
            }
            output.write(value.data(), static_cast<std::streamsize>(value.size()));
            if (!output)
                std::cerr << "Common-to-Scene failed writing SVG dump " << path.string() << '\n';
        };
        writeSvg(base.string() + ".expected.svg", expected);
        writeSvg(base.string() + ".actual.svg", actual);
    }
    std::size_t at = 0;
    while (at < expected.size() && at < actual.size() && expected[at] == actual[at])
        ++at;
    const auto begin = at > 48 ? at - 48 : 0;
    std::cerr << "Common-to-Scene SVG mismatch case=" << name << " first_byte=" << at
              << " expected_size=" << expected.size() << " actual_size=" << actual.size()
              << "\nexpected: " << expected.substr(begin, 112)
              << "\nactual:   " << actual.substr(begin, 112) << '\n';
}

struct SceneStats {
    std::size_t groups{};
    std::size_t glyphs{};
    std::size_t paths{};
    std::size_t rules{};
    std::size_t ellipses{};
    std::size_t invalid_ellipses{};
    std::size_t text_runs{};
    std::size_t typed_fonts{};
    std::size_t typed_paints{};
    std::size_t phantom_groups{};
    std::size_t phantom_glyphs{};
    std::size_t phantom_entries{};
    std::size_t table_nodes{};
    std::size_t table_frame_roles{};
    std::size_t table_vertical_roles{};
    std::size_t table_horizontal_roles{};
    std::size_t table_viewports{};
    std::size_t table_unclipped_viewports{};
    std::size_t table_invalid_viewports{};
    std::size_t table_rules{};
    std::size_t table_invalid_paints{};
    std::size_t table_line_attributes{};
    std::size_t table_frame_attributes{};
    std::size_t table_label_attributes{};
    std::size_t table_class_attributes{};
    std::size_t table_background_rules{};
    std::size_t table_invalid_backgrounds{};
    std::size_t vertical_extender_roles{};
    std::size_t vertical_extender_viewports{};
    std::size_t horizontal_extender_roles{};
    std::size_t horizontal_extender_viewports{};
};

bool isTableRole(scene::ElementRole role) {
    return role == scene::ElementRole::table_frame ||
           role == scene::ElementRole::table_vertical_line ||
           role == scene::ElementRole::table_horizontal_line;
}

bool isTableViewportRole(scene::ElementRole role) {
    return role == scene::ElementRole::table_content_viewport ||
           role == scene::ElementRole::table_label_frame;
}

void inspectTableMetadata(scene::SemanticMetadata const &metadata, SceneStats &stats) {
    for (const auto &attribute : metadata.ordered_attributes) {
        if (attribute.name == "data-line")
            ++stats.table_line_attributes;
        else if (attribute.name == "data-frame")
            ++stats.table_frame_attributes;
        else if (attribute.name == "data-labels")
            ++stats.table_label_attributes;
        else if (attribute.name == "class")
            ++stats.table_class_attributes;
    }
}

void inspectTableRole(scene::ElementRole role, SceneStats &stats) {
    if (!isTableRole(role))
        return;
    if (role == scene::ElementRole::table_frame)
        ++stats.table_frame_roles;
    else if (role == scene::ElementRole::table_vertical_line)
        ++stats.table_vertical_roles;
    else
        ++stats.table_horizontal_roles;
}

void inspectTableRule(scene::ElementRole role, scene::Paint const &paint, SceneStats &stats) {
    if (!isTableRole(role))
        return;
    ++stats.table_rules;
    if (!std::holds_alternative<scene::NoPaint>(paint.fill) ||
        !std::holds_alternative<scene::InheritedPaint>(paint.stroke) ||
        !std::isfinite(paint.stroke_width) || std::abs(paint.stroke_width - 0.07) > 1e-9)
        ++stats.table_invalid_paints;
    if (paint.dash_array) {
        const auto &dash = *paint.dash_array;
        const bool dashed = dash.size() == 1 && std::abs(dash[0] - 0.14) <= 1e-9;
        const bool dotted =
            dash.size() == 2 && std::abs(dash[0]) <= 1e-9 && std::abs(dash[1] - 0.14) <= 1e-9;
        if (!dashed && !dotted)
            ++stats.table_invalid_paints;
        if (dotted && paint.line_cap != scene::StrokeCap::round)
            ++stats.table_invalid_paints;
    }
}

void inspect(scene::Group const &group, SceneStats &stats, bool insidePhantom = false) {
    ++stats.groups;
    if (group.metadata.source_tag == "mtable")
        ++stats.table_nodes;
    inspectTableMetadata(group.metadata, stats);
    inspectTableRole(group.role, stats);
    if (isTableViewportRole(group.role) && group.viewport) {
        ++stats.table_viewports;
        if (!group.viewport->clip_content)
            ++stats.table_unclipped_viewports;
        const auto &bounds = group.viewport->bounds;
        const auto &viewBox = group.viewport->view_box;
        if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) || !std::isfinite(bounds.width) ||
            !std::isfinite(bounds.height) || !std::isfinite(viewBox.x) ||
            !std::isfinite(viewBox.y) || !std::isfinite(viewBox.width) ||
            !std::isfinite(viewBox.height) || bounds.width <= 0 || bounds.height <= 0 ||
            viewBox.width <= 0 || viewBox.height <= 0)
            ++stats.table_invalid_viewports;
    }
    const bool isPhantom = insidePhantom || group.metadata.source_tag == "mphantom";
    if (group.metadata.source_tag == "mphantom")
        ++stats.phantom_groups;
    if (isPhantom)
        stats.phantom_entries += group.entries.size();
    if (group.role == scene::ElementRole::vertical_extender_viewport) {
        ++stats.vertical_extender_roles;
        stats.vertical_extender_viewports += group.viewport.has_value();
    } else if (group.role == scene::ElementRole::horizontal_extender_viewport) {
        ++stats.horizontal_extender_roles;
        stats.horizontal_extender_viewports += group.viewport.has_value();
    }
    for (auto const &entry : group.entries) {
        if (auto const *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child)
                inspect(**child, stats, isPhantom);
            continue;
        }
        if (std::holds_alternative<scene::ActionRegion>(entry)) {
            ++stats.rules;
            ++stats.typed_paints;
            continue;
        }
        auto const &primitive = std::get<scene::Primitive>(entry);
        std::visit(
            [&](auto const &value) {
                using T = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::same_as<T, scene::GlyphRun>) {
                    for (auto const &glyph : value.glyphs) {
                        ++stats.glyphs;
                        if (isPhantom)
                            ++stats.phantom_glyphs;
                        if (glyph.resource && !glyph.key.variant.empty())
                            ++stats.typed_fonts;
                        ++stats.typed_paints;
                    }
                } else if constexpr (std::same_as<T, scene::Path>) {
                    ++stats.paths;
                    inspectTableMetadata(value.metadata, stats);
                    inspectTableRole(value.role, stats);
                    inspectTableRule(value.role, value.paint, stats);
                    ++stats.typed_paints;
                } else if constexpr (std::same_as<T, scene::Rule>) {
                    ++stats.rules;
                    inspectTableMetadata(value.metadata, stats);
                    inspectTableRole(value.role, stats);
                    inspectTableRule(value.role, value.paint, stats);
                    const bool tableBackground =
                        value.role == scene::ElementRole::background_box ||
                        std::any_of(
                            value.metadata.ordered_attributes.begin(),
                            value.metadata.ordered_attributes.end(),
                            [](const auto &attribute) { return attribute.name == "data-bgcolor"; });
                    if (tableBackground) {
                        ++stats.table_background_rules;
                        const bool validBounds = std::isfinite(value.bounds.x) &&
                                                 std::isfinite(value.bounds.y) &&
                                                 std::isfinite(value.bounds.width) &&
                                                 std::isfinite(value.bounds.height) &&
                                                 value.bounds.width > 0 && value.bounds.height > 0;
                        const bool colorFill =
                            std::holds_alternative<scene::CssColor>(value.paint.fill) ||
                            std::holds_alternative<scene::Color>(value.paint.fill);
                        const bool noExplicitStroke =
                            std::holds_alternative<scene::NoPaint>(value.paint.stroke) ||
                            std::holds_alternative<scene::InheritedPaint>(value.paint.stroke);
                        if (!validBounds || !colorFill || !noExplicitStroke)
                            ++stats.table_invalid_backgrounds;
                    }
                    ++stats.typed_paints;
                } else if constexpr (std::same_as<T, scene::Ellipse>) {
                    ++stats.ellipses;
                    inspectTableMetadata(value.metadata, stats);
                    inspectTableRole(value.role, stats);
                    const bool validGeometry =
                        std::isfinite(value.center.x) && std::isfinite(value.center.y) &&
                        std::isfinite(value.radius_x) && std::isfinite(value.radius_y) &&
                        value.radius_x >= 0 && value.radius_y >= 0;
                    const bool validPaint = std::isfinite(value.paint.stroke_width) &&
                                            std::isfinite(value.paint.opacity) &&
                                            value.paint.stroke_width >= 0 &&
                                            value.paint.opacity >= 0 && value.paint.opacity <= 1;
                    if (!validGeometry || !validPaint)
                        ++stats.invalid_ellipses;
                    ++stats.typed_paints;
                } else if constexpr (std::same_as<T, scene::TextRun>) {
                    ++stats.text_runs;
                    if (value.font && !value.font->family.empty())
                        ++stats.typed_fonts;
                    ++stats.typed_paints;
                }
            },
            primitive);
    }
}

CommonSceneCase const &sourceCase(std::string_view name) {
    auto found = std::find_if(commonSceneCases.begin(), commonSceneCases.end(),
                              [&](auto const &item) { return item.name == name; });
    if (found == commonSceneCases.end())
        throw std::logic_error("missing pinned Common-to-Scene Source case");
    return *found;
}

std::size_t sourceMathSvgCount(std::string_view svg) {
    std::size_t count = 0;
    std::size_t cursor = 0;
    while ((cursor = svg.find("<svg", cursor)) != std::string_view::npos) {
        const auto end = svg.find('>', cursor);
        if (end == std::string_view::npos)
            break;
        const auto tag = svg.substr(cursor, end - cursor);
        if (tag.find("role=\"img\"") != std::string_view::npos)
            ++count;
        cursor = end + 1;
    }
    return count;
}

std::size_t sourceMmlTableCount(std::string_view svg) {
    constexpr std::string_view needle = "data-mml-node=\"mtable\"";
    std::size_t count = 0, cursor = 0;
    while ((cursor = svg.find(needle, cursor)) != std::string_view::npos) {
        ++count;
        cursor += needle.size();
    }
    return count;
}

bool sourceHasNestedTableViewport(std::string_view svg) {
    for (auto position = svg.find("<svg"); position != std::string_view::npos;
         position = svg.find("<svg", position + 4)) {
        const auto end = svg.find('>', position);
        if (end == std::string_view::npos)
            break;
        const auto tag = svg.substr(position, end - position);
        if (tag.find("data-table=\"true\"") != std::string_view::npos ||
            tag.find("data-labels=\"true\"") != std::string_view::npos)
            return true;
    }
    return false;
}

std::size_t sourceNestedAssemblyPieceCount(std::string_view svg, bool vertical) {
    std::size_t count = 0;
    std::size_t cursor = 0;
    while ((cursor = svg.find("<svg", cursor)) != std::string_view::npos) {
        const auto tagEnd = svg.find('>', cursor);
        if (tagEnd == std::string_view::npos)
            break;
        const auto tag = svg.substr(cursor, tagEnd - cursor);
        cursor = tagEnd + 1;
        if (tag.find("role=\"img\"") != std::string_view::npos)
            continue;
        const auto close = svg.find("</svg>", tagEnd);
        if (close == std::string_view::npos)
            continue;
        const auto piece = svg.substr(cursor, close - cursor);
        if (piece.find("<path data-c=") == std::string_view::npos)
            continue;
        if (vertical) {
            if (piece.find("transform=\"scale(1,") != std::string_view::npos)
                ++count;
        } else {
            const auto scale = piece.find("transform=\"scale(");
            if (scale == std::string_view::npos)
                continue;
            const auto comma = piece.find(',', scale);
            const auto closeParen = piece.find(')', comma);
            if (comma != std::string_view::npos && closeParen != std::string_view::npos &&
                piece.substr(comma + 1, closeParen - comma - 1) == "1")
                ++count;
        }
    }
    return count;
}
} // namespace

suite common_scene_source = [] {
    "real_TeX_to_MML_to_standard_Common_to_frozen_Scene_matches_pinned_Source_SVG"_test = [] {
        InputOwner owner;
        NoUnexpectedMeasurement measurement;
        LiteUnknownTextMeasurement canonicalMeasurement(owner.adaptor);
        // Stage coverage is deliberately limited to focused currently emitted
        // Common families, table branches, and isolated script-family probes.
        // The accepted canonical 75/150 profile has its separate test entry.
        constexpr std::array<std::string_view, 88> selected{
            "plain",
            "mathbb",
            "fraction-inline",
            "fraction-display",
            "root",
            "indexed-root",
            "scene-identifiers-numbers",
            "scene-fixed-operator",
            "scene-assembled-operator",
            "scene-script-sub",
            "scene-script-sup",
            "scene-script-subsup",
            "scene-script-nested",
            "scene-sum-limits-inline",
            "scene-sum-limits-display",
            "scene-overline-single",
            "scene-widehat-single",
            "scene-prescript-sup-slot",
            "scene-prescript-sub-slot",
            "scene-prescript-both-slots",
            "text",
            "inline-linebreak-narrow",
            "scene-inline-break-before",
            "scene-inline-break-after",
            "scene-text-wordbreak",
            "scene-zero-size-frame",
            "scene-vertical-extender-assembly",
            "scene-horizontal-extender-assembly",
            "bbox",
            "phantom",
            "base-smash-raise",
            "base-phantom-sides",
            "scene-simple-phantom",
            "scene-padding-negative-dw-center",
            "scene-padding-negative-dw-right",
            "matrix",
            "colortbl-row-cell",
            "display-linebreak-table",
            "scene-table-array-basic",
            "scene-table-array-column-rules",
            "scene-table-array-row-rules",
            "scene-table-array-cell-padding",
            "scene-table-array-paragraph-column",
            "scene-table-row-dashed",
            "scene-table-column-dashed",
            "scene-table-label-right",
            "scene-table-label-left-metric",
            "scene-table-nested-label",
            "scene-enclose-all-lines",
            "scene-enclose-paired-sides",
            "scene-enclose-radical-top",
            "cancel-lines",
            "enclose-notations",
            "scene-enclose-notation-top",
            "scene-enclose-notation-right",
            "scene-enclose-notation-bottom",
            "scene-enclose-notation-left",
            "scene-enclose-notation-actuarial",
            "scene-enclose-notation-madruwb",
            "scene-enclose-notation-updiagonalstrike",
            "scene-enclose-notation-downdiagonalstrike",
            "scene-enclose-notation-horizontalstrike",
            "scene-enclose-notation-verticalstrike",
            "scene-enclose-notation-box",
            "scene-enclose-notation-roundedbox",
            "scene-enclose-notation-circle",
            "scene-enclose-notation-phasorangle",
            "scene-enclose-notation-uparrow",
            "scene-enclose-notation-downarrow",
            "scene-enclose-notation-leftarrow",
            "scene-enclose-notation-rightarrow",
            "scene-enclose-notation-updownarrow",
            "scene-enclose-notation-leftrightarrow",
            "scene-enclose-notation-updiagonalarrow",
            "scene-enclose-notation-northeastarrow",
            "scene-enclose-notation-southeastarrow",
            "scene-enclose-notation-northwestarrow",
            "scene-enclose-notation-southwestarrow",
            "scene-enclose-notation-northeastsouthwestarrow",
            "scene-enclose-notation-northwestsoutheastarrow",
            "scene-enclose-notation-longdiv",
            "scene-enclose-notation-radical",
            "scene-enclose-boundary-roundedbox-empty",
            "scene-enclose-boundary-uparrow-left",
            "scene-enclose-boundary-rightarrow-top",
            "scene-enclose-boundary-arrow-custom-parameters",
            "scene-border-dashed",
            "scene-border-dotted"};
        const char *profileEnvironment = std::getenv("FOLIA_MATH_SCENE_PROFILE");
        bool canonicalProfile = false;
        if (profileEnvironment)
            canonicalProfile = std::string_view(profileEnvironment) == "canonical";
        if (profileEnvironment && !canonicalProfile) {
            std::cerr << "Common-to-Scene profile=unsupported category=unsupported_profile value="
                      << std::quoted(profileEnvironment) << '\n';
            expect(false) << "unsupported FOLIA_MATH_SCENE_PROFILE value";
            return;
        }
        std::vector<CommonSceneCase const *> executionCases;
        if (canonicalProfile) {
            if (commonSceneCases.size() < 75) {
                std::cerr << "Common-to-Scene profile=canonical category=fixture_too_short count="
                          << commonSceneCases.size() << '\n';
                expect(false) << "canonical Source fixture must contain the original 75 cases";
                return;
            }
            for (std::size_t index = 0; index < 75; ++index)
                executionCases.push_back(&commonSceneCases[index]);
        } else {
            for (auto name : selected)
                executionCases.push_back(&sourceCase(name));
        }
        for (auto const *sourceCaseValue : executionCases) {
            const auto &source = *sourceCaseValue;
            const auto name = source.name;
            try {
                const bool sourceOk = source.error_name.empty();
                if (!sourceOk) {
                    expect(false) << "Source conversion failed for case " << name << ": "
                                  << source.error_name << ": " << source.error_message;
                    if (!canonicalProfile) {
                        std::cerr << "Common-to-Scene case=" << name
                                  << " result=mismatch source-error=" << source.error_name << ": "
                                  << source.error_message << '\n';
                        continue;
                    }
                }
                auto &caseMeasurement =
                    canonicalProfile
                        ? static_cast<common::MeasurementTarget &>(canonicalMeasurement)
                        : static_cast<common::MeasurementTarget &>(measurement);
                auto completed =
                    buildScene(texInput(source), owner, caseMeasurement, canonicalProfile);
                if (!sourceOk) {
                    const auto actual = serialize(completed, owner);
                    (void)actual;
                    std::cerr
                        << "Common-to-Scene profile=canonical case=" << name
                        << " category=source_error_native_success result=failed source_error_name="
                        << std::quoted(std::string{source.error_name}) << " source_error_message="
                        << std::quoted(std::string{source.error_message})
                        << " native_outcome=scene_returned_without_source_svg\n";
                    continue;
                }
                SceneStats stats;
                inspect(completed.root(), stats);
                const bool sourceHasGlyphs = source.svg.find("data-c=") != std::string_view::npos;
                const bool sourceHasDrawing =
                    sourceHasGlyphs || source.svg.find("<path") != std::string_view::npos ||
                    source.svg.find("<rect") != std::string_view::npos ||
                    source.svg.find("<line") != std::string_view::npos ||
                    source.svg.find("<ellipse") != std::string_view::npos ||
                    source.svg.find("<polygon") != std::string_view::npos;
                const bool sourceHasTable =
                    source.svg.find("data-mml-node=\"mtable\"") != std::string_view::npos;
                const bool sourceHasVerticalTableLine =
                    source.svg.find("data-line=\"v\"") != std::string_view::npos;
                const bool sourceHasHorizontalTableLine =
                    source.svg.find("data-line=\"h\"") != std::string_view::npos;
                const bool sourceHasDashedTableLine =
                    source.svg.find("stroke-dasharray=") != std::string_view::npos;
                bool sourceHasTableFrameMetadata = false;
                for (auto attribute = source.svg.find("data-frame=\"true\"");
                     attribute != std::string_view::npos;
                     attribute = source.svg.find("data-frame=\"true\"", attribute + 18)) {
                    const auto tagStart = source.svg.rfind('<', attribute);
                    if (tagStart == std::string_view::npos ||
                        source.svg.substr(tagStart, attribute - tagStart).starts_with("<rect"))
                        continue;
                    sourceHasTableFrameMetadata = true;
                    break;
                }
                bool sourceHasTableFrameRect = false;
                for (auto rect = source.svg.find("<rect"); rect != std::string_view::npos;
                     rect = source.svg.find("<rect", rect + 5)) {
                    const auto end = source.svg.find('>', rect);
                    if (end == std::string_view::npos)
                        break;
                    if (source.svg.substr(rect, end - rect).find("data-frame=\"true\"") !=
                        std::string_view::npos) {
                        sourceHasTableFrameRect = true;
                        break;
                    }
                }
                const bool sourceHasTableFrame =
                    sourceHasTableFrameRect || sourceHasTableFrameMetadata;
                const bool sourceHasTableLabels =
                    source.svg.find("data-labels=\"true\"") != std::string_view::npos;
                const bool sourceHasTableLineMetadata =
                    source.svg.find("data-line=\"") != std::string_view::npos;
                const bool sourceHasTableClassMetadata =
                    source.svg.find("class=\"mjx-") != std::string_view::npos;
                const bool sourceHasTableBackground =
                    source.svg.find("data-bgcolor=\"true\"") != std::string_view::npos;
                const bool sourceHasPercentageTable =
                    source.svg.find("width=\"100%\"") != std::string_view::npos &&
                    source.svg.find("data-mjx-viewBox=") != std::string_view::npos;
                const bool requiresTableViewport = sourceHasNestedTableViewport(source.svg);
                const auto &metrics = completed.metrics();
                const bool completedMetrics = std::isfinite(metrics.width) &&
                                              std::isfinite(metrics.height) &&
                                              std::isfinite(metrics.depth);
                const auto &frame = completed.frame_layout();
                bool frameMetrics = std::isfinite(frame.minimum_extent) && frame.minimum_extent > 0;
                const auto sourceFragmentCount = sourceMathSvgCount(source.svg);
                const bool frameFragmentsMatchSource =
                    sourceFragmentCount > 1 ? frame.inline_fragments.size() == sourceFragmentCount
                                            : frame.inline_fragments.empty();
                bool fragmentRootsOwned = true;
                for (auto const &fragment : frame.inline_fragments) {
                    frameMetrics = frameMetrics && std::isfinite(fragment.metrics.width) &&
                                   std::isfinite(fragment.metrics.height) &&
                                   std::isfinite(fragment.metrics.depth);
                    bool ownedByRoot = false;
                    for (auto const &entry : completed.root().entries) {
                        if (auto const *group =
                                std::get_if<std::shared_ptr<scene::Group const>>(&entry))
                            ownedByRoot = ownedByRoot || (*group && (*group)->id == fragment.root);
                    }
                    fragmentRootsOwned = fragmentRootsOwned && ownedByRoot;
                }
                bool extenderRolesMatchSource = true;
                if (name == "scene-vertical-extender-assembly") {
                    const auto sourcePieces = sourceNestedAssemblyPieceCount(source.svg, true);
                    extenderRolesMatchSource = sourcePieces > 0 &&
                        stats.vertical_extender_roles == sourcePieces &&
                        stats.vertical_extender_viewports == sourcePieces;
                } else if (name == "scene-horizontal-extender-assembly") {
                    const auto sourcePieces = sourceNestedAssemblyPieceCount(source.svg, false);
                    extenderRolesMatchSource = sourcePieces > 0 &&
                        stats.horizontal_extender_roles == sourcePieces &&
                        stats.horizontal_extender_viewports == sourcePieces;
                }
                bool brokenBorderGeometryOk = true;
                if (name == "scene-border-dashed" || name == "scene-border-dotted") {
                    const bool dotted = name == "scene-border-dotted";
                    std::vector<std::array<double, 7>> observed;
                    const auto collectLines = [&](auto &&self, scene::Group const &group) -> void {
                        for (auto const &entry : group.entries) {
                            if (auto const *child =
                                    std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
                                if (*child)
                                    self(self, **child);
                                continue;
                            }
                            if (!std::holds_alternative<scene::Primitive>(entry))
                                continue;
                            auto const &primitive = std::get<scene::Primitive>(entry);
                            const auto *line = std::get_if<scene::Path>(&primitive);
                            if (!line || line->element != scene::PathElement::line ||
                                !line->paint.dash_array)
                                continue;
                            brokenBorderGeometryOk = brokenBorderGeometryOk &&
                                line->paint.dash_array->size() == 2 && line->paint.line_cap ==
                                    (dotted ? scene::StrokeCap::round : scene::StrokeCap::square) &&
                                std::get_if<scene::CssColor>(&line->paint.stroke) &&
                                std::get<scene::CssColor>(line->paint.stroke).value == "red" &&
                                line->geometry.segments.size() == 2;
                            const auto *from = line->geometry.segments.size() == 2
                                ? std::get_if<scene::MoveTo>(&line->geometry.segments[0]) : nullptr;
                            const auto *to = line->geometry.segments.size() == 2
                                ? std::get_if<scene::LineTo>(&line->geometry.segments[1]) : nullptr;
                            if (!from || !to || line->paint.dash_array->size() != 2)
                                continue;
                            observed.push_back({from->to.x, from->to.y, to->to.x, to->to.y,
                                line->paint.stroke_width, (*line->paint.dash_array)[0],
                                (*line->paint.dash_array)[1]});
                        }
                    };
                    collectLines(collectLines, completed.root());
                    const std::vector<std::array<double, 7>> expected = dotted
                        ? std::vector<std::array<double, 7>>{
                            {-.06025,.64325,2.3446944444444444,.64325,.1105,.001,.21663131313131312},
                            {2.3446944444444444,-.26525,2.3446944444444444,.64325,.1105,.001,.1797},
                            {-.06025,-.26525,2.3446944444444444,-.26525,.1105,.001,.21663131313131312},
                            {-.06025,-.26525,-.06025,.64325,.1105,.001,.1797}}
                        : std::vector<std::array<double, 7>>{
                            {-.06025,.64325,2.3446944444444444,.64325,.1105,.09619777777777777,.2885933333333333},
                            {2.3446944444444444,-.26525,2.3446944444444444,.64325,.1105,.10094444444444445,.30283333333333334},
                            {-.06025,-.26525,2.3446944444444444,-.26525,.1105,.09619777777777777,.2885933333333333},
                            {-.06025,-.26525,-.06025,.64325,.1105,.10094444444444445,.30283333333333334}};
                    brokenBorderGeometryOk = brokenBorderGeometryOk && observed.size() == 4;
                    for (const auto &wanted : expected)
                        brokenBorderGeometryOk = brokenBorderGeometryOk &&
                            std::ranges::any_of(observed, [&](const auto &actual) {
                                for (std::size_t i = 0; i < wanted.size(); ++i)
                                    if (std::abs(actual[i] - wanted[i]) > 1e-10)
                                        return false;
                                return true;
                            });
                    expect(bool(brokenBorderGeometryOk))
                        << "CommonWrapper border geometry differs from the Source observation for "
                        << name;
                }
                const bool typedSceneOk =
                    stats.groups > 0 && completedMetrics && frameMetrics && fragmentRootsOwned &&
                    frameFragmentsMatchSource && extenderRolesMatchSource &&
                    stats.invalid_ellipses == 0 &&
                    (!sourceHasGlyphs || (stats.glyphs > 0 && stats.typed_fonts > 0)) &&
                    (!sourceHasDrawing || stats.typed_paints > 0);
                bool phantomEmissionOk = true;
                if (name == "scene-simple-phantom") {
                    phantomEmissionOk = !sourceHasGlyphs && stats.phantom_groups > 0 &&
                                        stats.phantom_glyphs == 0 && stats.phantom_entries == 0 &&
                                        metrics.width > 0 && metrics.height + metrics.depth > 0;
                }
                bool tableStructureOk = true;
                if (sourceHasTable) {
                    tableStructureOk = stats.table_nodes > 0;
                    const auto sourceTableCount = sourceMmlTableCount(source.svg);
                    if (sourceTableCount > 1)
                        tableStructureOk =
                            tableStructureOk && stats.table_nodes >= sourceTableCount;
                    if (sourceHasVerticalTableLine)
                        tableStructureOk = tableStructureOk && stats.table_vertical_roles > 0;
                    if (sourceHasHorizontalTableLine)
                        tableStructureOk = tableStructureOk && stats.table_horizontal_roles > 0;
                    if (sourceHasDashedTableLine || sourceHasTableFrame)
                        tableStructureOk = tableStructureOk && stats.table_rules > 0;
                    if (sourceHasTableLabels)
                        tableStructureOk = tableStructureOk && stats.table_label_attributes > 0;
                    if (sourceHasTableLineMetadata)
                        tableStructureOk = tableStructureOk && stats.table_line_attributes > 0;
                    if (sourceHasTableFrameMetadata)
                        tableStructureOk = tableStructureOk && stats.table_frame_attributes > 0;
                    if (sourceHasTableClassMetadata)
                        tableStructureOk = tableStructureOk && stats.table_class_attributes > 0;
                    if (sourceHasTableBackground)
                        tableStructureOk = tableStructureOk && stats.table_background_rules > 0 &&
                                           stats.table_invalid_backgrounds == 0;
                    if (sourceHasPercentageTable) {
                        tableStructureOk = tableStructureOk && frame.percentage.has_value();
                    }
                    if (requiresTableViewport) {
                        tableStructureOk =
                            tableStructureOk && stats.table_viewports > 0 &&
                            stats.table_unclipped_viewports == stats.table_viewports &&
                            stats.table_invalid_viewports == 0;
                    }
                    tableStructureOk = tableStructureOk && stats.table_invalid_paints == 0;
                }
                expect(bool(stats.groups > 0)) << "no real Scene groups for " << name;
                expect(bool(completedMetrics)) << "non-finite completed Scene metrics for " << name;
                expect(bool(frameMetrics))
                    << "invalid completed frame or fragment metrics for " << name;
                expect(bool(frameFragmentsMatchSource))
                    << "Scene frame fragment count differs from Source SVG segments for " << name;
                expect(bool(stats.invalid_ellipses == 0))
                    << "Scene ellipse has invalid geometry or paint for " << name;
                expect(bool(fragmentRootsOwned))
                    << "inline fragments are not owned by the Scene root for " << name;
                expect(bool(extenderRolesMatchSource))
                    << "Source extender-piece count differs from typed Scene viewport roles for "
                    << name << " (vertical=" << stats.vertical_extender_roles << "/"
                    << stats.vertical_extender_viewports << ", horizontal="
                    << stats.horizontal_extender_roles << "/"
                    << stats.horizontal_extender_viewports << ")";
                if (name == "scene-simple-phantom")
                    expect(bool(phantomEmissionOk)) << "phantom must keep nonzero measured "
                                                       "dimensions but emit no hidden drawing for "
                                                    << name << " (groups=" << stats.phantom_groups
                                                    << ", entries=" << stats.phantom_entries
                                                    << ", glyphs=" << stats.phantom_glyphs << ')';
                expect(bool(tableStructureOk))
                    << "Source table attrs/lines/labels differ from typed Scene table roles for "
                    << name << " (nodes=" << stats.table_nodes
                    << ", frame=" << stats.table_frame_roles
                    << ", vertical=" << stats.table_vertical_roles
                    << ", horizontal=" << stats.table_horizontal_roles
                    << ", viewport=" << stats.table_viewports
                    << ", unclipped=" << stats.table_unclipped_viewports
                    << ", rules=" << stats.table_rules
                    << ", invalid_paint=" << stats.table_invalid_paints
                    << ", line_attrs=" << stats.table_line_attributes
                    << ", frame_attrs=" << stats.table_frame_attributes
                    << ", label_attrs=" << stats.table_label_attributes
                    << ", backgrounds=" << stats.table_background_rules
                    << ", invalid_backgrounds=" << stats.table_invalid_backgrounds << ')';
                if (sourceHasGlyphs) {
                    expect(bool(stats.glyphs > 0))
                        << "Source has glyphs but Scene has none for " << name;
                    expect(bool(stats.typed_fonts > 0))
                        << "Source has glyphs but Scene has no typed font resources for " << name;
                }
                if (sourceHasDrawing)
                    expect(bool(stats.typed_paints > 0))
                        << "Source has drawing but Scene has no typed paint for " << name;
                const auto actual = serialize(completed, owner);
                const auto expectedSvg = source_svg_paint::materialize(source.svg);
                reportFirstDifference(name, expectedSvg, actual);
                const bool svgMatches = actual == expectedSvg;
                expect(bool(svgMatches)) << "Source SVG with explicit table CSS paint differs for " << name;
                const bool casePassed =
                    typedSceneOk && phantomEmissionOk && tableStructureOk &&
                    brokenBorderGeometryOk && svgMatches;
                if (canonicalProfile)
                    std::cerr << "Common-to-Scene profile=canonical case=" << name << " category="
                              << (casePassed   ? "source_success_svg_equal"
                                  : svgMatches ? "scene_validation_mismatch"
                                               : "source_success_svg_mismatch")
                              << " result=" << (casePassed ? "success" : "failed");
                else
                    std::cerr << "Common-to-Scene case=" << name
                              << " result=" << (casePassed ? "success" : "mismatch");
                std::cerr << " viewport_roles(vertical=" << stats.vertical_extender_roles
                          << ",horizontal=" << stats.horizontal_extender_roles << ')'
                          << " phantom(groups=" << stats.phantom_groups
                          << ",entries=" << stats.phantom_entries
                          << ",glyphs=" << stats.phantom_glyphs << ')'
                          << " table(nodes=" << stats.table_nodes
                          << ",frame=" << stats.table_frame_roles
                          << ",vertical=" << stats.table_vertical_roles
                          << ",horizontal=" << stats.table_horizontal_roles
                          << ",viewport=" << stats.table_viewports
                          << ",class_attrs=" << stats.table_class_attributes
                          << ",backgrounds=" << stats.table_background_rules << ')' << '\n';
            } catch (std::exception const &error) {
                if (canonicalProfile) {
                    std::cerr << "Common-to-Scene profile=canonical case=" << name << " category="
                              << (source.error_name.empty() ? "source_success_native_exception"
                                                            : "source_error_native_exception")
                              << " result=failed source_error_name="
                              << std::quoted(std::string{source.error_name})
                              << " source_error_message="
                              << std::quoted(std::string{source.error_message})
                              << " native_exception_type=" << std::quoted(typeid(error).name())
                              << " native_exception_message=" << std::quoted(error.what()) << '\n';
                } else {
                    std::cerr << "Common-to-Scene case=" << name
                              << " result=exception: " << error.what() << '\n';
                }
                expect(false) << "Common-to-Scene case " << name << " threw: " << error.what();
            } catch (...) {
                if (canonicalProfile) {
                    std::cerr << "Common-to-Scene profile=canonical case=" << name << " category="
                              << (source.error_name.empty() ? "source_success_native_exception"
                                                            : "source_error_native_exception")
                              << " result=failed source_error_name="
                              << std::quoted(std::string{source.error_name})
                              << " source_error_message="
                              << std::quoted(std::string{source.error_message})
                              << " native_exception_type=non-standard "
                                 "native_exception_message=unavailable\n";
                } else {
                    std::cerr << "Common-to-Scene case=" << name
                              << " result=exception: non-standard exception\n";
                }
                expect(false) << "Common-to-Scene case " << name
                              << " threw a non-standard exception";
            }
        }
    };

    "frozen_Scene_serializes_after_job_and_document_retirement_and_repeat_request"_test = [] {
        InputOwner owner;
        NoUnexpectedMeasurement measurement;
        constexpr std::array<std::string_view, 12> repeatedCases{
            "fraction-inline",
            "plain",
            "scene-inline-break-before",
            "scene-inline-break-after",
            "scene-table-array-row-rules",
            "scene-table-label-right",
            "scene-table-label-left-metric",
            "scene-enclose-radical-top",
            "scene-enclose-boundary-roundedbox-empty",
            "scene-enclose-notation-circle",
            "scene-enclose-notation-longdiv",
            "scene-enclose-notation-northeastsouthwestarrow"};
        for (auto name : repeatedCases) {
            const auto &source = sourceCase(name);
            auto first = buildScene(texInput(source), owner, measurement);
            const auto firstSvg = serialize(first, owner);
            auto second = buildScene(texInput(source), owner, measurement);
            const auto secondSvg = serialize(second, owner);
            const bool repeatedMatches = firstSvg == secondSvg;
            const auto expectedSvg = source_svg_paint::materialize(source.svg);
            const bool sourceMatches = firstSvg == expectedSvg;
            expect(bool(repeatedMatches))
                << "repeated request changed the completed Scene output for " << name;
            expect(bool(sourceMatches))
                << "retired-request Scene differs from independent Source SVG for " << name;
            reportFirstDifference(name, expectedSvg, firstSvg);
            std::cerr << "Common-to-Scene case=" << name << "-retirement result="
                      << (repeatedMatches && sourceMatches ? "success" : "mismatch") << '\n';
        }
    };
};
