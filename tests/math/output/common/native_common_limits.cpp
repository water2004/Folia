#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "math/support/source_svg_paint.hpp"
#include "support/folia_test.hpp"
import folia.math.output.common;
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.munderover;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.scene;
import folia.math.output.svg.wrapper_factory;
import folia.math.output.svg.writer;

#include "math/support/mathml_observations.hpp"

namespace {
using namespace folia::math;
using namespace folia::math::output::common;
using namespace boost::ut;
namespace mml = folia::math::mml;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;
static_assert(std::is_base_of_v<CommonScriptbase, CommonMunder>);
static_assert(std::is_base_of_v<CommonScriptbase, CommonMover>);
static_assert(std::is_base_of_v<CommonScriptbase, CommonMunderover>);
// Source SVG's limits mixins extend the matching script family, which also
// supplies the movable-limits offsets and the two-script BBox algorithm.
static_assert(std::is_base_of_v<CommonMsub, CommonMunder>);
static_assert(std::is_base_of_v<CommonMsup, CommonMover>);
static_assert(std::is_base_of_v<CommonMsubsup, CommonMunderover>);

struct Target final : MeasurementTarget {
    ExtendedMetrics measureMetrics(MetricRequest const &request) override {
        return {.em = 16,
                .ex = 8,
                .containerWidth = 640,
                .scale = 1,
                .family = request.family,
                .display = false};
    }

    UnknownBBox measureUnknownText(UnknownTextRequest const &) override {
        throw std::logic_error("limits fixture uses known glyphs");
    }
};

template <class T> CommonWrapperRegistration registration(mml::NodeKind kind) {
    using Args = WrapperBuildArguments;
    using Creator = tree::FactoryClass<CommonWrapper, Args>;
    return {kind,
            Creator{[](Args const &a) -> std::unique_ptr<CommonWrapper> {
                        return std::make_unique<T>(a.factory, a.node, a.parent);
                    },
                    [](CommonWrapper const &w) { return dynamic_cast<T const *>(&w) != nullptr; }},
            CommonWrapperClassData::commonDefaults()};
}

class NoBreaks final : public Linebreaks {
  public:
    using Linebreaks::Linebreaks;

    void breakToWidth(CommonWrapper &, double) override {
        throw std::logic_error("unexpected linebreak");
    }
};

class FixedToken final : public CommonWrapper {
  protected:
    void computeBBox(BBox &out, bool) override {
        const bool base = getText() == u"x";
        out.w = base ? .5 : .2;
        out.h = base ? .7 : .3;
        out.d = base ? .2 : .1;
        out.ic = base ? .08 : 0;
        out.sk = base ? .02 : 0;
    }

  public:
    using CommonWrapper::CommonWrapper;
};

CommonWrapperCatalog catalog(bool fixedTokens) {
    using K = mml::NodeKind;
    CommonWrapperCatalog c{registration<CommonTextNode>(K::text),
                           registration<CommonMath>(K::math),
                           registration<CommonInferredMrow>(K::inferred_mrow),
                           registration<CommonMunder>(K::munder),
                           registration<CommonMover>(K::mover),
                           registration<CommonMunderover>(K::munderover),
                           registration<CommonMsub>(K::msub),
                           registration<CommonMsup>(K::msup),
                           registration<CommonMsubsup>(K::msubsup),
                           registration<CommonMmultiscripts>(K::mmultiscripts),
                           registration<CommonWrapper>(K::mprescripts),
                           registration<CommonWrapper>(K::none)};
    if (fixedTokens) {
        c.push_back(registration<FixedToken>(K::mi));
        c.push_back(registration<FixedToken>(K::mn));
    } else {
        c.push_back(registration<CommonMi>(K::mi));
        c.push_back(registration<CommonMn>(K::mn));
        c.push_back(registration<CommonMo>(K::mo));
        c.push_back(registration<CommonMrow>(K::mrow));
    }
    return c;
}

FontRepository const &fixtureFonts() {
    static const auto repo = [] {
        FontRepositoryBuilder b;
        FontFaceData f;
        f.name = "MathJaxNewcm";
        for (auto name : {"normal", "italic", "bold", "bold-italic"}) {
            FontVariant v;
            v.name = name;
            v.cache_id = name;
            f.variants.emplace(name, std::move(v));
        }
        b.addFace(std::move(f));
        for (auto [name, value] :
             std::array<std::pair<char const *, double>, 17>{{{"sub1", .2},
                                                              {"sub2", .16},
                                                              {"sup1", .4},
                                                              {"sup2", .3},
                                                              {"sup3", .5},
                                                              {"sub_drop", .1},
                                                              {"sup_drop", .1},
                                                              {"x_height", .4},
                                                              {"rule_thickness", .04},
                                                              {"separation_factor", 5},
                                                              {"big_op_spacing1", .1},
                                                              {"big_op_spacing2", .1},
                                                              {"big_op_spacing3", .1},
                                                              {"big_op_spacing4", .1},
                                                              {"big_op_spacing5", .15},
                                                              {"scriptspace", .05},
                                                              {"extra_ic", 0}}})
            b.addParameter("MathJaxNewcm", name, value);
        return std::move(b).freeze();
    }();
    return repo;
}

CommonOutputJax makeJax(mml::MmlDocument &d, Target &target, FontRepository const &fonts,
                        bool fixed) {
    ExtendedMetrics metrics{.em = 16,
                            .ex = 8,
                            .containerWidth = 640,
                            .scale = 1,
                            .family = "MathJaxNewcm",
                            .display = false};
    return CommonOutputJax(d, CommonOutputJaxConfig{metrics, {}, fonts, "MathJaxNewcm"}, target,
                           catalog(fixed),
                           [](CommonWrapperFactory &f) -> std::unique_ptr<Linebreaks> {
                               return std::make_unique<NoBreaks>(f);
                           });
}

mml::MmlNode &token(mml::MmlDocument &d, mml::NodeKind kind, std::u16string value) {
    auto &t = dynamic_cast<mml::TextNode &>(d.factory().make(mml::NodeKind::text));
    t.setText(std::move(value));
    mml::MmlNode *children[]{&t};
    return d.factory().make(kind, {}, children);
}

mml::MmlNode &makeNode(mml::MmlDocument &d, mml::NodeKind kind,
                       std::span<mml::MmlNode *const> children, std::u16string attr = {},
                       mml::AttributeValue value = {}) {
    auto &node = d.factory().make(kind, {}, children);
    if (!attr.empty())
        node.attributes().set(std::move(attr), std::move(value));
    return node;
}

std::array<double, 3> box(CommonWrapper &wrapper) {
    const auto b = wrapper.getOuterBBox();
    return {b.w, b.h, b.d};
}

CommonWrapper &wrapInMath(CommonOutputJax &jax, mml::MmlNode &node) {
    auto &document = node.document();
    const std::array<mml::MmlNode *, 1> children{&node};
    auto &math = document.factory().make(mml::NodeKind::math, {}, children);
    document.setRoot(math);
    (void)jax.factory().wrap(math);
    return *jax.wrapperFor(node);
}
} // namespace

#include "math/support/limits_observations.hpp"

namespace {
class RejectLimitsMeasurement final : public MeasurementTarget {
  public:
    ExtendedMetrics measureMetrics(MetricRequest const &) override {
        throw std::logic_error("limits Source input unexpectedly requested font metrics");
    }
    UnknownBBox measureUnknownText(UnknownTextRequest const &) override {
        throw std::logic_error("limits Source input unexpectedly measured unknown text");
    }
};

std::unique_ptr<Linebreaks> makeLimitsLinebreaks(CommonWrapperFactory &factory) {
    return std::make_unique<LinebreakVisitor>(factory);
}

bool close(double actual, double expected) {
    return std::abs(actual - expected) <= 1e-9;
}

bool matches(CommonWrapper &wrapper, LimitsSceneBBox const &expected) {
    const auto &box = wrapper.getBBox();
    return close(box.w, expected.w) && close(box.h, expected.h) && close(box.d, expected.d) &&
           close(box.scale, expected.scale) && close(box.rscale, expected.rscale) &&
           close(box.L, expected.L) && close(box.R, expected.R);
}

std::string writeLimitsScene(scene::Scene const &value,
                             folia::math::fonts::NewcmFontDataBundle const &bundle,
                             LimitsSceneSourceCase const &source) {
    svg::SvgWriteOptions options;
    options.font_resources = &bundle.svg_resources;
    options.font_cache =
        source.font_cache == "local" ? svg::FontCacheMode::local : svg::FontCacheMode::none;
    options.local_cache_prefixes.reserve(source.local_cache_prefixes.size());
    for (auto prefix : source.local_cache_prefixes)
        options.local_cache_prefixes.emplace_back(prefix);
    return svg::write(value, options);
}

struct LimitsSceneCapture {
    scene::Scene value;
    std::string before_retirement;
    bool boxes_match{};
};

LimitsSceneCapture captureLimitsScene(LimitsSceneSourceCase const &source,
                                      folia::math::fonts::NewcmFontDataBundle const &bundle) {
    std::optional<scene::Scene> completed;
    std::string before;
    bool boxesMatch = false;
    {
        mml::MmlDocument document;
        auto &root = mathml_scene_test::constructRoot(document, source.nodes, source.attributes);
        document.setRoot(root);
        root.setInheritedAttributes();
        root.setTeXclass(nullptr);
        RejectLimitsMeasurement measurement;
        CommonOptions options;
        options.displayOverflow = "linebreak";
        options.linebreaks.inlineBreaks = true;
        CommonOutputJaxConfig config{
            {source.em, source.ex, source.width, 1.0, "MathJaxNewcm", source.display},
            std::move(options),
            bundle.fonts,
            "MathJaxNewcm",
            InlineBreakPolicy::forced};
        LayoutJob job(document, std::move(config), measurement,
                      svg::SvgWrapperFactory::defaultNodes(), makeLimitsLinebreaks);
        auto &commonRoot = job.build();

        std::vector<mml::MmlNode *> flattened;
        const auto visit = [&](auto &&self, mml::MmlNode &node) -> void {
            flattened.push_back(&node);
            for (auto *child : node.children())
                if (child)
                    self(self, *child);
        };
        visit(visit, root);
        bool allBoxesMatch =
            flattened.size() == source.nodes.size() && source.limits_node_index < flattened.size();
        auto *rootWrapper = &commonRoot;
        auto *limitsWrapper = source.limits_node_index < flattened.size()
                                  ? job.jax().wrapperFor(*flattened[source.limits_node_index])
                                  : nullptr;
        if (source.limits_node_index < flattened.size())
            allBoxesMatch =
                allBoxesMatch && mml::nodeKindName(flattened[source.limits_node_index]->kind()) ==
                                     source.nodes[source.limits_node_index].kind;
        allBoxesMatch = allBoxesMatch && rootWrapper && limitsWrapper &&
                        matches(*rootWrapper, source.root_bbox) &&
                        matches(*limitsWrapper, source.limits_bbox);
        auto value = job.jax().buildScene(commonRoot);
        const auto &metrics = value.metrics();
        allBoxesMatch = allBoxesMatch && close(metrics.width, source.root_bbox.w) &&
                        close(metrics.height, source.root_bbox.h) &&
                        close(metrics.depth, source.root_bbox.d);
        before = writeLimitsScene(value, bundle, source);
        boxesMatch = allBoxesMatch;
        completed.emplace(std::move(value));
    }
    return {std::move(*completed), std::move(before), boxesMatch};
}
} // namespace

suite native_common_limits_scene = [] {
    "direct_MathML_limits_Source_cases_match_BBoxes_and_survive_retirement"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        expect(sourceLimitsSceneCases().size() == 24_u);
        for (auto const &source : sourceLimitsSceneCases()) {
            const auto captured = captureLimitsScene(source, bundle);
            expect(captured.boxes_match)
                << "root/limits BBoxes and Scene metrics match Source case=" << source.name;
            if (source.font_cache == "local")
                expect(!source.local_cache_prefixes.empty())
                    << "local Source case has captured prefixes case=" << source.name;
            else
                expect(source.local_cache_prefixes.empty())
                    << "none Source case has no prefixes case=" << source.name;
            const auto expectedSvg = source_svg_paint::materialize(source.svg);
            expect(captured.before_retirement == expectedSvg)
                << "complete Source SVG matches before retirement case=" << source.name;
            const auto after = writeLimitsScene(captured.value, bundle, source);
            expect(after == expectedSvg)
                << "complete Scene SVG survives document/job retirement case=" << source.name;
        }
    };
};

suite native_common_limits_factory = [] {
    "common_munder_matches_pinned_source_bbox_and_child_slot"_test = [] {
        Target target;
        mml::MmlDocument d;
        auto jax = makeJax(d, target, fixtureFonts(), true);
        auto &base = token(d, mml::NodeKind::mi, u"x");
        auto &under = token(d, mml::NodeKind::mi, u"i");
        mml::MmlNode *kids[]{&base, &under};
        auto &node = makeNode(d, mml::NodeKind::munder, kids);
        d.setRoot(node);
        auto &w = dynamic_cast<CommonMunder &>(wrapInMath(jax, node));
        const auto b = box(w);
        expect(w.scriptChild() == w.childNodes[1]);
        expect(dynamic_cast<CommonScriptbase *>(&w) != nullptr);
        expect(eq(b[0], .5_d));
        expect(eq(b[1], .7_d));
        expect(eq(b[2], .8500000000000001));
    };
    "common_mover_accent_matches_pinned_source_bbox"_test = [] {
        Target target;
        mml::MmlDocument d;
        auto jax = makeJax(d, target, fixtureFonts(), true);
        auto &base = token(d, mml::NodeKind::mi, u"x");
        auto &over = token(d, mml::NodeKind::mi, u"2");
        mml::MmlNode *kids[]{&base, &over};
        auto &node = makeNode(d, mml::NodeKind::mover, kids, u"accent", true);
        d.setRoot(node);
        auto &w = dynamic_cast<CommonMover &>(wrapInMath(jax, node));
        const auto b = box(w);
        expect(w.scriptChild() == w.childNodes[1]);
        expect(dynamic_cast<CommonScriptbase *>(&w) != nullptr);
        expect(eq(b[0], .5_d));
        expect(eq(b[1], 1.4499999999999997));
        expect(eq(b[2], .2_d));
    };
    "common_munderover_nonmovable_matches_isolated_common_mixin_probe"_test = [] {
        Target target;
        mml::MmlDocument d;
        auto jax = makeJax(d, target, fixtureFonts(), true);
        auto &base = token(d, mml::NodeKind::mi, u"x");
        auto &under = token(d, mml::NodeKind::mi, u"i");
        auto &over = token(d, mml::NodeKind::mi, u"2");
        mml::MmlNode *kids[]{&base, &under, &over};
        auto &node = makeNode(d, mml::NodeKind::munderover, kids);
        d.setRoot(node);
        auto &w = dynamic_cast<CommonMunderover &>(wrapInMath(jax, node));
        const auto b = box(w);
        expect(w.underChild() == w.childNodes[1]);
        expect(w.overChild() == w.childNodes[2]);
        // Source's CommonMunderover maps scriptChild to the over slot.
        expect(w.scriptChild() == w.overChild());
        expect(dynamic_cast<CommonScriptbase *>(&w) != nullptr);
        expect(eq(b[0], .5_d));
        expect(eq(b[1], 1.3499999999999999));
        expect(eq(b[2], .8500000000000001));
    };
    "common_munderover_movable_limits_match_concrete_Source_two_script_flow"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        Target target;
        mml::MmlDocument d;
        auto jax = makeJax(d, target, bundle.fonts, false);
        auto &integral = token(d, mml::NodeKind::mo, u"∫");
        integral.attributes().set(u"movablelimits", true);
        auto &under = token(d, mml::NodeKind::mi, u"i");
        auto &over = token(d, mml::NodeKind::mi, u"2");
        mml::MmlNode *kids[]{&integral, &under, &over};
        auto &node = makeNode(d, mml::NodeKind::munderover, kids, u"displaystyle", false);
        node.setInheritedAttributes();
        node.setTeXclass(nullptr);
        d.setRoot(node);
        auto &w = dynamic_cast<CommonMunderover &>(wrapInMath(jax, node));
        expect(w.hasMovableLimits());
        expect(w.scriptChild() == w.overChild());
        const auto result = box(w);
        // Pinned SVG actual-wrapper Source witness: movable munderover with
        // <mo movablelimits="true">∫</mo><mi>i</mi><mi>2</mi>, display=false.
        expect(eq(result[0], 1.1936158364337877));
        expect(eq(result[1], 1.0034898987322334));
        expect(eq(result[2], .34863351365237943));
    };
    "common_munder_movable_limits_match_concrete_Source_subscript_offsets"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        Target target;
        mml::MmlDocument d;
        auto jax = makeJax(d, target, bundle.fonts, false);
        auto &integral = token(d, mml::NodeKind::mo, u"∫");
        integral.attributes().set(u"movablelimits", true);
        auto &under = token(d, mml::NodeKind::mi, u"i");
        mml::MmlNode *kids[]{&integral, &under};
        auto &node = makeNode(d, mml::NodeKind::munder, kids, u"displaystyle", false);
        node.setInheritedAttributes();
        node.setTeXclass(nullptr);
        d.setRoot(node);
        auto &w = dynamic_cast<CommonMunder &>(wrapInMath(jax, node));
        expect(w.hasMovableLimits());
        // Pinned SVG actual-wrapper Source witness, same input as the Source
        // `munder` record in limits-source-expected.json.
        expect(eq(w.getOffset()[0], 0.0));
        expect(eq(w.getOffset()[1], -.3408553390593274));
        const auto result = box(w);
        expect(eq(result[0], .871951839509359));
        expect(eq(result[1], .8055));
        expect(eq(result[2], .34863351365237943));
    };
    "common_mover_movable_limits_match_concrete_Source_superscript_offsets"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        Target target;
        mml::MmlDocument d;
        auto jax = makeJax(d, target, bundle.fonts, false);
        auto &integral = token(d, mml::NodeKind::mo, u"∫");
        integral.attributes().set(u"movablelimits", true);
        auto &over = token(d, mml::NodeKind::mi, u"2");
        mml::MmlNode *kids[]{&integral, &over};
        auto &node = makeNode(d, mml::NodeKind::mover, kids, u"displaystyle", false);
        node.setInheritedAttributes();
        node.setTeXclass(nullptr);
        d.setRoot(node);
        auto &w = dynamic_cast<CommonMover &>(wrapInMath(jax, node));
        expect(w.hasMovableLimits());
        // Pinned SVG actual-wrapper Source witness, same input as the Source
        // `mover` record in limits-source-expected.json.
        expect(eq(w.getOffset()[0], .055999999999999994));
        expect(eq(w.getOffset()[1], .5325567824619927));
        const auto result = box(w);
        expect(eq(result[0], 1.1936158364337877));
        expect(eq(result[1], 1.0034898987322334));
        expect(eq(result[2], .30550000000000005));
    };
};
