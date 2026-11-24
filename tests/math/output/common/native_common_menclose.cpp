#include <cstdlib>
#include <fstream>
#include "support/folia_test.hpp"
#include "math/fixtures/common-source/menclose-geometry-source-expected.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.math;
import folia.math.core.mml_tree.mml_nodes.mi;
import folia.math.core.mml_tree.mml_nodes.menclose;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.core.mml_tree.mml_nodes.mrow;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.svg.menclose_notations;
import folia.math.output.svg.writer;

namespace {
using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;
namespace scene = folia::math::output::scene;

struct NoMeasurement final : common::MeasurementTarget {
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("menclose fixture supplies metrics");
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("menclose fixture uses pinned NewCM glyphs");
    }
};

std::unique_ptr<common::Linebreaks> makeVisitor(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

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

common::CommonWrapperCatalog catalog(std::span<common::NotationSpec const> notations) {
    common::CommonWrapperCatalog result;
    result.push_back(registration<common::CommonWrapper>(mml::NodeKind::math));
    common::CommonWrapperRegistration menclose;
    menclose.kind = mml::NodeKind::menclose;
    menclose.data = common::CommonWrapperClassData::commonDefaults();
    menclose.creator.construct = [notations](common::WrapperBuildArguments const &args) {
        return std::make_unique<common::CommonMenclose>(args.factory, args.node, args.parent,
                                                        notations);
    };
    menclose.creator.isInstance = [](common::CommonWrapper const &wrapper) {
        return dynamic_cast<common::CommonMenclose const *>(&wrapper) != nullptr;
    };
    result.push_back(std::move(menclose));
    result.push_back(registration<common::CommonMsqrt>(mml::NodeKind::msqrt));
    result.push_back(registration<common::CommonMo>(mml::NodeKind::mo));
    result.push_back(registration<common::CommonInferredMrow>(mml::NodeKind::inferred_mrow));
    result.push_back(registration<common::CommonMrow>(mml::NodeKind::mrow));
    result.push_back(registration<common::CommonMi>(mml::NodeKind::mi));
    result.push_back(registration<common::CommonTextNode>(mml::NodeKind::text));
    return result;
}

mml::MmlMi &makeMi(mml::MmlDocument &document, std::u16string text) {
    auto &token = dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
    token.setText(std::move(text));
    const std::array<mml::MmlNode *, 1> children{&token};
    return dynamic_cast<mml::MmlMi &>(document.factory().make(mml::NodeKind::mi, {}, children));
}

class GeometryInputWrapper final : public common::CommonWrapper {
    folia::math::BBox input_;

    static double numericAttribute(mml::MmlNode const &node, std::u16string_view key) {
        const auto *value = node.attributes().lookup(key);
        if (!value || !std::holds_alternative<double>(*value))
            throw std::logic_error("geometry fixture is missing a numeric extent");
        return std::get<double>(*value);
    }

  public:
    GeometryInputWrapper(common::CommonWrapperFactory &factory, mml::MmlNode &node,
                         common::CommonWrapper *parent)
        : CommonWrapper(factory, node, parent),
          input_(folia::math::BBoxData{.w = numericAttribute(node, u"probe-width"),
                                       .h = numericAttribute(node, u"probe-height"),
                                       .d = numericAttribute(node, u"probe-depth")}) {
    }

    folia::math::BBox &getBBox() override {
        return input_;
    }
};

common::CommonWrapperCatalog geometryCatalog(std::span<common::NotationSpec const> notations) {
    auto result = catalog(notations);
    auto &row = *std::find_if(result.begin(), result.end(), [](auto const &entry) {
        return entry.kind == mml::NodeKind::inferred_mrow;
    });
    row.creator.construct = [](common::WrapperBuildArguments const &args) {
        return std::make_unique<GeometryInputWrapper>(args.factory, args.node, args.parent);
    };
    row.creator.isInstance = [](common::CommonWrapper const &wrapper) {
        return dynamic_cast<GeometryInputWrapper const *>(&wrapper) != nullptr;
    };
    return result;
}

std::string serializeGeometry(std::span<common::NotationGeometry const> geometry) {
    scene::Group root;
    root.metadata.source_tag = "mrow";
    for (const auto &item : geometry)
        root.entries.emplace_back(
            std::visit([](auto const &primitive) { return scene::Primitive{primitive}; }, item));
    const scene::Scene completed(std::move(root), {3, 1, .5}, {1, .5, .4, .7});
    folia::math::output::svg::SvgWriteOptions options;
    options.font_cache = folia::math::output::svg::FontCacheMode::none;
    options.include_math_container = false;
    return folia::math::output::svg::write(completed, options);
}
} // namespace

suite native_common_menclose = [] {
    "pinned_notation_recipes_keep_mathjax_order_and_exact_extents"_test = [] {
        const auto specs = folia::math::output::svg::mencloseNotations();
        const std::array<std::string_view, 29> expected{"top",
                                                        "right",
                                                        "bottom",
                                                        "left",
                                                        "actuarial",
                                                        "madruwb",
                                                        "updiagonalstrike",
                                                        "downdiagonalstrike",
                                                        "horizontalstrike",
                                                        "verticalstrike",
                                                        "box",
                                                        "roundedbox",
                                                        "circle",
                                                        "phasorangle",
                                                        "uparrow",
                                                        "downarrow",
                                                        "leftarrow",
                                                        "rightarrow",
                                                        "updownarrow",
                                                        "leftrightarrow",
                                                        "updiagonalarrow",
                                                        "northeastarrow",
                                                        "southeastarrow",
                                                        "northwestarrow",
                                                        "southwestarrow",
                                                        "northeastsouthwestarrow",
                                                        "northwestsoutheastarrow",
                                                        "longdiv",
                                                        "radical"};
        expect(specs.size() == expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
            expect(specs[i].source_name == expected[i]);
        const auto box = std::find_if(specs.begin(), specs.end(), [](auto const &s) {
            return s.kind == common::NotationKind::box;
        });
        expect(box != specs.end());
        expect(box->removes.size() == 4_u);
        const std::array<std::pair<common::NotationKind, std::vector<std::string_view>>, 8>
            removals{
                {{common::NotationKind::actuarial, {"top", "right"}},
                 {common::NotationKind::madruwb, {"bottom", "right"}},
                 {common::NotationKind::box, {"left", "right", "top", "bottom"}},
                 {common::NotationKind::phasor_angle, {"bottom"}},
                 {common::NotationKind::up_arrow, {"verticalstrike"}},
                 {common::NotationKind::down_arrow, {"verticakstrike"}},
                 {common::NotationKind::up_down_arrow, {"verticalstrike", "uparrow", "downarrow"}},
                 {common::NotationKind::left_right_arrow,
                  {"horizontalstrike", "leftarrow", "rightarrow"}}}};
        for (auto const &[kind, expectedRemovals] : removals) {
            const auto found = std::find_if(specs.begin(), specs.end(),
                                            [&](auto const &s) { return s.kind == kind; });
            expect(found != specs.end());
            if (found != specs.end())
                expect(std::equal(found->removes.begin(), found->removes.end(),
                                  expectedRemovals.begin(), expectedRemovals.end()));
        }
        const auto down = std::find_if(specs.begin(), specs.end(), [](auto const &s) {
            return s.kind == common::NotationKind::down_arrow;
        });
        expect(down != specs.end());
        expect(down->removes.size() == 1_u && down->removes.front() == "verticakstrike");
        const auto radical = std::find_if(specs.begin(), specs.end(), [](auto const &s) {
            return s.kind == common::NotationKind::radical;
        });
        expect(radical != specs.end() && radical->render_child);

        folia::math::BBox content(folia::math::BBoxData{.w = 3, .h = 1, .d = .5});
        const auto arrowHead = common::arrowHead(.2, .067, {4, 2, 1});
        expect(arrowHead == .402) << "arrowHead";
        const auto diagonalData = common::arrowData(.2, .067, {4, 2, 1}, content);
        expect(diagonalData.angle == 0.485706245871551)
            << "arrow angle" << std::format("{:.17g}", diagonalData.angle);
        expect(diagonalData.width == 4.0699775561813185) << "arrow width";
        expect(diagonalData.x == 0.29963310898497186) << "arrow x";
        expect(diagonalData.y == .2) << "arrow y";
        const auto diagonalExt = common::notationBBox(
            *std::find_if(
                specs.begin(), specs.end(),
                [](auto const &s) { return s.kind == common::NotationKind::northeast_arrow; }),
            .2, .067, {4, 2, 1}, &content);
        expect(diagonalExt[0] == .2 && diagonalExt[1] == diagonalData.x && diagonalExt[2] == .2 &&
               diagonalExt[3] == diagonalData.x)
            << "diagonal extents";
        const auto diagAW = common::arrowAW({.4, .1, .2, .3}, content);
        expect(diagAW.first == 0.5532943253222926) << "arrowAW angle";
        expect(diagAW.second == 3.996248240537617) << "arrowAW width";
        expect((common::notationBBox(
                    *std::find_if(
                        specs.begin(), specs.end(),
                        [](auto const &s) { return s.kind == common::NotationKind::up_arrow; }),
                    .2, .067, {4, 2, 1}, &content) == common::PaddingData{.402, 0, .2, 0}));
        const auto phasor = common::notationBBox(
            *std::find_if(
                specs.begin(), specs.end(),
                [](auto const &s) { return s.kind == common::NotationKind::phasor_angle; }),
            .2, .067, {4, 2, 1}, nullptr);
        expect((phasor == common::PaddingData{.2, .1, .167, .36700000000000005}))
            << "phasor extents";
        expect(common::findNotation(specs, u"not-a-notation") == std::nullopt);
        expect(common::findNotation(specs, u"radical") == common::NotationKind::radical);

        const auto line = common::lineGeometry(common::LineName::top, content, .067);
        expect(line.segments.size() == 2_u);
        const auto *first = std::get_if<folia::math::output::scene::MoveTo>(&line.segments[0]);
        const auto *last = std::get_if<folia::math::output::scene::LineTo>(&line.segments[1]);
        expect(first != nullptr && last != nullptr);
        if (first && last) {
            expect(first->to.x == 0.0);
            expect(first->to.y ==
                   1.0 -
                       .0335); // Source coordinates already share the enclosing group's math axis.
            expect(last->to.x == 3.0);
            expect(last->to.y == first->to.y);
        }
        const auto fillArrow = common::notationGeometry(
            *std::find_if(specs.begin(), specs.end(),
                          [](auto const &s) { return s.kind == common::NotationKind::up_arrow; }),
            content, .067, .2, {4, 2, 1}, {.1, .4, .1, .2});
        expect(fillArrow.size() == 1_u);
        if (!fillArrow.empty()) {
            const auto *arrow = std::get_if<scene::Path>(&fillArrow.front());
            expect(arrow != nullptr);
            if (arrow) {
                expect(arrow->paint.fill == scene::PaintColor{scene::InheritedPaint{}});
                expect(arrow->paint.stroke == scene::PaintColor{scene::InheritedPaint{}});
                expect(arrow->role == scene::ElementRole::enclosure_mark);
                expect(arrow->syntax == scene::PathSyntax::spaced);
                const auto operations = arrow->transform.operations();
                expect(operations.size() == 2_u);
                if (operations.size() == 2) {
                    // Source arrow() emits translation before rotation about the box center.
                    const auto *translation = std::get_if<scene::Translation>(&operations[0]);
                    const auto *rotation = std::get_if<scene::Rotation>(&operations[1]);
                    expect(translation != nullptr && rotation != nullptr);
                    if (translation && rotation) {
                        expect((*translation ==
                                scene::Translation{-.1, 0, scene::TranslationSeparator::space}));
                        expect((*rotation == scene::Rotation{90, {1.5, .25}}));
                    }
                }
                const bool hasRelativeLine = std::any_of(
                    arrow->geometry.segments.begin(), arrow->geometry.segments.end(),
                    [](scene::PathSegment const &segment) {
                        const auto *lineTo = std::get_if<scene::LineTo>(&segment);
                        return lineTo && lineTo->mode == scene::CoordinateMode::relative;
                    });
                expect(hasRelativeLine);
            }
        }
        const auto phasorPaths = common::notationGeometry(
            *std::find_if(
                specs.begin(), specs.end(),
                [](auto const &s) { return s.kind == common::NotationKind::phasor_angle; }),
            content, .067, .2, {4, 2, 1}, {.1, .2, .1, .2});
        expect(phasorPaths.size() == 1_u); // Source angle path already contains its bottom segment.
        const auto circlePaths = common::notationGeometry(
            *std::find_if(specs.begin(), specs.end(),
                          [](auto const &s) { return s.kind == common::NotationKind::circle; }),
            content, .067, .2, {4, 2, 1}, {.1, .2, .1, .2});
        expect(circlePaths.size() == 1_u);
        if (!circlePaths.empty()) {
            const auto *ellipse = std::get_if<scene::Ellipse>(&circlePaths.front());
            expect(ellipse != nullptr);
            if (ellipse) {
                expect(ellipse->center == scene::Point{1.5, .25});
                expect(ellipse->radius_x == 1.4665);
                expect(ellipse->radius_y == .7165);
                expect(ellipse->role == scene::ElementRole::enclosure_mark);
            }
        }
        const auto boxGeometry = common::notationGeometry(
            *std::find_if(specs.begin(), specs.end(),
                          [](auto const &s) { return s.kind == common::NotationKind::box; }),
            content, .067, .2, {4, 2, 1}, {.1, .2, .1, .2});
        expect(boxGeometry.size() == 1_u);
        if (!boxGeometry.empty()) {
            const auto *rule = std::get_if<scene::Rule>(&boxGeometry.front());
            expect(rule != nullptr);
            if (rule) {
                expect(rule->bounds == scene::Rect{.0335, -.4665, 2.933, 1.433});
                expect(!rule->corner_radius);
                expect(rule->role == scene::ElementRole::enclosure_mark);
            }
        }
        const auto roundedBox = common::notationGeometry(
            *std::find_if(
                specs.begin(), specs.end(),
                [](auto const &s) { return s.kind == common::NotationKind::rounded_box; }),
            content, .067, .2, {4, 2, 1}, {.1, .2, .1, .2});
        expect(roundedBox.size() == 1_u);
        if (!roundedBox.empty()) {
            const auto *rule = std::get_if<scene::Rule>(&roundedBox.front());
            expect(rule != nullptr);
            if (rule) {
                expect(rule->bounds == scene::Rect{.0335, -.4665, 2.933, 1.433});
                expect(rule->corner_radius == .267);
                expect(rule->role == scene::ElementRole::enclosure_mark);
            }
        }
        const auto longdivPaths = common::notationGeometry(
            *std::find_if(specs.begin(), specs.end(),
                          [](auto const &s) { return s.kind == common::NotationKind::long_div; }),
            content, .067, .2, {4, 2, 1}, {.1, .2, .1, .2});
        expect(longdivPaths.size() == 1_u);
        if (!longdivPaths.empty()) {
            const auto *path = std::get_if<scene::Path>(&longdivPaths.front());
            expect(path != nullptr);
            if (path && path->geometry.segments.size() > 1) {
                const auto *arc = std::get_if<scene::ArcTo>(&path->geometry.segments[1]);
                expect(arc != nullptr);
                if (arc) {
                    expect(arc->mode == scene::CoordinateMode::relative);
                    expect(arc->flags_separator == scene::ArcFlagSeparator::comma);
                    expect(arc->to == scene::Point{0, 1.433});
                    // Source evaluates p - t / 2, with t = thickness / 2.
                    expect(arc->radius_x == .18325000000000002)
                        << "longdiv Source radius_x" << std::format("{:.17g}", arc->radius_x);
                    expect(arc->radius_y == .616);
                }
                expect(path->syntax == scene::PathSyntax::spaced);
                expect(path->paint_encoding == scene::PathPaintEncoding::stroke_width_style);
                expect(path->role == scene::ElementRole::enclosure_mark);
            }
        }
    };

    "explicit_source_cap_join_values_are_preserved_and_absence_stays_absent"_test = [] {
        using namespace folia::math::output::scene;
        const auto specs = folia::math::output::svg::mencloseNotations();
        const folia::math::BBox box(folia::math::BBoxData{.w = 3, .h = 1.25, .d = .5});
        const auto lookup = [&](common::NotationKind kind) -> common::NotationSpec const & {
            const auto found = std::find_if(specs.begin(), specs.end(),
                                            [&](auto const &spec) { return spec.kind == kind; });
            if (found == specs.end())
                throw std::logic_error("notation is absent from pinned source catalog");
            return *found;
        };
        const auto phasor =
            common::notationGeometry(lookup(common::NotationKind::phasor_angle), box, .067, .2,
                                     {4, 2, 1}, {.2, .1, .167, .367});
        const auto longdiv = common::notationGeometry(lookup(common::NotationKind::long_div), box,
                                                      .067, .2, {4, 2, 1}, {.267, .2, .2, .4335});
        const auto top = common::notationGeometry(lookup(common::NotationKind::top), box, .067, .2,
                                                  {4, 2, 1}, {.267, 0, 0, 0});
        const auto *phasorPath = phasor.empty() ? nullptr : std::get_if<Path>(&phasor.front());
        const auto *longdivPath = longdiv.empty() ? nullptr : std::get_if<Path>(&longdiv.front());
        const auto *topPath = top.empty() ? nullptr : std::get_if<Path>(&top.front());
        expect(phasorPath && phasorPath->paint.line_cap == StrokeCap::round);
        expect(phasorPath && phasorPath->paint.line_join == StrokeJoin::miter);
        expect(phasorPath && phasorPath->paint_encoding == PathPaintEncoding::stroke_width_style);
        expect(phasorPath && phasorPath->join_spelling == StrokeJoinSpelling::mitre);
        expect(longdivPath && longdivPath->paint.line_cap == StrokeCap::round);
        expect(longdivPath && longdivPath->paint.line_join == StrokeJoin::round);
        expect(longdivPath && longdivPath->paint_encoding == PathPaintEncoding::stroke_width_style);
        expect(topPath && !topPath->paint.line_cap && !topPath->paint.line_join);
    };

    "source_bbox_and_extenders_match_exact_raw_values_for_positive_and_negative_height"_test = [] {
        using namespace native_common_menclose_source_geometry;
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurement measurement;
        mml::MmlDocument document;
        auto specs = folia::math::output::svg::mencloseNotations();
        auto catalogValue = geometryCatalog(specs);
        common::CommonOptions options;
        common::ExtendedMetrics metrics;
        common::CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"},
                                    measurement, std::move(catalogValue), &makeVisitor);
        struct InputCase {
            std::string_view name;
            double w, h, d;
        };
        const std::array<InputCase, 2> cases{
            {{"positive", 3.0, 1.25, 0.5}, {"negative_height", 2.4, -0.4, 1.1}}};
        std::size_t expectedIndex = 0;
        std::ofstream dump;
        if (const char *path = std::getenv("FOLIA_MENCLOSE_SVG_DUMP"))
            dump.open(path, std::ios::trunc);
        for (const auto &input : cases) {
            for (const auto &spec : specs) {
                if (spec.render_child)
                    continue; // radical child placement has its own ownership test.
                mml::AttributeSet attributes;
                attributes.set(u"notation",
                               std::u16string(spec.source_name.begin(), spec.source_name.end()));
                auto &node =
                    document.factory().make(mml::NodeKind::menclose, std::move(attributes));
                auto &row = *node.children().front();
                row.attributes().set(u"probe-width", input.w);
                row.attributes().set(u"probe-height", input.h);
                row.attributes().set(u"probe-depth", input.d);
                auto &wrapper = dynamic_cast<common::CommonMenclose &>(jax.factory().wrap(node));
                const auto &expected = layout_rows.at(expectedIndex);
                expect(expected.case_name == input.name);
                expect(expected.notation == spec.source_name);
                auto &actualBox = wrapper.getBBox();
                auto check = [&](std::string_view field, double actual, double wanted) {
                    expect(actual == wanted)
                        << input.name << spec.source_name << field
                        << std::format("actual={:.17g} expected={:.17g}", actual, wanted);
                };
                check("w", actualBox.w, expected.w);
                check("h", actualBox.h, expected.h);
                check("d", actualBox.d, expected.d);
                const auto extenders = wrapper.getBBoxExtenders();
                for (std::size_t side = 0; side < extenders.size(); ++side)
                    check(std::to_string(side), extenders[side], expected.trbl[side]);
                if (dump.is_open()) {
                    const auto geometry = wrapper.sceneGeometry();
                    dump << input.name << '\t' << spec.source_name << '\t'
                         << serializeGeometry(geometry) << '\n';
                }
                ++expectedIndex;
            }
        }
        expect(expectedIndex == layout_rows.size());
    };

    "radical_wrapper_projects_real_child_without_reparenting_or_extra_mml_edge"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurement measurement;
        mml::MmlDocument document;
        auto &mencloseNode = document.factory().make(mml::NodeKind::menclose);
        mencloseNode.attributes().set(u"notation", u"radical");
        auto &inferred = *mencloseNode.children().front();
        auto &mi = makeMi(document, u"x");
        document.append(inferred, mi);
        auto &math = document.factory().make(mml::NodeKind::math);
        document.append(math, mencloseNode);
        document.setRoot(math);
        auto *physicalParent = inferred.parent();
        auto *semanticParent = inferred.Parent();
        const std::vector<mml::MmlNode *> originalMmlChildren(inferred.children().begin(),
                                                              inferred.children().end());
        const auto arenaSizeBeforeLayout = document.size();
        auto catalogValue = catalog(folia::math::output::svg::mencloseNotations());
        common::CommonOptions options;
        common::ExtendedMetrics metrics;
        common::CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"},
                                    measurement, std::move(catalogValue), &makeVisitor);
        auto &mathWrapper = jax.factory().wrap(math);
        auto &menclose = dynamic_cast<common::CommonMenclose &>(*jax.wrapperFor(mencloseNode));
        auto &sqrt = *menclose.msqrt();
        auto *projected = jax.wrapperFor(inferred);
        auto *originalChildWrapper = menclose.childNodes.front();
        expect(projected != nullptr);
        expect(originalChildWrapper != projected);
        expect(inferred.parent() == physicalParent);
        expect(inferred.Parent() == semanticParent);
        expect(std::equal(inferred.children().begin(), inferred.children().end(),
                          originalMmlChildren.begin(), originalMmlChildren.end()));
        expect(mencloseNode.children().size() == 1_u &&
               mencloseNode.children().front() == &inferred);
        expect(physicalParent != nullptr && physicalParent->kind() == mml::NodeKind::menclose);
        expect(sqrt.node->parent() == nullptr);
        expect(sqrt.node->children()
                   .empty()); // Its initialized inferred row was detached, not attached twice.
        expect(document.size() ==
               arenaSizeBeforeLayout +
                   std::size_t{4}); // sqrt + detached default row + surd Mo + text.
        expect(jax.wrapperFor(*sqrt.node) == &sqrt);
        expect(sqrt.parent == &menclose);
        expect(std::find(menclose.childNodes.begin(), menclose.childNodes.end(), &sqrt) ==
               menclose.childNodes.end());
        expect(sqrt.childNodes.size() == 1_u && sqrt.childNodes.front() == projected);
        expect(projected->node == &inferred && projected->parent == &sqrt);
        expect(jax.wrapperFor(inferred) == projected); // nodeMap's Source Map.set replacement.
        expect(menclose.renderChild() == common::NotationKind::radical);
        expect(menclose.sceneGeometry().empty()); // radical renders the projected child wrapper.
        expect(sqrt.getBBox().w > 0.0);
        expect(menclose.getBBox().w >= sqrt.getBBox().w);
        expect(mathWrapper.node == &math); // the original root remains the wrapper root.
    };

    "data_parameters_use_existing_em_lengths_and_source_parseFloat_prefix"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurement measurement;
        mml::MmlDocument document;
        mml::AttributeSet attributes;
        attributes.set(u"notation", u"box");
        attributes.set(u"data-padding", u".5em");
        attributes.set(u"data-thickness", u"2mu");
        attributes.set(u"data-arrowhead", u"5 .25 2tail");
        auto &mencloseNode =
            document.factory().make(mml::NodeKind::menclose, std::move(attributes));
        auto &inferred = *mencloseNode.children().front();
        auto &mi = makeMi(document, u"x");
        document.append(inferred, mi);
        auto &math = document.factory().make(mml::NodeKind::math);
        document.append(math, mencloseNode);
        document.setRoot(math);
        auto catalogValue = catalog(folia::math::output::svg::mencloseNotations());
        common::CommonOptions options;
        common::ExtendedMetrics metrics;
        common::CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"},
                                    measurement, std::move(catalogValue), &makeVisitor);
        (void)jax.factory().wrap(math);
        auto &menclose = dynamic_cast<common::CommonMenclose &>(*jax.wrapperFor(mencloseNode));
        expect(menclose.padding == .5);
        expect(menclose.thickness == 0.1111111111111111);
        expect(menclose.arrowhead.x == 5.0);
        expect(menclose.arrowhead.y == .25);
        expect(menclose.arrowhead.dx == 2.0);
        const auto extents = menclose.getBBoxExtenders();
        for (const auto value : extents)
            expect(value == 0.6111111111111112);
        const auto padding = menclose.getPadding();
        for (const auto value : padding)
            expect(value == .5);
        expect(menclose.sceneGeometry().size() == 1_u);
    };
};
