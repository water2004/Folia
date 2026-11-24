#pragma once

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
#include "math/support/source_svg_paint.hpp"
#include "support/folia_test.hpp"
#include "math/support/tex_scene.hpp"
#include "math/support/scene_fixtures.hpp"
using math_test::fixtures::CommonSceneCase;
using math_test::fixtures::commonSceneCases;
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
    auto found = std::find_if(commonSceneCases().begin(), commonSceneCases().end(),
                              [&](auto const &item) { return item.name == name; });
    if (found == commonSceneCases().end())
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
