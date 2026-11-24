#include "math/support/source_svg_paint.hpp"
#include "math/support/svg_paint_artifacts.hpp"
#include "support/folia_test.hpp"
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.svg.writer;
#include "math/support/mathml_scene.hpp"

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

using MathmlAttributeKind = mathml_scene_test::AttributeKind;
using MathmlSceneAttribute = mathml_scene_test::Attribute;
using MathmlSceneNode = mathml_scene_test::Node;
using MathmlSceneMetrics = mathml_scene_test::Metrics;
struct MathmlSceneBBox {
    double w{}, h{}, d{}, L{}, R{};
};
struct MathmlTableSceneCase {
    std::string_view name;
    MathmlSceneMetrics metrics;
    std::span<MathmlSceneNode const> nodes;
    std::span<MathmlSceneAttribute const> attributes;
    std::span<double const> H, D, W;
    double L{};
    std::span<double const> row_rscales;
    std::size_t dotted_lines{};
    std::size_t dotted_frames{};
    MathmlSceneBBox bbox;
    std::string_view source_svg;
};
#include "math/fixtures/common-source/table-mathml-scene.inc"

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

suite common_table_native = [] {
    "factory_created_table_keeps_rows_cells_dimensions_spacing_and_frame"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument doc;
        mml::AttributeSet s1;
        s1.set(u"width", u"1.25em");
        s1.set(u"height", u".8em");
        s1.set(u"depth", u".2em");
        auto &a = doc.factory().make(mml::NodeKind::mspace, std::move(s1));
        mml::AttributeSet s2;
        s2.set(u"width", u".7em");
        s2.set(u"height", u".3em");
        s2.set(u"depth", u".1em");
        auto &b = doc.factory().make(mml::NodeKind::mspace, std::move(s2));
        auto &inferredA = inferredCellContent(doc, a);
        auto &inferredB = inferredCellContent(doc, b);
        mml::MmlNode *ca[]{&inferredA};
        mml::MmlNode *cb[]{&inferredB};
        auto &cellA = doc.factory().make(mml::NodeKind::mtd, {}, ca);
        auto &cellB = doc.factory().make(mml::NodeKind::mtd, {}, cb);
        mml::MmlNode *rowCells[]{&cellA, &cellB};
        auto &row = doc.factory().make(mml::NodeKind::mtr, {}, rowCells);
        mml::AttributeSet s3;
        s3.set(u"width", u".7em");
        s3.set(u"height", u".3em");
        s3.set(u"depth", u".1em");
        auto &c = doc.factory().make(mml::NodeKind::mspace, std::move(s3));
        mml::AttributeSet s4;
        s4.set(u"width", u"1.25em");
        s4.set(u"height", u".8em");
        s4.set(u"depth", u".2em");
        auto &d = doc.factory().make(mml::NodeKind::mspace, std::move(s4));
        auto &inferredC = inferredCellContent(doc, c);
        auto &inferredD = inferredCellContent(doc, d);
        mml::MmlNode *cc[]{&inferredC};
        mml::MmlNode *cd[]{&inferredD};
        auto &cellC = doc.factory().make(mml::NodeKind::mtd, {}, cc);
        auto &cellD = doc.factory().make(mml::NodeKind::mtd, {}, cd);
        mml::MmlNode *rowCells2[]{&cellC, &cellD};
        auto &row2 = doc.factory().make(mml::NodeKind::mtr, {}, rowCells2);
        mml::MmlNode *rows[]{&row, &row2};
        mml::AttributeSet attrs;
        attrs.set(u"columnspacing", u".4em");
        attrs.set(u"rowspacing", u".3em");
        attrs.set(u"columnlines", u"solid");
        attrs.set(u"rowlines", u"solid");
        attrs.set(u"frame", u"solid");
        attrs.set(u"framespacing", u".5em .6em");
        attrs.set(u"width", u"auto");
        attrs.set(u"align", u"axis");
        auto &tableNode = doc.factory().make(mml::NodeKind::mtable, std::move(attrs), rows);
        expect(tableNode.properties().table.use_height.value_or(false));
        auto &mathNode = mathTableRoot(doc, tableNode);
        doc.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", true}, sourceOptions(), bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(doc, config, measurement, tableCatalog(), makeLinebreaks);
        auto &root = job.build();
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
        auto *rowWrapper = dynamic_cast<common::CommonMtr *>(job.jax().wrapperFor(row));
        auto *firstCell = dynamic_cast<common::CommonMtd *>(job.jax().wrapperFor(cellA));
        expect(table != nullptr);
        expect(rowWrapper != nullptr);
        expect(firstCell != nullptr);
        if (!table || !rowWrapper || !firstCell)
            return;
        expect(root.node == &mathNode);
        expect(table->numRows == 2_i);
        expect(table->numCols == 2_i);
        expect(table->tableRows().size() == 2_i);
        expect(rowWrapper->numCells() == 2_i);
        expect(rowWrapper->getChild(0) == job.jax().wrapperFor(cellA));
        expect(rowWrapper->getChildBBoxes().size() == 2_i);
        const auto &dimensions = table->getTableData();
        expect(dimensions.W.size() == 2_i);
        expect(dimensions.H.size() == 2_i);
        expect(dimensions.D.size() == 2_i);
        expect(eq(dimensions.W[0], 1.25));
        expect(eq(dimensions.W[1], 1.25));
        expect(eq(dimensions.H[0], .8));
        expect(eq(dimensions.H[1], .8));
        expect(eq(dimensions.D[0], .25));
        expect(eq(dimensions.D[1], .25));
        expect(table->cSpace.size() == 1_i);
        expect(eq(table->cSpace[0], .4));
        expect(table->rSpace.size() == 1_i);
        expect(eq(table->rSpace[0], .3));
        expect(table->frame);
        expect(table->fframe);
        expect(eq(table->fLine, .07));
        expect(eq(table->fSpace[0], .5));
        expect(eq(table->fSpace[1], .6));
        expect(table->cLines.size() == 1_i);
        expect(eq(table->cLines[0], .07));
        expect(table->rLines.size() == 1_i);
        expect(eq(table->rLines[0], .07));
        expect(eq(firstCell->getWrapWidth(0), dimensions.W[0]));
        expect(!firstCell->fixesPWidth());
        expect(job.jax().table() == table);
        expect(eq(table->getBBox().w, 4.109999999999999));
        expect(eq(table->getBBox().h, 2.155));
        expect(eq(table->getBBox().d, 1.6549999999999998));
    };

    "table_width_modes_match_locked_NewCM_Source_values"_test = [] {
        struct WidthCase {
            std::u16string width, columns;
            bool equal;
            double first, second, tableWidth;
        };
        const WidthCase cases[]{{u"4em", u"auto fit", false, 1.25, 1.25, 4.5},
                                {u"75%", u"30% auto", false, 15.312, 35.728, 53.04},
                                {u"auto", u"", true, 1.25, 1.25, 4.5}};
        auto bundle = folia::math::fonts::makeNewcmFontData();
        for (const auto &expected : cases) {
            RejectMeasurement measurement;
            mml::MmlDocument doc;
            const auto makeCell = [&](double width, double height, double depth) -> mml::MmlNode & {
                mml::AttributeSet spaceAttrs;
                spaceAttrs.set(u"width", width == 1.25 ? u"1.25em" : u".7em");
                spaceAttrs.set(u"height", height == .8 ? u".8em" : u".3em");
                spaceAttrs.set(u"depth", depth == .2 ? u".2em" : u".1em");
                auto &space = doc.factory().make(mml::NodeKind::mspace, std::move(spaceAttrs));
                auto &inferred = inferredCellContent(doc, space);
                mml::MmlNode *children[]{&inferred};
                return doc.factory().make(mml::NodeKind::mtd, {}, children);
            };
            auto &a = makeCell(1.25, .8, .2);
            auto &b = makeCell(.7, .3, .1);
            auto &c = makeCell(.7, .3, .1);
            auto &d = makeCell(1.25, .8, .2);
            mml::MmlNode *firstCells[]{&a, &b};
            mml::MmlNode *secondCells[]{&c, &d};
            auto &firstRow = doc.factory().make(mml::NodeKind::mtr, {}, firstCells);
            auto &secondRow = doc.factory().make(mml::NodeKind::mtr, {}, secondCells);
            mml::MmlNode *rows[]{&firstRow, &secondRow};
            mml::AttributeSet tableAttrs;
            setArrayDefaults(tableAttrs, u"center center");
            tableAttrs.set(u"width", expected.width);
            if (!expected.columns.empty())
                tableAttrs.set(u"columnwidth", expected.columns);
            if (expected.equal)
                tableAttrs.set(u"equalcolumns", true);
            auto &tableNode =
                doc.factory().make(mml::NodeKind::mtable, std::move(tableAttrs), rows);
            expect(tableNode.properties().table.use_height.value_or(false));
            auto &mathNode = mathTableRoot(doc, tableNode);
            doc.setRoot(mathNode);
            mathNode.setInheritedAttributes();
            common::CommonOutputJaxConfig config{{16, 8, 1280, 1, "MathJaxNewcm", true},
                                                 sourceOptions(),
                                                 bundle.fonts,
                                                 "MathJaxNewcm"};
            common::LayoutJob job(doc, config, measurement, tableCatalog(), makeLinebreaks);
            auto& root = job.build();
            (void)root.getBBox();
            auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
            expect(table != nullptr);
            if (!table)
                continue;
            const auto &data = table->getTableData();
            expect(eq(data.W[0], 1.25));
            expect(eq(data.W[1], 1.25));
            const auto *first = std::get_if<double>(&table->cWidths[0]);
            const auto *second = std::get_if<double>(&table->cWidths[1]);
            expect(first != nullptr);
            expect(second != nullptr);
            if (first)
                expect(eq(*first, expected.first));
            if (second)
                expect(eq(*second, expected.second));
            expect(eq(table->getBBox().w, expected.tableWidth));
        }
    };

    "labeled_rows_keep_the_label_slot_out_of_column_metrics"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument doc;
        const auto makeMeasuredCell = [&](std::u16string width, std::u16string height,
                                          std::u16string depth) -> mml::MmlNode & {
            mml::AttributeSet attrs;
            attrs.set(u"width", std::move(width));
            attrs.set(u"height", std::move(height));
            attrs.set(u"depth", std::move(depth));
            auto &space = doc.factory().make(mml::NodeKind::mspace, std::move(attrs));
            auto &inferred = inferredCellContent(doc, space);
            mml::MmlNode *children[]{&inferred};
            return doc.factory().make(mml::NodeKind::mtd, {}, children);
        };
        auto &labelA = makeMeasuredCell(u"1.528em", u".748em", u".248em");
        auto &a = makeMeasuredCell(u".572em", u".442em", u".011em");
        auto &b = makeMeasuredCell(u"1.8335555555555554em", u".666em", u"0em");
        mml::MmlNode *firstCells[]{&labelA, &a, &b};
        auto &first = doc.factory().make(mml::NodeKind::mlabeledtr, {}, firstCells);
        auto &labelB = makeMeasuredCell(u"1.486em", u".748em", u".248em");
        auto &c = makeMeasuredCell(u".49em", u".442em", u".205em");
        auto &d = makeMeasuredCell(u"1.8335555555555554em", u".666em", u"0em");
        mml::MmlNode *secondCells[]{&labelB, &c, &d};
        auto &second = doc.factory().make(mml::NodeKind::mlabeledtr, {}, secondCells);
        mml::MmlNode *rows[]{&first, &second};
        mml::AttributeSet tableAttrs;
        tableAttrs.set(u"displaystyle", true);
        tableAttrs.set(u"columnalign", u"right left");
        tableAttrs.set(u"columnspacing", u"0em");
        tableAttrs.set(u"rowspacing", u"3pt");
        tableAttrs.set(u"data-break-align", u"bottom top");
        auto &tableNode = doc.factory().make(mml::NodeKind::mtable, std::move(tableAttrs), rows);
        expect(tableNode.properties().table.use_height.value_or(false));
        auto &mathNode = mathTableRoot(doc, tableNode);
        doc.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", true}, sourceOptions(), bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(doc, config, measurement, tableCatalog(), makeLinebreaks);
        (void)job.build();
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
        auto *row = dynamic_cast<common::CommonMlabeledtr *>(job.jax().wrapperFor(first));
        expect(table != nullptr);
        expect(row != nullptr);
        if (!table || !row)
            return;
        expect(table->hasLabels);
        expect(table->numRows == 2_u);
        expect(table->numCols == 2_u);
        expect(row->labeled());
        expect(row->numCells() == 2_u);
        expect(row->tableCells().size() == 2_u);
        expect(eq(table->getTableData().L, 1.528));
        expect(eq(table->getTableData().W[0], .572));
        expect(eq(table->getTableData().W[1], 1.8335555555555554));
    };

    "fixed_column_break_matches_locked_text_break_measurements"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument doc;
        auto &text = doc.factory().make(mml::NodeKind::text);
        text.setText(u"x x");
        mml::MmlNode *textChildren[]{&text};
        auto &content = doc.factory().make(mml::NodeKind::mtext, {}, textChildren);
        auto &inferred = inferredCellContent(doc, content);
        mml::MmlNode *cellChildren[]{&inferred};
        auto &cell = doc.factory().make(mml::NodeKind::mtd, {}, cellChildren);
        mml::MmlNode *rowCells[]{&cell};
        auto &row = doc.factory().make(mml::NodeKind::mtr, {}, rowCells);
        mml::MmlNode *rows[]{&row};
        mml::AttributeSet tableAttrs;
        setArrayDefaults(tableAttrs, u"center");
        tableAttrs.set(u"width", u"auto");
        tableAttrs.set(u"columnwidth", u".5em");
        auto &tableNode = doc.factory().make(mml::NodeKind::mtable, std::move(tableAttrs), rows);
        expect(tableNode.properties().table.use_height.value_or(false));
        mml::AttributeSet mathAttrs;
        mathAttrs.set(u"overflow", u"linebreak");
        auto &mathNode = mathTableRoot(doc, tableNode, std::move(mathAttrs));
        doc.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonOptions options;
        options.linebreaks.inlineBreaks = true;
        options.displayOverflow = "linebreak";
        common::CommonWrapperCatalog catalog{
            reg<common::CommonMath>(mml::NodeKind::math),
            reg<common::CommonMtable>(mml::NodeKind::mtable),
            reg<common::CommonMtr>(mml::NodeKind::mtr),
            reg<common::CommonMtd>(mml::NodeKind::mtd),
            reg<common::CommonMtext>(mml::NodeKind::mtext),
            reg<common::CommonTextNode>(mml::NodeKind::text),
            reg<common::CommonMrow>(mml::NodeKind::mrow),
            reg<common::CommonInferredMrow>(mml::NodeKind::inferred_mrow)};
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", true}, options, bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(doc, config, measurement, std::move(catalog), makeLinebreakVisitor);
        (void)job.build();
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
        auto *rowWrapper = dynamic_cast<common::CommonMtr *>(job.jax().wrapperFor(row));
        expect(table != nullptr);
        expect(rowWrapper != nullptr);
        if (!table || !rowWrapper)
            return;
        auto *cellWrapper = rowWrapper->getChild(0);
        expect(cellWrapper != nullptr);
        if (!cellWrapper || cellWrapper->childNodes.empty())
            return;
        auto *line = cellWrapper->childNodes.front();
        expect(line->breakCount() == 1_u);
        expect(eq(cellWrapper->getBBox().w, .528));
        expect(eq(cellWrapper->getBBox().h, .75));
        expect(eq(cellWrapper->getBBox().d, 1.45));
        expect(eq(std::get<double>(table->cWidths[0]), .528));
        expect(eq(table->getBBox().w, 1.528));
        expect(eq(table->getBBox().h, 1.475));
        expect(eq(table->getBBox().d, .9750000000000001));
    };

    "table_stretches_real_NewCM_delimiters_to_the_locked_matrix_dimensions"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument doc;
        const auto makeMi = [&](std::u16string value) -> mml::MmlNode & {
            auto &text = doc.factory().make(mml::NodeKind::text);
            text.setText(std::move(value));
            mml::MmlNode *children[]{&text};
            return doc.factory().make(mml::NodeKind::mi, {}, children);
        };
        const auto makeFrac = [&](std::u16string numerator,
                                  std::u16string denominator) -> mml::MmlNode & {
            auto &n = makeMi(std::move(numerator));
            auto &d = makeMi(std::move(denominator));
            mml::MmlNode *children[]{&n, &d};
            return doc.factory().make(mml::NodeKind::mfrac, {}, children);
        };
        const auto makeFence = [&](std::u16string glyph, std::u16string form) -> mml::MmlNode & {
            auto &text = doc.factory().make(mml::NodeKind::text);
            text.setText(std::move(glyph));
            mml::MmlNode *children[]{&text};
            mml::AttributeSet attrs;
            attrs.set(u"stretchy", true);
            attrs.set(u"symmetric", true);
            attrs.set(u"fence", true);
            attrs.set(u"form", std::move(form));
            return doc.factory().make(mml::NodeKind::mo, std::move(attrs), children);
        };
        auto &left = makeFence(u"(", u"prefix");
        auto &numerator = makeFrac(u"x", u"y");
        auto &right = makeFence(u")", u"postfix");
        left.properties().tex.explicit_tex_class = mml::TexClass::open;
        right.properties().tex.explicit_tex_class = mml::TexClass::close;
        mml::MmlNode *bracketChildren[]{&left, &numerator, &right};
        auto &bracketed = doc.factory().make(mml::NodeKind::mrow, {}, bracketChildren);
        bracketed.properties().tex.open = u"(";
        bracketed.properties().tex.close = u")";
        bracketed.properties().tex.explicit_tex_class = mml::TexClass::inner;
        auto &other = makeFrac(u"a", u"b");
        auto &bracketContent = inferredCellContent(doc, bracketed);
        auto &otherContent = inferredCellContent(doc, other);
        mml::MmlNode *firstCells[]{&bracketContent, &otherContent};
        auto &row = doc.factory().make(mml::NodeKind::mtr, {}, firstCells);
        mml::MmlNode *rows[]{&row};
        mml::AttributeSet tableAttrs;
        tableAttrs.set(u"columnspacing", u"1em");
        tableAttrs.set(u"rowspacing", u"4pt");
        auto &tableNode = doc.factory().make(mml::NodeKind::mtable, std::move(tableAttrs), rows);
        expect(tableNode.properties().table.use_height.value_or(false));
        auto &mathNode = mathTableRoot(doc, tableNode);
        doc.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonWrapperCatalog catalog{
            reg<common::CommonMath>(mml::NodeKind::math),
            reg<common::CommonMtable>(mml::NodeKind::mtable),
            reg<common::CommonMtr>(mml::NodeKind::mtr),
            reg<common::CommonMtd>(mml::NodeKind::mtd),
            reg<common::CommonMrow>(mml::NodeKind::mrow),
            reg<common::CommonMi>(mml::NodeKind::mi),
            reg<common::CommonMfrac>(mml::NodeKind::mfrac),
            reg<common::CommonMo>(mml::NodeKind::mo),
            reg<common::CommonTextNode>(mml::NodeKind::text),
            reg<common::CommonInferredMrow>(mml::NodeKind::inferred_mrow)};
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", true}, sourceOptions(), bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(doc, config, measurement, std::move(catalog), makeLinebreaks);
        (void)job.build();
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
        auto *leftWrapper = dynamic_cast<common::CommonMo *>(job.jax().wrapperFor(left));
        auto *rightWrapper = dynamic_cast<common::CommonMo *>(job.jax().wrapperFor(right));
        expect(table != nullptr);
        expect(leftWrapper != nullptr);
        expect(rightWrapper != nullptr);
        if (!table || !leftWrapper || !rightWrapper)
            return;
        expect(leftWrapper->canStretch(common::StretchDirection::vertical));
        expect(rightWrapper->canStretch(common::StretchDirection::vertical));
        for (auto* delimiter : {leftWrapper, rightWrapper}) {
            expect(delimiter->size.has_value());
            if (delimiter->size) {
                auto* selected = std::get_if<common::FixedStretchSize>(&*delimiter->size);
                expect(selected != nullptr);
                if (selected)
                    expect(selected->index == 3_u);
            }
            expect(delimiter->variant == "-size3");
        }
        expect(eq(table->getTableData().H[0], .972));
        expect(eq(table->getTableData().D[0], .4899568901432422));
        expect(eq(table->getTableData().W[0], 1.8904650788387052));
        expect(eq(table->getTableData().W[1], .8140594872476836));
        expect(eq(table->getBBox().w, 3.7045245660863886));
        expect(eq(table->getBBox().h, .980978445071621));
        expect(eq(table->getBBox().d, .480978445071621));
    };

    "table_breaks_an_operator_expression_at_the_locked_long_case_width"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument doc;
        std::vector<mml::MmlNode *> expression;
        const auto makeToken = [&](mml::NodeKind kind, std::u16string textValue) -> mml::MmlNode & {
            auto &text = doc.factory().make(mml::NodeKind::text);
            text.setText(std::move(textValue));
            mml::MmlNode *children[]{&text};
            return doc.factory().make(kind, {}, children);
        };
        for (char16_t ch : std::u16string_view(u"abcdefghij"))
            expression.push_back(&makeToken(mml::NodeKind::mi, std::u16string(1, ch)));
        expression.push_back(&makeToken(mml::NodeKind::mo, u"+"));
        for (char16_t ch : std::u16string_view(u"klmnopqrst"))
            expression.push_back(&makeToken(mml::NodeKind::mi, std::u16string(1, ch)));
        auto &rowContent = doc.factory().make(mml::NodeKind::inferred_mrow, {}, expression);
        mml::MmlNode *cellChildren[]{&rowContent};
        auto &cell = doc.factory().make(mml::NodeKind::mtd, {}, cellChildren);
        mml::MmlNode *rowCells[]{&cell};
        auto &row = doc.factory().make(mml::NodeKind::mtr, {}, rowCells);
        mml::MmlNode *rows[]{&row};
        mml::AttributeSet tableAttrs;
        setArrayDefaults(tableAttrs, u"center");
        tableAttrs.set(u"width", u"auto");
        tableAttrs.set(u"columnwidth", u"2em");
        auto &tableNode = doc.factory().make(mml::NodeKind::mtable, std::move(tableAttrs), rows);
        expect(tableNode.properties().table.use_height.value_or(false));
        mml::AttributeSet mathAttrs;
        mathAttrs.set(u"overflow", u"linebreak");
        auto &mathNode = mathTableRoot(doc, tableNode, std::move(mathAttrs));
        doc.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonOptions options;
        options.linebreaks.inlineBreaks = true;
        options.displayOverflow = "linebreak";
        common::CommonWrapperCatalog catalog{
            reg<common::CommonMath>(mml::NodeKind::math),
            reg<common::CommonMtable>(mml::NodeKind::mtable),
            reg<common::CommonMtr>(mml::NodeKind::mtr),
            reg<common::CommonMtd>(mml::NodeKind::mtd),
            reg<common::CommonMrow>(mml::NodeKind::mrow),
            reg<common::CommonInferredMrow>(mml::NodeKind::inferred_mrow),
            reg<common::CommonMi>(mml::NodeKind::mi),
            reg<common::CommonMo>(mml::NodeKind::mo),
            reg<common::CommonTextNode>(mml::NodeKind::text)};
        common::CommonOutputJaxConfig config{
            {16, 8, 100, 1, "MathJaxNewcm", true}, options, bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(doc, config, measurement, std::move(catalog), makeLinebreakVisitor);
        (void)job.build();
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
        auto *rowWrapper = dynamic_cast<common::CommonMtr *>(job.jax().wrapperFor(row));
        expect(table != nullptr);
        expect(rowWrapper != nullptr);
        if (!table || !rowWrapper)
            return;
        auto *cellWrapper = rowWrapper->getChild(0);
        expect(cellWrapper != nullptr);
        if (!cellWrapper || cellWrapper->childNodes.empty())
            return;
        expect(cellWrapper->childNodes.front()->breakCount() == 1_u);
        expect(eq(cellWrapper->getBBox().w, 6.018222222222222));
        expect(eq(cellWrapper->getBBox().h, .75));
        expect(eq(cellWrapper->getBBox().d, 1.45));
        expect(eq(table->getTableData().W[0], 10.979444444444445));
        expect(eq(table->cWidths[0].index() == 1 ? std::get<double>(table->cWidths[0]) : 0,
                  6.018222222222222));
        expect(eq(table->getBBox().w, 7.018222222222222));
        expect(eq(table->getBBox().h, 1.475));
        expect(eq(table->getBBox().d, .9750000000000001));
    };

    "direct_MathML_table_attributes_match_pinned_Source_SVG"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        for (const auto &test : sourceTableCases) {
            try {
                verifySourceMathmlTable(test, bundle);
            } catch (const std::exception &error) {
                expect(false) << "case " << test.name << " threw: " << error.what();
            }
        }
    };
};
