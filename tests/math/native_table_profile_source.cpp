#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include "support/source_svg_paint.hpp"
#include "support/svg_paint_artifacts.hpp"
#include "support/folia_test.hpp"

import folia.math.core.mml_tree.mml_document;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.scene;
import folia.math.output.svg.wrapper_factory;
import folia.math.output.svg.writer;

#include "support/mathml_scene.hpp"

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace mml = folia::math::mml;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

struct TableProfileBBox {
    double w{}, h{}, d{}, scale{}, rscale{}, L{}, R{};
};
struct TableProfileTable {
    std::size_t node_index{};
    TableProfileBBox bbox;
    std::size_t rows{}, cols{};
    bool has_labels{}, is_top{};
    double pwidth{};
    std::span<double const> H, D, W, NH, ND;
    double label_width{};
};
struct TableProfileMeasurement {
    std::u16string_view text;
    std::string_view variant, font_family;
    bool italic{}, bold{};
    double w{}, h{}, d{};
};
struct TableProfileSourceCase {
    std::string_view name, mathml;
    bool requested_display{}, display{};
    double em{}, ex{}, width{};
    std::string_view font_cache;
    TableProfileBBox root_bbox;
    std::string_view error_name, error_message, svg;
    std::span<std::string_view const> local_cache_prefixes;
    std::span<TableProfileTable const> tables;
    std::span<TableProfileMeasurement const> measurements;
    std::span<mathml_scene_test::Node const> nodes;
    std::span<mathml_scene_test::Attribute const> attributes;
};

#include "fixtures/table_profile_source.hpp"

namespace {
bool close(double actual, double expected) {
    return std::abs(actual - expected) <= 1e-9;
}

void reportDifference(std::string_view name, std::string_view expected, std::string_view actual) {
    std::size_t first = 0;
    while (first < expected.size() && first < actual.size() && expected[first] == actual[first])
        ++first;
    const auto begin = first > 48 ? first - 48 : 0;
    std::cerr << "table profile SVG mismatch case=" << name << " first_byte=" << first
              << " expected_size=" << expected.size() << " actual_size=" << actual.size()
              << "\nexpected: " << expected.substr(begin, 112)
              << "\nactual:   " << actual.substr(begin, 112) << '\n';
}

class SourceMeasurement final : public common::MeasurementTarget {
    std::span<TableProfileMeasurement const> expected_;

  public:
    std::size_t next{};

    explicit SourceMeasurement(std::span<TableProfileMeasurement const> expected)
        : expected_(expected) {}

    common::ExtendedMetrics measureMetrics(common::MetricRequest const &request) override {
        throw std::logic_error("unexpected metrics request for family=" + request.family);
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &request) override {
        if (next >= expected_.size()) {
            throw std::logic_error("unexpected unknown-text request: text_units=" +
                                   std::to_string(request.text.size()) + ", variant=" +
                                   request.variant + ", family=" + request.font.family);
        }
        const auto &expected = expected_[next++];
        const bool matches = request.text == expected.text && request.variant == expected.variant &&
                             request.font.family == expected.font_family &&
                             request.font.italic == expected.italic &&
                             request.font.bold == expected.bold;
        if (!matches) {
            throw std::logic_error("unknown-text request differs from Source record: text_units=" +
                                   std::to_string(request.text.size()) + ", variant=" +
                                   request.variant + ", family=" + request.font.family +
                                   ", italic=" + (request.font.italic ? "true" : "false") +
                                   ", bold=" + (request.font.bold ? "true" : "false"));
        }
        return {expected.w, expected.h, expected.d};
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreakVisitor(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

std::vector<mml::MmlNode *> fixtureNodes(mml::MmlNode &root,
                                         std::span<mathml_scene_test::Node const> nodes) {
    constexpr auto no_parent = std::numeric_limits<std::size_t>::max();
    std::vector<mml::MmlNode *> result(nodes.size());
    std::size_t rootIndex = no_parent;
    for (std::size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].parent == no_parent) {
            if (rootIndex != no_parent)
                throw std::logic_error("table profile fixture contains multiple roots");
            rootIndex = i;
        }
    if (rootIndex == no_parent)
        throw std::logic_error("table profile fixture has no root");
    result[rootIndex] = &root;

    for (std::size_t parentIndex = 0; parentIndex < nodes.size(); ++parentIndex) {
        if (!result[parentIndex])
            continue;
        const auto children = result[parentIndex]->children();
        std::size_t childOffset = 0;
        for (std::size_t childIndex = 0; childIndex < nodes.size(); ++childIndex) {
            if (nodes[childIndex].parent != parentIndex)
                continue;
            if (childOffset >= children.size())
                throw std::logic_error("table profile fixture child mapping is inconsistent");
            result[childIndex] = children[childOffset++];
        }
        if (childOffset != children.size())
            throw std::logic_error("table profile fixture omitted a constructed child");
    }
    return result;
}

void compareBBox(folia::math::BBox const &actual, TableProfileBBox const &expected,
                 std::string_view caseName, std::string_view owner) {
    expect(close(actual.w, expected.w)) << caseName << ": " << owner << " bbox.w";
    expect(close(actual.h, expected.h)) << caseName << ": " << owner << " bbox.h";
    expect(close(actual.d, expected.d)) << caseName << ": " << owner << " bbox.d";
    expect(close(actual.scale, expected.scale)) << caseName << ": " << owner << " bbox.scale";
    expect(close(actual.rscale, expected.rscale)) << caseName << ": " << owner << " bbox.rscale";
    expect(close(actual.L, expected.L)) << caseName << ": " << owner << " bbox.L";
    expect(close(actual.R, expected.R)) << caseName << ": " << owner << " bbox.R";
}

void compareVector(std::span<double const> actual, std::span<double const> expected,
                   std::string_view caseName, std::string_view field) {
    expect(actual.size() == expected.size()) << caseName << ": " << field << " count";
    const auto count = std::min(actual.size(), expected.size());
    for (std::size_t i = 0; i < count; ++i)
        expect(close(actual[i], expected[i])) << caseName << ": " << field << '[' << i << ']';
}

svg::SvgWriteOptions writeOptions(TableProfileSourceCase const &source,
                                  folia::math::fonts::NewcmFontDataBundle const &bundle) {
    svg::SvgWriteOptions options;
    options.font_resources = &bundle.svg_resources;
    if (source.font_cache == "local") {
        options.font_cache = svg::FontCacheMode::local;
        options.local_cache_prefixes.reserve(source.local_cache_prefixes.size());
        for (const auto prefix : source.local_cache_prefixes)
            options.local_cache_prefixes.emplace_back(prefix);
    } else if (source.font_cache == "none") {
        options.font_cache = svg::FontCacheMode::none;
    } else {
        throw std::logic_error("unknown table profile font-cache mode");
    }
    return options;
}

struct CapturedScene {
    scene::Scene value;
    std::string before_retirement;
};

CapturedScene captureTableProfile(TableProfileSourceCase const &source,
                                  folia::math::fonts::NewcmFontDataBundle const &bundle) {
    mml::MmlDocument document;
    auto &root = mathml_scene_test::constructRoot(document, source.nodes, source.attributes);
    auto nodes = fixtureNodes(root, source.nodes);
    document.setRoot(root);
    root.setInheritedAttributes();
    root.setTeXclass(nullptr);

    common::ExtendedMetrics metrics{source.em, source.ex,      source.width,
                                    1,         "MathJaxNewcm", source.display};
    common::CommonOptions options;
    options.displayOverflow = "linebreak";
    options.linebreaks.inlineBreaks = true;
    SourceMeasurement measurement(source.measurements);
    common::CommonOutputJaxConfig config{metrics, std::move(options), bundle.fonts, "MathJaxNewcm"};

    common::LayoutJob job(document, config, measurement, svg::SvgWrapperFactory::defaultNodes(),
                          &makeLinebreakVisitor);
    auto &rootWrapper = job.build();
    auto value = job.jax().buildScene(rootWrapper);
    compareBBox(rootWrapper.getBBox(), source.root_bbox, source.name, "root");
    expect(measurement.next == source.measurements.size())
        << source.name << ": all observed Source measurement requests were consumed";

    const auto sourceTableCount = static_cast<std::size_t>(std::ranges::count_if(
        source.nodes, [](auto const &entry) { return entry.kind == "mtable"; }));
    expect(source.tables.size() == sourceTableCount)
        << source.name << ": every compiled mtable has a recorded Source profile";

    for (auto const &expected : source.tables) {
        expect(expected.node_index < nodes.size()) << source.name << ": table node index is valid";
        if (expected.node_index >= nodes.size())
            continue;
        auto *tableNode = nodes[expected.node_index];
        expect(tableNode != nullptr) << source.name << ": table node mapping exists";
        if (!tableNode)
            continue;
        expect(tableNode->kind() == mml::NodeKind::mtable)
            << source.name << ": indexed node is an mtable";
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(*tableNode));
        expect(table != nullptr) << source.name << ": native CommonMtable wrapper exists";
        if (!table)
            continue;

        const auto &data = table->getTableData();
        compareBBox(table->getBBox(), expected.bbox, source.name, "table");
        expect(table->numRows == expected.rows) << source.name << ": table row count";
        expect(table->numCols == expected.cols) << source.name << ": table column count";
        expect(table->hasLabels == expected.has_labels) << source.name << ": table labels flag";
        expect(table->isTop == expected.is_top) << source.name << ": table top flag";
        expect(close(table->pWidth, expected.pwidth)) << source.name << ": table pWidth";
        expect(close(data.L, expected.label_width)) << source.name << ": TableData label width";
        compareVector(data.H, expected.H, source.name, "TableData.H");
        compareVector(data.D, expected.D, source.name, "TableData.D");
        compareVector(data.W, expected.W, source.name, "TableData.W");
        compareVector(data.NH, expected.NH, source.name, "TableData.NH");
        compareVector(data.ND, expected.ND, source.name, "TableData.ND");
    }

    auto optionsForWrite = writeOptions(source, bundle);
    auto serialized = svg::write(value, optionsForWrite);
    const auto expectedSvg = source_svg_paint::materialize(source.svg);
    if (serialized != expectedSvg)
        reportDifference(source.name, expectedSvg, serialized);
    expect(serialized == expectedSvg) << source.name << ": complete Source SVG before retirement";
    return {std::move(value), std::move(serialized)};
}
} // namespace

suite native_table_profile_source = [] {
    "pinned_MathML_table_profile_matches_layout_cache_modes_and_request_retirement"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        expect(std::size(sourceTableProfileCases) == 14u)
            << "Source profile contains seven representative cases in none/local modes";
        for (const auto &source : sourceTableProfileCases) {
            expect(source.error_name.empty())
                << source.name << ": pinned Source compilation succeeded (" << source.error_name
                << ": " << source.error_message << ')';
            if (source.font_cache == "local")
                expect(!source.local_cache_prefixes.empty())
                    << source.name << ": local mode uses actual resolved Source prefixes";
            else
                expect(source.local_cache_prefixes.empty())
                    << source.name << ": none mode does not carry local cache prefixes";
            CapturedScene captured = [&] {
                try {
                    return captureTableProfile(source, bundle);
                } catch (std::exception const &error) {
                    expect(false) << source.name << ": native pipeline threw: " << error.what();
                    throw;
                }
            }();
            const auto afterRetirement = svg::write(captured.value, writeOptions(source, bundle));
            svg_paint_artifacts::capture("table-profile", source.name, source.svg, afterRetirement,
                                         source.em, source.ex, source.width);
            const auto expectedSvg = source_svg_paint::materialize(source.svg);
            if (afterRetirement != expectedSvg)
                reportDifference(source.name, expectedSvg, afterRetirement);
            expect(afterRetirement == expectedSvg)
                << source.name << ": complete Source SVG after request retirement";
            expect(captured.before_retirement == afterRetirement)
                << source.name << ": Scene remains serializable after request retirement";
        }
    };
};
