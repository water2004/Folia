#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.mi;
import folia.math.core.mml_tree.mml_nodes.msqrt;
import folia.math.core.mml_tree.mml_nodes.mroot;
import folia.math.output.common;
#include "math/support/layout_source_table.hpp"

using namespace boost::ut;
using namespace folia::math;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;
namespace scene = folia::math::output::scene;
using folia::math::test::SourceTable;

namespace {
class NoMeasurement final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("radical fixture must not request metrics");
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("radical fixture must not measure text");
    }
};

class RejectBreaks final : public common::Linebreaks {
  public:
    using common::Linebreaks::Linebreaks;

    void breakToWidth(common::CommonWrapper &, double) override {
        throw std::logic_error("radical fixture must not linebreak");
    }
};

std::unique_ptr<common::Linebreaks> makeBreaks(common::CommonWrapperFactory &factory) {
    return std::make_unique<RejectBreaks>(factory);
}

void check_point(folia::math::test::SourceRow const &row, scene::Point const &point,
                 std::string const &prefix) {
    row.check(point.x, prefix + ".x");
    row.check(point.y, prefix + ".y");
}

void check_rect(folia::math::test::SourceRow const &row, scene::Rect const &rect,
                std::string const &prefix) {
    row.check(rect.x, prefix + ".x");
    row.check(rect.y, prefix + ".y");
    row.check(rect.width, prefix + ".width");
    row.check(rect.height, prefix + ".height");
}

class FixtureBBoxWrapper final : public common::CommonWrapper {
  public:
    using CommonWrapper::CommonWrapper;

    FixtureBBoxWrapper(common::CommonWrapperFactory &factory, mml::MmlNode &node,
                       common::CommonWrapper *parent,
                       std::map<mml::MmlNode const *, BBox> const &boxes)
        : CommonWrapper(factory, node, parent) {
        if (auto found = boxes.find(&node); found != boxes.end())
            fixtureBox = found->second;
    }

    void initializeSource() override {
        CommonWrapper::initializeSource();
        if (fixtureBox) {
            bbox = *fixtureBox;
            bboxComputed = true;
        }
    }

  private:
    std::optional<BBox> fixtureBox;
};

template <class Wrapper> common::CommonWrapperRegistration registration(mml::NodeKind kind) {
    common::CommonWrapperRegistration result;
    result.kind = kind;
    result.data = common::CommonWrapperClassData::commonDefaults();
    result.creator.construct = [](common::WrapperBuildArguments const &args) {
        return std::make_unique<Wrapper>(args.factory, args.node, args.parent);
    };
    result.creator.isInstance = [](common::CommonWrapper const &wrapper) {
        return dynamic_cast<Wrapper const *>(&wrapper) != nullptr;
    };
    return result;
}

common::CommonWrapperCatalog catalog(std::map<mml::MmlNode const *, BBox> const &boxes) {
    common::CommonWrapperCatalog result;
    result.push_back(registration<common::CommonMsqrt>(mml::NodeKind::msqrt));
    result.push_back(registration<common::CommonMroot>(mml::NodeKind::mroot));
    result.push_back(registration<common::CommonMo>(mml::NodeKind::mo));
    for (const auto kind : {mml::NodeKind::mi, mml::NodeKind::inferred_mrow}) {
        common::CommonWrapperRegistration fixture;
        fixture.kind = kind;
        fixture.data = common::CommonWrapperClassData::commonDefaults();
        fixture.creator.construct = [&boxes](common::WrapperBuildArguments const &args) {
            return std::make_unique<FixtureBBoxWrapper>(args.factory, args.node, args.parent,
                                                        boxes);
        };
        fixture.creator.isInstance = [](common::CommonWrapper const &wrapper) {
            return dynamic_cast<FixtureBBoxWrapper const *>(&wrapper) != nullptr;
        };
        result.push_back(std::move(fixture));
    }
    result.push_back(registration<common::CommonWrapper>(mml::NodeKind::text));
    return result;
}

common::FontRepository makeFonts(folia::math::test::SourceRow const &row) {
    common::FontRepositoryBuilder builder;
    common::FontFaceData face;
    face.name = "radical-source-fixture";
    face.size_variants = {"size0", "size1", "size2"};
    for (auto const *name : {"normal", "italic", "size0", "size1", "size2"}) {
        common::FontVariant variant;
        variant.name = name;
        variant.cache_id = name;
        face.variants.emplace(name, std::move(variant));
    }
    const auto faceName = face.name;
    builder.addFace(std::move(face));
    builder.addParameter(faceName, "rule_thickness", row.number("input.font.rule_thickness"));
    builder.addParameter(faceName, "surd_height", row.number("input.font.surd_height"));
    builder.addParameter(faceName, "x_height", row.number("input.font.x_height"));
    builder.addParameter(faceName, "axis_height", .25);
    builder.addParameter(faceName, "delimiterfactor", 901);
    builder.addParameter(faceName, "delimitershortfall", .1);
    builder.addParameter(faceName, "nulldelimiterspace", .12);
    common::DelimiterRecipe recipe;
    recipe.direction = common::StretchDirection::vertical;
    if (row.number("input.surdSize") < 0) {
        recipe.stretch = {0x221A};
        recipe.heightDepthWidth = std::array<double, 3>{1, .25, 1};
    } else {
        recipe.sizes = {0, 0, 1e6};
        recipe.variants = {0, 1, 2};
    }
    builder.addDelimiter(faceName, 0x221A, std::move(recipe));
    return std::move(builder).freeze();
}

void check_stretched(common::CommonMsqrt const &radical, folia::math::test::SourceRow const &row) {
    row.check(radical.surdH, "expected.request.requiredSurdHeight");
    row.check(radical.rootWidth(), "expected.request.rootWidth");
    row.require(radical.base() == 0, "input.base role");
    row.require(radical.root().has_value() == row.present("input.degree.w"), "input.degree role");
    const auto size = radical.surd->size;
    row.require(size.has_value(), "input.surdSize selected");
    if (size) {
        if (row.number("input.surdSize") < 0)
            row.require(std::holds_alternative<common::AssemblyStretchSize>(*size),
                        "input.surdSize assembly");
        else
            row.require(std::get<common::FixedStretchSize>(*size).index ==
                            static_cast<std::size_t>(row.number("input.surdSize")),
                        "input.surdSize fixed");
    }
    auto const &face = radical.jax().face();
    const double p = row.boolean("input.display") ? face.parameters.required("x_height")
                                                  : face.parameters.required("rule_thickness");
    const auto baseBox = row.box("input.base");
    row.require(radical.surdH == baseBox.h + baseBox.d + face.parameters.required("rule_thickness") +
                                    face.parameters.required("surd_height") + p / 4,
                "required surd height from base and font parameters");
    row.check(radical.surdH - baseBox.d, "expected.request.stretchHeight");
    row.check(baseBox.d, "expected.request.stretchDepth");
}
} // namespace

suite native_common_radical_source_pairs = [] {
    "all_generated_source_radical_pairs_use_common_wrapper_classes"_test = [] {
        SourceTable{"radical_source_pairs.txt", 192}.for_each([](auto const &row) {
            auto fonts = makeFonts(row);
            NoMeasurement measurement;
            mml::MmlDocument document;
            std::map<mml::MmlNode const *, BBox> boxes;

            mml::AttributeSet radicalAttributes;
            radicalAttributes.set(u"displaystyle", row.boolean("input.display"));
            auto &base = document.factory().make(mml::NodeKind::mi);
            boxes.emplace(&base, row.box("input.base"));
            mml::MmlNode *radicalNode{};
            if (row.present("input.degree.w")) {
                auto &degree = document.factory().make(mml::NodeKind::mi);
                boxes.emplace(&degree, row.box("input.degree"));
                const std::array<mml::MmlNode *, 2> children{&base, &degree};
                radicalNode = &document.factory().make(mml::NodeKind::mroot,
                                                       std::move(radicalAttributes), children);
            } else {
                const std::array<mml::MmlNode *, 1> children{&base};
                radicalNode = &document.factory().make(mml::NodeKind::msqrt,
                                                       std::move(radicalAttributes), children);
                row.require(radicalNode->children().size() == 1 &&
                                radicalNode->children().front()->isInferred(),
                            "msqrt inferred row");
                boxes.emplace(radicalNode->children().front(), row.box("input.base"));
            }
            document.setRoot(*radicalNode);
            mml::MmlNode *mmlBase{};
            if (row.present("input.degree.w")) {
                mmlBase = radicalNode->children().front();
                row.require(mmlBase->parent() == radicalNode, "mroot base parent link");
                row.require(radicalNode->children().size() == 2 &&
                                radicalNode->children()[1]->parent() == radicalNode,
                            "mroot degree parent link");
            } else {
                auto *inferred = radicalNode->children().front();
                mmlBase = &base;
                row.require(base.parent() == inferred && inferred->parent() == radicalNode,
                            "msqrt base through inferred-row parent link");
            }
            const auto originalBase = mmlBase->attributes().ownEntries();
            common::CommonOptions options;
            common::CommonOutputJaxConfig config{common::ExtendedMetrics{}, options, fonts,
                                                 "radical-source-fixture"};
            common::LayoutJob job(document, std::move(config), measurement, catalog(boxes),
                                  &makeBreaks);
            auto &wrapped = job.build();
            auto *radical = dynamic_cast<common::CommonMsqrt *>(&wrapped);
            row.require(radical != nullptr, "factory selected radical family class");
            if (!radical)
                return;
            row.check_box(radical->childNodes.at(radical->base())->getBBox(), "input.base");
            if (auto degree = radical->root())
                row.check_box(radical->childNodes.at(*degree)->getBBox(), "input.degree");
            check_stretched(*radical, row);
            radical->getStretchedSurd();
            check_stretched(*radical, row);

            radical->surd->bbox = row.box("input.surd");
            radical->surd->bboxComputed = true;
            const auto surdBox = row.box("input.surd");
            row.check_box(radical->surd->getBBox(), "input.surd");
            const auto [p, q] = radical->getPQ(surdBox);
            row.check(p, "expected.p");
            row.check(q, "expected.q");
            const auto &font = job.jax().face().parameters;
            const double measuredHeight =
                row.box("input.base").h + q + font.required("rule_thickness");
            row.check(measuredHeight, "expected.measuredHeight");
            row.check(radical->rootWidth(), "expected.request.rootWidth");
            const double commonScale = radical->bbox.scale;
            radical->bbox.scale = row.number("input.wrapperScale");
            auto geometry = radical->layoutGeometry();
            radical->bbox.scale = commonScale;
            row.check(geometry.placed_height, "expected.placedHeight");
            row.check(geometry.placed_surd_height, "expected.placedSurdHeight");
            check_rect(row, geometry.overbar, "expected.overbar");
            bool saw_surd = false;
            bool saw_base = false;
            bool saw_degree = false;
            for (int i = 0; i < 3; ++i) {
                const auto prefix = std::string("expected.placements.") + std::to_string(i);
                if (!row.present(prefix + ".role"))
                    continue;
                const auto role = row.text(prefix + ".role");
                if (role == "surd") {
                    check_point(row, geometry.surd_origin, prefix);
                    saw_surd = true;
                } else if (role == "base") {
                    check_point(row, geometry.base_origin, prefix);
                    saw_base = true;
                } else if (role == "degree") {
                    row.require(geometry.degree_origin.has_value(), prefix + ".role");
                    if (geometry.degree_origin)
                        check_point(row, *geometry.degree_origin, prefix);
                    saw_degree = true;
                } else {
                    row.require(false, prefix + ".role");
                }
            }
            row.require(saw_surd && saw_base, "expected.placements");
            row.require(geometry.degree_origin.has_value() == saw_degree,
                        "expected.degreeOrigin");
            const auto dimensions = radical->getRootDimens(surdBox, geometry.placed_height);
            row.check(dimensions[0], "expected.rootOffset");
            row.check(dimensions[1], "expected.rootHeight");
            row.check(dimensions[2], "expected.rootDx");
            row.check_box(radical->getBBox(), "expected.bbox");
            row.require(mmlBase->attributes().ownEntries() == originalBase,
                        "radical leaves source child attributes unchanged");
        });
    };
};
