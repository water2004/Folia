#pragma once

#include "math/support/source_svg_paint.hpp"
#include "math/support/svg_paint_artifacts.hpp"
#include "support/folia_test.hpp"
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.svg.writer;
#include "math/support/mathml_observations.hpp"

using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

namespace {
class RejectMeasurement final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("table fixture unexpectedly requested metrics");
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("table fixture unexpectedly measured text");
    }
};

class RejectLinebreaks final : public common::Linebreaks {
  public:
    using Linebreaks::Linebreaks;

    void breakToWidth(common::CommonWrapper &, double) override {
        throw std::logic_error("table fixture unexpectedly line-broke");
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreaks(common::CommonWrapperFactory &f) {
    return std::make_unique<RejectLinebreaks>(f);
}

std::unique_ptr<common::Linebreaks> makeLinebreakVisitor(common::CommonWrapperFactory &f) {
    return std::make_unique<common::LinebreakVisitor>(f);
}

mml::MmlNode &inferredCellContent(mml::MmlDocument &doc, mml::MmlNode &content) {
    mml::MmlNode *children[]{&content};
    return doc.factory().make(mml::NodeKind::inferred_mrow, {}, children);
}

template <class W> common::CommonWrapperRegistration reg(mml::NodeKind kind) {
    using Args = common::WrapperBuildArguments;
    using Creator = folia::math::tree::FactoryClass<common::CommonWrapper, Args>;
    return {kind,
            Creator{[](Args const &a) -> std::unique_ptr<common::CommonWrapper> {
                        return std::make_unique<W>(a.factory, a.node, a.parent);
                    },
                    [](common::CommonWrapper const &w) {
                        return dynamic_cast<W const *>(&w) != nullptr;
                    }},
            common::CommonWrapperClassData::commonDefaults()};
}

common::CommonWrapperCatalog tableCatalog() {
    using K = mml::NodeKind;
    return {reg<common::CommonMath>(K::math),
            reg<common::CommonMtable>(K::mtable),
            reg<common::CommonMtr>(K::mtr),
            reg<common::CommonMlabeledtr>(K::mlabeledtr),
            reg<common::CommonMtd>(K::mtd),
            reg<common::CommonMspace>(K::mspace),
            reg<common::CommonInferredMrow>(K::inferred_mrow)};
}

common::CommonOptions sourceOptions() {
    common::CommonOptions options;
    options.displayOverflow = "linebreak";
    return options;
}

void setArrayDefaults(mml::AttributeSet &attrs, std::u16string_view columnAlign) {
    attrs.set(u"columnalign", std::u16string(columnAlign));
    attrs.set(u"columnspacing", u"1em");
    attrs.set(u"rowspacing", u"4pt");
    attrs.set(u"data-frame-styles", u"");
    attrs.set(u"framespacing", u".5em .125em");
}

mml::MmlNode &mathTableRoot(mml::MmlDocument &doc, mml::MmlNode &table,
                            mml::AttributeSet attrs = {}) {
    attrs.set(u"display", u"block");
    auto &inferred = inferredCellContent(doc, table);
    mml::MmlNode *children[]{&inferred};
    return doc.factory().make(mml::NodeKind::math, std::move(attrs), children);
}

#include "math/support/table_observations.hpp"

mml::MmlNode &constructSourceMathml(mml::MmlDocument &doc, MathmlTableSceneCase const &source,
                                    mml::MmlNode **tableOut) {
    auto &root = mathml_scene_test::constructRoot(doc, source.nodes, source.attributes);
    *tableOut = nullptr;
    std::vector<mml::MmlNode *> pending{&root};
    while (!pending.empty()) {
        auto *node = pending.back();
        pending.pop_back();
        if (node->kind() == mml::NodeKind::mtable) {
            if (*tableOut)
                throw std::logic_error("Source fixture must contain one mtable");
            *tableOut = node;
        }
        for (auto *child : node->children())
            pending.push_back(child);
    }
    return root;
}

std::vector<scene::Primitive const *> tableScenePrimitives(scene::Scene const &value) {
    std::vector<scene::Primitive const *> result;
    const auto visit = [&](auto &&self, scene::Group const &group) -> void {
        for (const auto &entry : group.entries)
            if (const auto *primitive = std::get_if<scene::Primitive>(&entry))
                result.push_back(primitive);
            else if (const auto *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry))
                self(self, **child);
    };
    visit(visit, value.root());
    return result;
}

void compareTableValues(std::span<double const> actual, std::span<double const> expected,
                        std::string_view caseName, std::string_view field) {
    expect(actual.size() == expected.size()) << "case " << caseName << ": " << field << " size";
    for (std::size_t i = 0; i < std::min(actual.size(), expected.size()); ++i)
        expect(eq(actual[i], expected[i]))
            << "case " << caseName << ": " << field << '[' << i << ']';
}

void reportTableSvgDifference(std::string_view name, std::string_view expected,
                              std::string_view actual) {
    std::size_t offset = 0;
    while (offset < expected.size() && offset < actual.size() && expected[offset] == actual[offset])
        ++offset;
    constexpr std::size_t radius = 48;
    const auto snippet = [&](std::string_view value) {
        const auto begin = std::min(value.size(), offset > radius ? offset - radius : 0);
        return value.substr(begin, std::min(value.size() - begin, 2 * radius));
    };
    std::cerr << "table MathML SVG mismatch for " << name << " at byte " << offset
              << " (expected size " << expected.size() << ", actual size " << actual.size() << ")\n"
              << "expected: " << snippet(expected) << "\nactual:   " << snippet(actual) << '\n';
}

void verifySourceMathmlTable(MathmlTableSceneCase const &expected,
                             folia::math::fonts::NewcmFontDataBundle const &bundle) {
    const auto check = [&](bool value, std::string_view field) {
        expect(value) << "case " << expected.name << ": " << field;
    };
    RejectMeasurement measurement;
    mml::MmlDocument doc;
    mml::MmlNode *tableNode = nullptr;
    auto &mathNode = constructSourceMathml(doc, expected, &tableNode);
    check(tableNode != nullptr, "fixture contains one mtable");
    if (!tableNode)
        return;
    doc.setRoot(mathNode);
    mathNode.setInheritedAttributes();

    common::CommonOutputJaxConfig config{{expected.metrics.em, expected.metrics.ex,
                                          expected.metrics.container_width, 1, "MathJaxNewcm",
                                          expected.metrics.display},
                                         sourceOptions(),
                                         bundle.fonts,
                                         "MathJaxNewcm"};
    common::LayoutJob job(doc, config, measurement, common::CommonWrapperFactory::standardNodes(),
                          makeLinebreakVisitor);
    const auto built = job.buildScene();
    auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(*tableNode));
    check(table != nullptr, "table wrapper exists");
    if (!table)
        return;

    const auto &data = table->getTableData();
    compareTableValues(data.H, expected.H, expected.name, "H");
    compareTableValues(data.D, expected.D, expected.name, "D");
    compareTableValues(data.W, expected.W, expected.name, "W");
    check(eq(data.L, expected.L), "TableData.L");
    const auto &bbox = table->getBBox();
    check(eq(bbox.w, expected.bbox.w), "bbox.w");
    check(eq(bbox.h, expected.bbox.h), "bbox.h");
    check(eq(bbox.d, expected.bbox.d), "bbox.d");
    check(eq(bbox.L, expected.bbox.L), "bbox.L");
    check(eq(bbox.R, expected.bbox.R), "bbox.R");

    const auto rows = table->tableRows();
    check(rows.size() == expected.row_rscales.size(), "row count for rscale");
    for (std::size_t i = 0; i < std::min(rows.size(), expected.row_rscales.size()); ++i)
        check(eq(rows[i]->getBBox().rscale, expected.row_rscales[i]), "row bbox rscale");

    const auto nativeSvg = svg::write(built, {.font_cache = svg::FontCacheMode::none});
    svg_paint_artifacts::capture("table", expected.name, expected.source_svg, nativeSvg,
                                 expected.metrics.em, expected.metrics.ex,
                                 expected.metrics.container_width);
    const auto expectedSvg = source_svg_paint::materialize(expected.source_svg);
    if (nativeSvg != expectedSvg) {
        reportTableSvgDifference(expected.name, expectedSvg, nativeSvg);
        check(false, "SVG bytes match pinned Source with explicit CSS paint");
    }

    std::size_t dottedLines = 0;
    std::size_t dottedFrames = 0;
    for (const auto *primitive : tableScenePrimitives(built))
        std::visit(
            [&](auto const &shape) {
                using T = std::remove_cvref_t<decltype(shape)>;
                if constexpr (std::same_as<T, scene::Path>) {
                    if (shape.role == scene::ElementRole::table_vertical_line ||
                        shape.role == scene::ElementRole::table_horizontal_line) {
                        ++dottedLines;
                        check(shape.paint.stroke_width == .07, "dotted line width");
                        check(shape.paint.dash_array == std::vector<double>{0, .14},
                              "dotted line dash");
                        check(shape.paint.line_cap == scene::StrokeCap::round, "dotted line cap");
                    }
                } else if constexpr (std::same_as<T, scene::Rule>) {
                    if (shape.role == scene::ElementRole::table_frame) {
                        ++dottedFrames;
                        check(shape.paint.stroke_width == .07, "dotted frame width");
                        check(shape.paint.dash_array == std::vector<double>{0, .14},
                              "dotted frame dash");
                        check(shape.paint.line_cap == scene::StrokeCap::round, "dotted frame cap");
                    }
                }
            },
            *primitive);
    check(dottedLines == expected.dotted_lines, "Source dotted line count");
    check(dottedFrames == expected.dotted_frames, "Source dotted frame count");
}
} // namespace

