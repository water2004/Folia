#include "support/folia_test.hpp"

import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.node_kind;
import folia.math.output.common;

namespace {
using namespace boost::ut;
using namespace folia::math;
using namespace folia::math::output::common;

struct ProbeCounts {
    std::size_t initialized = 0;
    std::size_t destroyed = 0;
    int destructionOrder = 0;
    int visitorDestroyedAt = 0;
    int wrapperDestroyedAt = 0;
};

class ProbeWrapper final : public CommonWrapper {
    ProbeCounts &counts_;

  public:
    ProbeWrapper(CommonWrapperFactory &factory, mml::MmlNode &node, CommonWrapper *parent,
                 ProbeCounts &counts)
        : CommonWrapper(factory, node, parent), counts_(counts) {
    }

    void initializeSource() override {
        ++counts_.initialized;
        CommonWrapper::initializeSource();
    }

    ~ProbeWrapper() override {
        ++counts_.destroyed;
        counts_.wrapperDestroyedAt = ++counts_.destructionOrder;
    }
};

class NoopMeasurement final : public MeasurementTarget {
  public:
    ExtendedMetrics measureMetrics(MetricRequest const &) override {
        return {};
    }

    UnknownBBox measureUnknownText(UnknownTextRequest const &) override {
        return {};
    }
};

class ProbeLinebreaks final : public Linebreaks {
  public:
    inline static std::size_t constructed = 0;
    inline static double lastWidth = 0;
    inline static ProbeCounts *destructionCounts = nullptr;

    explicit ProbeLinebreaks(CommonWrapperFactory &factory) : Linebreaks(factory) {
        ++constructed;
    }

    ~ProbeLinebreaks() override {
        if (destructionCounts) {
            destructionCounts->visitorDestroyedAt = ++destructionCounts->destructionOrder;
        }
    }

    void breakToWidth(CommonWrapper &, double width) override {
        lastWidth = width;
    }
};

std::unique_ptr<Linebreaks> makeProbeLinebreaks(CommonWrapperFactory &factory) {
    return std::make_unique<ProbeLinebreaks>(factory);
}

FontRepository makeTestFonts() {
    FontRepositoryBuilder builder;
    FontFaceData face;
    face.name = "FactoryTest";
    builder.addFace(std::move(face));
    builder.addParameter("FactoryTest", "x_height", 0.45);
    return std::move(builder).freeze();
}

CommonOutputJaxConfig configFor(FontRepository const &fonts) {
    ExtendedMetrics metrics;
    metrics.em = 16;
    metrics.ex = 7.2;
    metrics.containerWidth = 800;
    CommonOptions options;
    return {std::move(metrics), std::move(options), fonts, "FactoryTest"};
}

CommonWrapperRegistration probeRegistration(ProbeCounts &counts) {
    CommonWrapperRegistration result;
    result.kind = mml::NodeKind::math;
    result.data = CommonWrapperClassData::commonDefaults();
    result.creator.construct = [&counts](WrapperBuildArguments const &args) {
        return std::make_unique<ProbeWrapper>(args.factory, args.node, args.parent, counts);
    };
    result.creator.isInstance = [](CommonWrapper const &wrapper) {
        return dynamic_cast<ProbeWrapper const *>(&wrapper) != nullptr;
    };
    return result;
}

CommonWrapperCatalog probeCatalog(ProbeCounts &counts) {
    CommonWrapperCatalog catalog;
    catalog.push_back(probeRegistration(counts));
    auto childRegistration = probeRegistration(counts);
    childRegistration.kind = mml::NodeKind::mi;
    catalog.push_back(std::move(childRegistration));
    auto rowRegistration = probeRegistration(counts);
    rowRegistration.kind = mml::NodeKind::inferred_mrow;
    catalog.push_back(std::move(rowRegistration));
    return catalog;
}

const suite<"native-raii-common-factory"> common_factory_tests = [] {
    "common-defaults-match-source-values"_test = [] {
        const auto defaults = CommonWrapperClassData::commonDefaults();
        expect(eq(defaults.removeStyles.size(), 6_u));
        expect(defaults.removeStyles[0] == "fontSize");
        expect(defaults.removeStyles[5] == "font");
        expect(eq(defaults.skipAttributes.size(), 10_u));
        expect(defaults.skipAttributes.at("xmlns"));
        expect(defaults.boldVariants.at("bold").at("sans-serif-italic") ==
               "sans-serif-bold-italic");
        expect(defaults.boldVariants.at("normal").at("bold-script") == "script");
        expect(defaults.italicVariants.at("normal").at("sans-serif-bold-italic") ==
               "bold-sans-serif");
        expect(defaults.styles.empty());
    };

    "common-default-variant-maps-preserve-all-values-and-independent-ownership"_test = [] {
        auto first = CommonWrapperClassData::commonDefaults();
        const auto second = CommonWrapperClassData::commonDefaults();
        const decltype(first.boldVariants) expectedBold{
            {"bold", {{"normal", "bold"}, {"italic", "bold-italic"},
                      {"fraktur", "bold-fraktur"}, {"script", "bold-script"},
                      {"sans-serif", "bold-sans-serif"},
                      {"sans-serif-italic", "sans-serif-bold-italic"}}},
            {"normal", {{"bold", "normal"}, {"bold-italic", "italic"},
                        {"bold-fraktur", "fraktur"}, {"bold-script", "script"},
                        {"bold-sans-serif", "sans-serif"},
                        {"sans-serif-bold-italic", "sans-serif-italic"}}}};
        const decltype(first.italicVariants) expectedItalic{
            {"italic", {{"normal", "italic"}, {"bold", "bold-italic"},
                        {"sans-serif", "sans-serif-italic"},
                        {"bold-sans-serif", "sans-serif-bold-italic"}}},
            {"normal", {{"italic", "normal"}, {"bold-italic", "bold"},
                        {"sans-serif-italic", "sans-serif"},
                        {"sans-serif-bold-italic", "bold-sans-serif"}}}};
        expect(first.boldVariants == expectedBold);
        expect(first.italicVariants == expectedItalic);
        auto copy = first;
        first.boldVariants.at("bold").at("normal") = "custom-bold";
        first.italicVariants.erase("normal");
        expect(second.boldVariants == expectedBold);
        expect(second.italicVariants == expectedItalic);
        expect(copy.boldVariants == expectedBold);
        expect(copy.italicVariants == expectedItalic);
        copy.boldVariants.clear();
        expect(first.boldVariants.at("bold").at("normal") == "custom-bold");
        expect(eq(first.italicVariants.size(), 1_u));
    };

    "adopt-initializes-once-and-owns-until-job-destruction"_test = [] {
        ProbeCounts counts;
        ProbeLinebreaks::constructed = 0;
        ProbeLinebreaks::lastWidth = 0;
        ProbeLinebreaks::destructionCounts = &counts;
        auto fonts = makeTestFonts();
        mml::MmlDocument document;
        auto &root = document.factory().make(mml::NodeKind::math);
        document.setRoot(root);
        NoopMeasurement measurement;

        {
            LayoutJob job(document, configFor(fonts), measurement, probeCatalog(counts),
                          &makeProbeLinebreaks);
            expect(eq(ProbeLinebreaks::constructed, 1_u));
            auto product = std::make_unique<ProbeWrapper>(job.factory(), root, nullptr, counts);
            auto *const address = product.get();
            CommonWrapper &adopted = job.factory().adopt(std::move(product));

            expect(&adopted == address);
            expect(eq(counts.initialized, 2_u));
            expect(eq(job.factory().wrapperCount(), 2_u));
            // Direct adoption is ownership/initialization only. Source wrapper
            // traversal owns node indexing and parent-child linkage.
            expect(job.jax().wrapperFor(root) == nullptr);
            job.jax().linebreaks().breakToWidth(adopted, 42.5);
            expect(eq(ProbeLinebreaks::lastWidth, 42.5));
        }

        expect(eq(counts.destroyed, 2_u));
        expect(counts.visitorDestroyedAt < counts.wrapperDestroyedAt);
        ProbeLinebreaks::destructionCounts = nullptr;
    };

    "generic-source-catalog-is-empty-and-no-generic-fallback-exists"_test = [] {
        const auto catalog = CommonWrapperFactory::defaultNodes();
        expect(catalog.empty());

        auto fonts = makeTestFonts();
        mml::MmlDocument document;
        auto &root = document.factory().make(mml::NodeKind::math);
        document.setRoot(root);
        NoopMeasurement measurement;
        LayoutJob job(document, configFor(fonts), measurement, {}, &makeProbeLinebreaks);
        bool rejected = false;
        try {
            (void)job.factory().wrap(root);
        } catch (std::out_of_range const &) {
            rejected = true;
        }
        expect(rejected);
        expect(eq(job.factory().wrapperCount(), 0_u));
    };

    "build-wraps-and-indexes-children-through-source-wrapper-boundary"_test = [] {
        ProbeCounts counts;
        ProbeLinebreaks::constructed = 0;
        ProbeLinebreaks::destructionCounts = nullptr;
        auto fonts = makeTestFonts();
        mml::MmlDocument document;
        auto &firstChild = document.factory().make(mml::NodeKind::mi);
        auto &secondChild = document.factory().make(mml::NodeKind::mi);
        const std::array<mml::MmlNode *, 2> children{&firstChild, &secondChild};
        auto &root = document.factory().make(mml::NodeKind::math, {}, children);
        document.setRoot(root);
        NoopMeasurement measurement;

        LayoutJob job(document, configFor(fonts), measurement, probeCatalog(counts),
                      &makeProbeLinebreaks);
        CommonWrapper &wrappedRoot = job.build();
        auto *const wrappedFirst = job.jax().wrapperFor(firstChild);
        auto *const wrappedSecond = job.jax().wrapperFor(secondChild);
        expect(wrappedFirst != nullptr);
        expect(wrappedSecond != nullptr);
        expect(eq(job.factory().wrapperCount(), 4_u));
        expect(eq(counts.initialized, 4_u));
        expect(eq(wrappedRoot.childNodes.size(), 1_u));
        auto &row = *wrappedRoot.childNodes.front();
        expect(row.node == root.children().front());
        expect(eq(row.childNodes.size(), 2_u));
        expect(row.childNodes[0] == wrappedFirst);
        expect(row.childNodes[1] == wrappedSecond);
        // The Source constructor's root wrapper is returned by build, while
        // the node map is populated by CommonWrapper::wrap for descendants.
        expect(job.jax().wrapperFor(root) == nullptr);
    };
};
} // namespace
