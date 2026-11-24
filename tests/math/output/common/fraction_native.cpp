#include "support/folia_test.hpp"
import folia.math.output.common;
import folia.math.core.mml_tree.mml_nodes.mfrac;
import folia.math.core.mml_tree.mml_nodes.mspace;
#include "math/support/layout_source_table.hpp"

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace mml = folia::math::mml;
using folia::math::BBox;
using folia::math::test::SourceRow;

namespace {
class RejectMeasurement final : public common::MeasurementTarget {
public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const&) override {
        throw std::logic_error("fraction wrapper test did not request metrics");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const&) override {
        throw std::logic_error("fraction wrapper test did not measure text");
    }
};

class RejectLinebreaks final : public common::Linebreaks {
public:
    using Linebreaks::Linebreaks;
    void breakToWidth(common::CommonWrapper&, double) override {
        throw std::logic_error("fraction wrapper test did not request line breaking");
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreaks(common::CommonWrapperFactory& factory) {
    return std::make_unique<RejectLinebreaks>(factory);
}

class FixedBox final : public common::CommonWrapper {
    BBox value_;
public:
    FixedBox(common::CommonWrapperFactory& factory, mml::MmlNode& node,
             common::CommonWrapper* parent, BBox value)
        : CommonWrapper(factory, node, parent), value_(std::move(value)) {}
protected:
    void computeBBox(BBox& target, bool) override { target = value_; }
    std::shared_ptr<common::LineBBox> computeLineBBox(std::size_t) override {
        auto line = std::make_shared<common::LineBBox>();
        static_cast<BBox&>(*line) = getBBox();
        return line;
    }
};

class FixedSlash final : public common::CommonMo {
    BBox value_;
public:
    FixedSlash(common::CommonWrapperFactory& factory, mml::MmlNode& node,
               common::CommonWrapper* parent, BBox value)
        : CommonMo(factory, node, parent), value_(std::move(value)) {}
protected:
    void computeBBox(BBox& target, bool) override { target = value_; }
};

template<class T, class... Extra>
common::CommonWrapperRegistration registration(mml::NodeKind kind, Extra... extra) {
    return {kind,
        {[extra...](common::WrapperBuildArguments const& args) {
             if constexpr (std::same_as<T, FixedBox>)
                 return std::unique_ptr<common::CommonWrapper>(std::make_unique<T>(
                     args.factory, args.node, args.parent, extra...));
             else
                 return std::unique_ptr<common::CommonWrapper>(std::make_unique<T>(
                     args.factory, args.node, args.parent, extra...));
         },
         [](common::CommonWrapper const& wrapper) { return dynamic_cast<T const*>(&wrapper) != nullptr; }},
        common::CommonWrapperClassData::commonDefaults()};
}

common::CommonWrapperCatalog makeCatalog(std::map<mml::MmlNode const*, BBox> const& boxes,
                                          BBox const& slash) {
    common::CommonWrapperRegistration fixedChildren{
        mml::NodeKind::mspace,
        {[&boxes](common::WrapperBuildArguments const& args) -> std::unique_ptr<common::CommonWrapper> {
             return std::make_unique<FixedBox>(args.factory, args.node, args.parent, boxes.at(&args.node));
         },
         [](common::CommonWrapper const& wrapper) { return dynamic_cast<FixedBox const*>(&wrapper) != nullptr; }},
        common::CommonWrapperClassData::commonDefaults()};
    return {registration<common::CommonMfrac>(mml::NodeKind::mfrac),
        std::move(fixedChildren), registration<FixedBox>(mml::NodeKind::text, BBox{}),
        registration<FixedSlash>(mml::NodeKind::mo, slash)};
}

common::FontRepository makeFonts(SourceRow const& row) {
    common::FontRepositoryBuilder builder;
    common::FontFaceData face;
    face.name = "fraction-test";
    face.cssFamilyPrefix = "MathJax_";
    face.size_variants = {"size-0"};
    face.parameters.values = {
        {"x_height", .5}, {"axis_height", row.number("input.font.axis_height")},
        {"num1", row.number("input.font.num1")}, {"num2", row.number("input.font.num2")},
        {"num3", row.number("input.font.num3")}, {"denom1", row.number("input.font.denom1")},
        {"denom2", row.number("input.font.denom2")},
        {"rule_thickness", row.number("input.font.rule_thickness")},
        {"nulldelimiterspace", row.number("input.font.nulldelimiterspace")},
        {"delimiterfactor", 900}, {"delimitershortfall", .3}};
    common::FontVariant normal;
    normal.name = "normal";
    normal.cache_id = "fraction-test-normal";
    face.variants.emplace(normal.name, std::move(normal));
    builder.addFace(std::move(face));
    common::DelimiterRecipe slash;
    slash.direction = common::StretchDirection::vertical;
    slash.sizes = {1000};
    slash.variants = {0};
    builder.addDelimiter("fraction-test", '/', std::move(slash));
    return std::move(builder).freeze();
}

void runSourceRow(SourceRow const& row) {
    const BBox numerator = row.box("input.numerator");
    const BBox denominator = row.box("input.denominator");
    const bool bevelled = row.text("input.kind") == "bevelled";
    const BBox slash = bevelled ? row.box("expected.slash") : BBox{};
    auto fonts = makeFonts(row);
    RejectMeasurement measurement;
    mml::MmlDocument document;
    auto& numeratorNode = document.factory().make(mml::NodeKind::mspace);
    auto& denominatorNode = document.factory().make(mml::NodeKind::mspace);
    const std::map<mml::MmlNode const*, BBox> boxes{{&numeratorNode, numerator},
                                                    {&denominatorNode, denominator}};
    std::array<mml::MmlNode*, 2> children{&numeratorNode, &denominatorNode};
    mml::AttributeSet attributes;
    attributes.set(u"displaystyle", row.boolean("input.display"));
    attributes.set(u"scriptlevel", 0.0);
    // The paired Source receiver supplies an already converted em thickness.
    auto thickness = std::format("{}", row.number("input.lineThickness"));
    attributes.set(u"linethickness", std::u16string(thickness.begin(), thickness.end()) + u"em");
    attributes.set(u"bevelled", bevelled);
    attributes.set(u"numalign", row.text("input.align.0").starts_with("l") ? u"left" :
        row.text("input.align.0").starts_with("r") ? u"right" : u"center");
    attributes.set(u"denomalign", row.text("input.align.1").starts_with("l") ? u"left" :
        row.text("input.align.1").starts_with("r") ? u"right" : u"center");
    auto& fractionNode = document.factory().make(mml::NodeKind::mfrac,
        std::move(attributes), children);
    fractionNode.properties().tex.with_delimiters = row.boolean("input.withDelims");
    document.setRoot(fractionNode);

    common::ExtendedMetrics metrics;
    common::CommonOptions options;
    common::CommonOutputJax jax(document,
        {metrics, options, fonts, "fraction-test"}, measurement,
        makeCatalog(boxes, slash), &makeLinebreaks);
    auto& actual = dynamic_cast<common::CommonMfrac&>(jax.factory().wrap(fractionNode));
    if (bevelled) {
        row.require(actual.bevel != nullptr && actual.bevel->size.has_value(),
                    "expected bevel delimiter size selection");
        const auto data = actual.getBevelData(row.boolean("input.display"));
        row.check(data.H, "expected.H");
        row.check(data.delta, "expected.delta");
        row.check(data.u, "expected.u");
        row.check(data.v, "expected.v");
    }
    row.check_box(actual.getBBox(), "expected.bbox");
    row.check(actual.getWrapWidth(0), "expected.wrapWidths.0");
    row.check(actual.getWrapWidth(1), "expected.wrapWidths.1");
    const std::string numeratorAlign = bevelled ? "left" : row.text("input.align.0");
    const std::string denominatorAlign = bevelled ? "left" : row.text("input.align.1");
    row.require(actual.getChildAlign(0) == numeratorAlign, "input.align.0");
    row.require(actual.getChildAlign(1) == denominatorAlign, "input.align.1");
    row.require(!actual.canStretch(common::StretchDirection::vertical), "canStretch");
    const auto line = actual.getLineBBox(0);
    expect(line != nullptr);
    expect(actual.getLineBBox(0) == line);

    const auto geometry = actual.layoutGeometry();
    row.check(std::string_view("0"), "expected.svg.0.child");
    row.check(geometry.numerator_origin.x, "expected.svg.0.x");
    row.check(geometry.numerator_origin.y, "expected.svg.0.y");
    const std::string denominatorEvent = bevelled ? "expected.svg.2" : "expected.svg.1";
    row.check(std::string_view("1"), denominatorEvent + ".child");
    row.check(geometry.denominator_origin.x, denominatorEvent + ".x");
    row.check(geometry.denominator_origin.y, denominatorEvent + ".y");
    if (bevelled) {
        row.check(std::string_view("slash"), "expected.svg.1.child");
        row.require(geometry.slash_origin.has_value(), "expected.svg.1 slash origin");
        if (geometry.slash_origin) {
            row.check(geometry.slash_origin->x, "expected.svg.1.x");
            row.check(geometry.slash_origin->y, "expected.svg.1.y");
        }
    } else {
        row.require(!geometry.slash_origin.has_value(), "unexpected slash origin");
    }
    if (row.present("expected.svg.2.rule.width")) {
        row.require(geometry.rule.has_value(), "expected fraction rule");
        if (geometry.rule) {
            row.check(geometry.rule->x, "expected.svg.2.rule.x");
            row.check(geometry.rule->y, "expected.svg.2.rule.y");
            row.check(geometry.rule->width, "expected.svg.2.rule.width");
            row.check(geometry.rule->height, "expected.svg.2.rule.height");
        }
    } else {
        row.require(!geometry.rule.has_value(), "unexpected fraction rule");
    }
}
}

suite common_mfrac_source_pairs = [] {
    "all_generated_source_fraction_pairs"_test = [] {
        folia::math::test::SourceTable{"fraction_source_pairs.txt", 288}.for_each(runSourceRow);
    };
};
