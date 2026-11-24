#include "support/folia_test.hpp"
import folia.math.output.common;
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.msubsup;
import folia.math.core.mml_tree.mml_nodes.mmultiscripts;
import folia.math.fonts.newcm_font_module_provider;

namespace {
using namespace folia::math;
using namespace folia::math::output::common;
using namespace boost::ut;
namespace mml = folia::math::mml;

struct Target final : MeasurementTarget {
    ExtendedMetrics measureMetrics(MetricRequest const &request) override {
        return {.em = 16,
                .ex = 8,
                .containerWidth = 640,
                .scale = 1,
                .family = request.family,
                .display = request.display};
    }

    UnknownBBox measureUnknownText(UnknownTextRequest const &) override {
        throw std::logic_error("script fixture uses known NewCM glyphs");
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
        c.push_back(registration<CommonInferredMrow>(K::inferred_mrow));
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
             std::array<std::pair<char const *, double>, 16>{{{"sub1", .2},
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

std::array<mml::MmlNode *, 2> subChildren(mml::MmlDocument &d) {
    return {&token(d, mml::NodeKind::mi, u"x"), &token(d, mml::NodeKind::mi, u"i")};
}

CommonWrapperCatalog sourceRootCatalog() {
    auto result = catalog(false); // includes CommonInferredMrow and real MML token wrappers
    result.push_back(registration<CommonMath>(mml::NodeKind::math));
    return result;
}

CommonOutputJaxConfig sourceScriptConfig(FontRepository const &fonts) {
    ExtendedMetrics metrics{.em = 16,
                            .ex = 8,
                            .containerWidth = 640,
                            .scale = 1,
                            .family = "MathJaxNewcm",
                            .display = false};
    CommonOptions options;
    options.linebreaks.inlineBreaks = true;
    return {metrics, std::move(options), fonts, "MathJaxNewcm", InlineBreakPolicy::forced};
}

mml::MmlNode &sourceMathRoot(mml::MmlDocument &d, mml::MmlNode &script) {
    mml::MmlNode *scriptChildren[]{&script};
    auto &inferred = d.factory().make(mml::NodeKind::inferred_mrow, {}, scriptChildren);
    mml::MmlNode *mathChildren[]{&inferred};
    auto &math = d.factory().make(mml::NodeKind::math, {}, mathChildren);
    d.setRoot(math);
    math.setInheritedAttributes(); // MathML input's inherited-attribute pass
    return math;                   // LayoutJob::build supplies the output TeXclass pass
}

void expectSourceBox(CommonWrapper &wrapper, double w, double h, double d, double ic, double sk,
                     double dx, double rscale, double scale) {
    const auto box = wrapper.getOuterBBox();
    auto check = [&](std::string_view field, double actual, double expected) {
        expect(actual == expected)
            << static_cast<unsigned>(wrapper.kind()) << field
            << std::format("actual={:.17g} expected={:.17g}", actual, expected);
    };
    check("w", box.w, w);
    check("h", box.h, h);
    check("d", box.d, d);
    check("ic", box.ic, ic);
    check("sk", box.sk, sk);
    check("dx", box.dx, dx);
    check("rscale", box.rscale, rscale);
    check("scale", box.scale, scale);
}

} // namespace

suite native_common_scripts_factory = [] {
    "common_factory_msub_matches_pinned_source_numeric_offset"_test = [] {
        Target target;
        mml::MmlDocument d;
        auto jax = makeJax(d, target, fixtureFonts(), true);
        auto kids = subChildren(d);
        auto &node = d.factory().make(mml::NodeKind::msub, {}, kids);
        d.setRoot(node);
        auto &w = dynamic_cast<CommonMsub &>(jax.factory().wrap(node));
        const auto offset = w.getOffset();
        expect(w.baseCore == w.childNodes[0]);
        expect(w.scriptChild() == w.childNodes[1]);
        expect(eq(offset[0], 0.0_d));
        expect(eq(offset[1], -.2_d));
        expect(eq(w.getDelta(*w.scriptChild()), .06_d));
    };

    "common_factory_msup_matches_pinned_source_numeric_offset"_test = [] {
        Target target;
        mml::MmlDocument d;
        auto jax = makeJax(d, target, fixtureFonts(), true);
        auto kids = subChildren(d);
        auto &node = d.factory().make(mml::NodeKind::msup, {}, kids);
        d.setRoot(node);
        auto &w = dynamic_cast<CommonMsup &>(jax.factory().wrap(node));
        const auto offset = w.getOffset();
        expect(w.scriptChild() == w.childNodes[1]);
        expect(eq(offset[0], .054000000000000006));
        expect(eq(offset[1], .3_d));
    };

    "common_factory_msubsup_matches_pinned_source_uvq"_test = [] {
        Target target;
        mml::MmlDocument d;
        auto jax = makeJax(d, target, fixtureFonts(), true);
        auto &base = token(d, mml::NodeKind::mi, u"x");
        auto &sub = token(d, mml::NodeKind::mi, u"i");
        auto &sup = token(d, mml::NodeKind::mi, u"2");
        mml::MmlNode *kids[]{&base, &sub, &sup};
        auto &node = d.factory().make(mml::NodeKind::msubsup, {}, kids);
        d.setRoot(node);
        auto &w = dynamic_cast<CommonMsubsup &>(jax.factory().wrap(node));
        const auto uvq = w.getUVQ();
        expect(w.subChild() == w.childNodes[1]);
        expect(w.supChild() == w.childNodes[2]);
        expect(eq(uvq[0], .42000000000000004));
        expect(eq(uvq[1], -.09999999999999992));
        expect(eq(uvq[2], .12_d));
        const auto cached = w.getUVQ();
        expect(eq(cached[0], uvq[0]));
        expect(eq(cached[1], uvq[1]));
        expect(eq(cached[2], uvq[2]));
    };

    "common_factory_mmultiscripts_preserves_pinned_list_counts_and_uvq"_test = [] {
        Target target;
        mml::MmlDocument d;
        auto jax = makeJax(d, target, fixtureFonts(), true);
        auto &base = token(d, mml::NodeKind::mi, u"x");
        auto &sub = token(d, mml::NodeKind::mi, u"i");
        auto &sup = token(d, mml::NodeKind::mi, u"2");
        auto &marker = d.factory().make(mml::NodeKind::mprescripts);
        auto &psub = token(d, mml::NodeKind::mi, u"j");
        auto &psup = token(d, mml::NodeKind::mi, u"3");
        mml::MmlNode *kids[]{&base, &sub, &sup, &marker, &psub, &psup};
        auto &node = d.factory().make(mml::NodeKind::mmultiscripts, {}, kids);
        d.setRoot(node);
        auto &w = dynamic_cast<CommonMmultiscripts &>(jax.factory().wrap(node));
        const auto &data = w.scriptsData();
        expect(data.numScripts == 1_u);
        expect(data.numPrescripts == 1_u);
        expect(w.firstPrescript() == 4_u);
        const auto uvq = w.getCombinedUV();
        expect(eq(uvq[0], .42000000000000004));
        expect(eq(uvq[1], -.09999999999999992));
        expect(eq(uvq[2], .12_d));
    };

    "real_newcm_provider_runs_independent_source_script_trees"_test = [] {
        // Provider lifetime is owned by this test. LayoutJob and every wrapper die
        // before `bundle`; no test-lambda reference capture keeps a local font bundle.
        auto bundle = folia::math::fonts::makeNewcmFontData();
        Target target;
        const auto wrappers = sourceRootCatalog();
        auto makeLinebreaks = [](CommonWrapperFactory &factory) -> std::unique_ptr<Linebreaks> {
            return std::make_unique<LinebreakVisitor>(factory);
        };
        constexpr double scriptScale = 0.7071067811865475;

        {
            mml::MmlDocument d;
            auto &base = token(d, mml::NodeKind::mi, u"x");
            auto &sub = token(d, mml::NodeKind::mi, u"i");
            mml::MmlNode *children[]{&base, &sub};
            auto &node = d.factory().make(mml::NodeKind::msub, {}, children);
            auto &root = sourceMathRoot(d, node);
            LayoutJob job(d, sourceScriptConfig(bundle.fonts), target, wrappers, makeLinebreaks);
            auto &math = dynamic_cast<CommonMath &>(job.build());
            expect(root.properties().layout.inline_marked.value_or(false));
            expect(root.properties().layout.inline_forced.value_or(false));
            auto &row = dynamic_cast<CommonInferredMrow &>(*math.childNodes[0]);
            auto &w = dynamic_cast<CommonMsub &>(*row.childNodes[0]);
            const auto offset = w.getOffset();
            expect(offset[0] == 0.0);
            expect(offset[1] == -0.15);
            expectSourceBox(*w.childNodes[0], .572, .442, .011, 0, .043, 0, 1, 1);
            expectSourceBox(*w.childNodes[1], .345, .661, .011, 0, .074, 0, scriptScale,
                            scriptScale);
            expectSourceBox(w, .8989518395093589, .442, .15777817459305202, 0, 0, 0, 1, 1);
            expectSourceBox(math, .8989518395093589, .442, .15777817459305202, 0, 0, 0, 1, 1);
        }
        {
            mml::MmlDocument d;
            auto &base = token(d, mml::NodeKind::mi, u"x");
            auto &sup = token(d, mml::NodeKind::mn, u"2");
            mml::MmlNode *children[]{&base, &sup};
            auto &node = d.factory().make(mml::NodeKind::msup, {}, children);
            auto &root = sourceMathRoot(d, node);
            LayoutJob job(d, sourceScriptConfig(bundle.fonts), target, wrappers, makeLinebreaks);
            auto &math = dynamic_cast<CommonMath &>(job.build());
            expect(root.properties().layout.inline_marked.value_or(false));
            expect(root.properties().layout.inline_forced.value_or(false));
            auto &row = dynamic_cast<CommonInferredMrow &>(*math.childNodes[0]);
            auto &w = dynamic_cast<CommonMsup &>(*row.childNodes[0]);
            const auto offset = w.getOffset();
            expect(offset[0] == 0.0);
            expect(offset[1] == .363);
            expectSourceBox(*w.childNodes[0], .572, .442, .011, 0, .043, 0, 1, 1);
            expectSourceBox(*w.childNodes[1], .5, .666, 0, 0, 0, 0, scriptScale, scriptScale);
            expectSourceBox(w, 1.0085533905932738, .8339331162702406, .011, 0, 0, 0, 1, 1);
            expectSourceBox(math, 1.0085533905932738, .8339331162702406, .011, 0, 0, 0, 1, 1);
        }
        {
            mml::MmlDocument d;
            auto &base = token(d, mml::NodeKind::mi, u"x");
            auto &sub = token(d, mml::NodeKind::mi, u"i");
            auto &sup = token(d, mml::NodeKind::mn, u"2");
            mml::MmlNode *children[]{&base, &sub, &sup};
            auto &node = d.factory().make(mml::NodeKind::msubsup, {}, children);
            auto &root = sourceMathRoot(d, node);
            LayoutJob job(d, sourceScriptConfig(bundle.fonts), target, wrappers, makeLinebreaks);
            auto &math = dynamic_cast<CommonMath &>(job.build());
            expect(root.properties().layout.inline_marked.value_or(false));
            expect(root.properties().layout.inline_forced.value_or(false));
            auto &row = dynamic_cast<CommonInferredMrow &>(*math.childNodes[0]);
            auto &w = dynamic_cast<CommonMsubsup &>(*row.childNodes[0]);
            const auto uvq = w.getUVQ();
            expect(uvq[0] == .363);
            expect(uvq[1] == -.2843975823643079);
            expect(uvq[2] == .18);
            expectSourceBox(*w.childNodes[0], .572, .442, .011, 0, .043, 0, 1, 1);
            expectSourceBox(*w.childNodes[1], .345, .661, .011, 0, .074, 0, scriptScale,
                            scriptScale);
            expectSourceBox(*w.childNodes[2], .5, .666, 0, 0, 0, 0, scriptScale, scriptScale);
            expectSourceBox(w, 1.0085533905932738, .8339331162702406, .2921757569573599, 0, 0, 0, 1,
                            1);
            expectSourceBox(math, 1.0085533905932738, .8339331162702406, .2921757569573599, 0, 0, 0,
                            1, 1);
        }
        {
            mml::MmlDocument d;
            auto &base = token(d, mml::NodeKind::mi, u"x");
            auto &sub = token(d, mml::NodeKind::mi, u"i");
            auto &sup = token(d, mml::NodeKind::mn, u"2");
            auto &marker = d.factory().make(mml::NodeKind::mprescripts);
            auto &psub = token(d, mml::NodeKind::mi, u"j");
            auto &psup = token(d, mml::NodeKind::mn, u"3");
            mml::MmlNode *children[]{&base, &sub, &sup, &marker, &psub, &psup};
            auto &node = d.factory().make(mml::NodeKind::mmultiscripts, {}, children);
            auto &root = sourceMathRoot(d, node);
            LayoutJob job(d, sourceScriptConfig(bundle.fonts), target, wrappers, makeLinebreaks);
            auto &math = dynamic_cast<CommonMath &>(job.build());
            expect(root.properties().layout.inline_marked.value_or(false));
            expect(root.properties().layout.inline_forced.value_or(false));
            auto &row = dynamic_cast<CommonInferredMrow &>(*math.childNodes[0]);
            auto &w = dynamic_cast<CommonMmultiscripts &>(*row.childNodes[0]);
            const auto &data = w.scriptsData();
            expect(data.numScripts == 1_u);
            expect(data.numPrescripts == 1_u);
            expect(w.firstPrescript() == 4_u);
            const auto uvq = w.getCombinedUV();
            expect(uvq[0] == .36915634918610407);
            expect(uvq[1] == -.29379758236430786);
            expect(uvq[2] == .18);
            expectSourceBox(*w.childNodes[0], .572, .442, .011, 0, .043, 0, 1, 1);
            expectSourceBox(*w.childNodes[1], .345, .661, .011, 0, .074, 0, scriptScale,
                            scriptScale);
            expectSourceBox(*w.childNodes[2], .5, .666, 0, 0, 0, 0, scriptScale, scriptScale);
            expectSourceBox(*w.childNodes[3], 0, 0, 0, 0, 0, 0, 1, 1);
            expectSourceBox(*w.childNodes[4], .412, .661, .205, 0, .153, 0, scriptScale,
                            scriptScale);
            expectSourceBox(*w.childNodes[5], .5, .666, .022, 0, 0, 0, scriptScale, scriptScale);
            expectSourceBox(w, 1.3791067811865474, .8400894654563447, .43875447250755006, 0, 0, 0,
                            1, 1);
            expectSourceBox(math, 1.3791067811865474, .8400894654563447, .43875447250755006, 0, 0,
                            0, 1, 1);
        }
    };
};
