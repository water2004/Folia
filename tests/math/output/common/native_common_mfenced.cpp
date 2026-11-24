#include "math/support/source_svg_paint.hpp"
#include "support/folia_test.hpp"

import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.math;
import folia.math.core.mml_tree.mml_nodes.mfenced;
import folia.math.core.mml_tree.mml_nodes.mspace;
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
        throw std::logic_error("mfenced fixture unexpectedly requested metrics");
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("mfenced fixture unexpectedly measured text");
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreakVisitor(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

common::CommonOptions sourceOptions() {
    common::CommonOptions options;
    options.displayOverflow = "linebreak";
    options.linebreaks.inlineBreaks = true;
    return options;
}

std::string serialize(scene::Scene const &value,
                      folia::math::fonts::NewcmFontDataBundle const &bundle) {
    svg::SvgWriteOptions options;
    options.font_cache = svg::FontCacheMode::none;
    options.font_resources = &bundle.svg_resources;
    return svg::write(value, options);
}

void reportSvgDifference(std::string_view caseName, std::string_view expected,
                         std::string_view actual) {
    std::size_t offset = 0;
    while (offset < expected.size() && offset < actual.size() && expected[offset] == actual[offset])
        ++offset;
    std::cerr << "mfenced SVG difference in " << caseName << " at byte " << offset << " (expected "
              << expected.size() << " bytes, actual " << actual.size() << " bytes)\n";
}

void expectMmlFenceParents(mml::MmlMfenced const &fenced) {
    for (auto *generated : {fenced.open, fenced.close})
        if (generated)
            expect(generated->parent() == &fenced) << "generated fence MML parent is unchanged";
    for (auto *separator : fenced.separators)
        if (separator)
            expect(separator->parent() == &fenced) << "generated separator MML parent is unchanged";
}

class ThrowingMspace final : public common::CommonMspace {
  public:
    using CommonMspace::CommonMspace;
    common::SceneNodes emitScene(scene::Builder &, std::span<scene::NodeId const>) override {
        throw std::runtime_error("expected child emission failure");
    }
};

common::CommonWrapperRegistration throwingMspaceRegistration() {
    using Args = common::WrapperBuildArguments;
    using Creator = folia::math::tree::FactoryClass<common::CommonWrapper, Args>;
    return {mml::NodeKind::mspace,
            Creator{[](Args const &a) -> std::unique_ptr<common::CommonWrapper> {
                        return std::make_unique<ThrowingMspace>(a.factory, a.node, a.parent);
                    },
                    [](common::CommonWrapper const &w) {
                        return dynamic_cast<ThrowingMspace const *>(&w) != nullptr;
                    }},
            common::CommonWrapperClassData::commonDefaults()};
}

void verifySourceMfenced(mathml_scene_test::Case const &expected,
                         folia::math::fonts::NewcmFontDataBundle const &bundle) {
    const auto expectedSvg = source_svg_paint::materialize(expected.source_svg);
    RejectMeasurement measurement;
    std::unique_ptr<scene::Scene> savedScene;
    {
        mml::MmlDocument document;
        auto &root =
            mathml_scene_test::constructRoot(document, expected.nodes, expected.attributes);
        document.setRoot(root);
        root.setInheritedAttributes();
        common::CommonOutputJaxConfig config{{expected.metrics.em, expected.metrics.ex,
                                              expected.metrics.container_width, 1, "MathJaxNewcm",
                                              expected.metrics.display},
                                             sourceOptions(),
                                             bundle.fonts,
                                             "MathJaxNewcm"};
        common::LayoutJob job(document, config, measurement,
                              common::CommonWrapperFactory::standardNodes(), makeLinebreakVisitor);
        savedScene = std::make_unique<scene::Scene>(job.buildScene());
        const auto actual = serialize(*savedScene, bundle);
        if (actual != expectedSvg)
            reportSvgDifference(expected.name, expectedSvg, actual);
        expect(actual == expectedSvg)
            << "case " << expected.name << ": serialized SVG matches pinned Source";
    }
    const auto afterOwnerDestruction = serialize(*savedScene, bundle);
    expect(afterOwnerDestruction == expectedSvg)
        << "case " << expected.name << ": frozen Scene survives document and LayoutJob destruction";
}
} // namespace

#include "math/fixtures/common-source/semantics-fenced-mathml-scene.inc"

suite native_common_mfenced = [] {
    "Mfenced rejects non-string typed delimiter inputs without changing the MML tree"_test = [] {
        const std::array<std::u16string_view, 3> fields{u"open", u"close", u"separators"};
        const std::array<mml::AttributeValue, 3> nonStrings{
            mml::AttributeNull{}, false, 7.0};

        for (const auto field : fields) {
            for (const auto &value : nonStrings) {
                mml::MmlDocument document;
                auto &fencedNode = document.factory().make(mml::NodeKind::mfenced);
                auto &fenced = dynamic_cast<mml::MmlMfenced &>(fencedNode);
                auto &content = document.factory().make(mml::NodeKind::mspace);
                fenced.appendChild(content);
                auto &root = document.factory().make(mml::NodeKind::math);
                root.appendChild(fenced);
                document.setRoot(root);
                expect(root.children().size() == 1_u);
                if (root.children().size() != 1)
                    return;
                auto *const originalRootChild = root.children().front();
                auto *const originalParent = fenced.parent();
                const auto originalSize = document.size();
                expect(originalParent == originalRootChild);
                fenced.attributes().set(std::u16string(field), value);

                expect(throws<std::invalid_argument>([&] { fenced.setInheritedAttributes(); }))
                    << "non-string delimiter field is rejected: " << field.size();
                expect(fenced.open == nullptr);
                expect(fenced.close == nullptr);
                expect(fenced.separators.empty());
                expect(fenced.children().size() == 1_u);
                if (fenced.children().size() != 1)
                    return;
                expect(fenced.children()[0] == &content);
                expect(content.parent() == &fenced);
                expect(fenced.parent() == originalParent);
                expect(root.children().size() == 1_u);
                if (root.children().size() != 1)
                    return;
                expect(root.children()[0] == originalRootChild);
                expect(document.size() == originalSize)
                    << "delimiter validation fails before fake text or operator nodes are adopted";
            }
        }
    };

    "mfenced Scene serialization matches pinned Source MathML cases"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        for (const auto &expected : sourceMfencedCases) {
            try {
                verifySourceMfenced(expected, bundle);
            } catch (const std::exception &error) {
                expect(false) << "case " << expected.name << " threw: " << error.what();
            }
        }
    };

    "mfenced restores exact child wrapper parents after successful emission"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument document;
        auto &space = document.factory().make(mml::NodeKind::mspace);
        const std::array<mml::MmlNode *, 1> fencedChildren{&space};
        auto &fencedNode = document.factory().make(mml::NodeKind::mfenced, {}, fencedChildren);
        auto &fencedMml = dynamic_cast<mml::MmlMfenced &>(fencedNode);
        const std::array<mml::MmlNode *, 1> mathChildren{&fencedNode};
        auto &mathNode = document.factory().make(mml::NodeKind::math, {}, mathChildren);
        document.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", false}, sourceOptions(), bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(document, config, measurement,
                              common::CommonWrapperFactory::standardNodes(), makeLinebreakVisitor);
        (void)job.build();
        auto *fenced = dynamic_cast<common::CommonMfenced *>(job.jax().wrapperFor(fencedNode));
        auto *child = job.jax().wrapperFor(space);
        expect(fenced != nullptr) << "success case has a CommonMfenced wrapper";
        expect(child != nullptr) << "success case has its direct child wrapper";
        if (!fenced || !child)
            return;

        child->parent = nullptr;
        const std::array<scene::NodeId, 1> parents{0};
        scene::Builder builder;
        (void)fenced->emitScene(builder, parents);
        expect(child->parent == nullptr) << "success restores an originally null wrapper parent";
        expect(space.parent() == &fencedNode) << "success preserves the real child's MML parent";
        expectMmlFenceParents(fencedMml);
    };

    "mfenced restores exact child wrapper parents when child emission throws"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument document;
        auto &space = document.factory().make(mml::NodeKind::mspace);
        const std::array<mml::MmlNode *, 1> fencedChildren{&space};
        auto &fencedNode = document.factory().make(mml::NodeKind::mfenced, {}, fencedChildren);
        auto &fencedMml = dynamic_cast<mml::MmlMfenced &>(fencedNode);
        const std::array<mml::MmlNode *, 1> mathChildren{&fencedNode};
        auto &mathNode = document.factory().make(mml::NodeKind::math, {}, mathChildren);
        document.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", false}, sourceOptions(), bundle.fonts, "MathJaxNewcm"};
        auto catalog = common::CommonWrapperFactory::standardNodes();
        auto found = std::find_if(catalog.begin(), catalog.end(), [](auto const &entry) {
            return entry.kind == mml::NodeKind::mspace;
        });
        expect(found != catalog.end()) << "standard catalog contains the mspace registration";
        if (found == catalog.end())
            return;
        *found = throwingMspaceRegistration();
        common::LayoutJob job(document, config, measurement, std::move(catalog),
                              makeLinebreakVisitor);
        auto &rootWrapper = job.build();
        auto *fenced = dynamic_cast<common::CommonMfenced *>(job.jax().wrapperFor(fencedNode));
        auto *child = job.jax().wrapperFor(space);
        auto *math = dynamic_cast<common::CommonMath *>(&rootWrapper);
        expect(fenced != nullptr) << "throw case has a CommonMfenced wrapper";
        expect(child != nullptr) << "throw case has its direct child wrapper";
        expect(math != nullptr) << "LayoutJob returns a CommonMath root wrapper";
        if (!fenced || !child || !math)
            return;

        child->parent = math;
        const std::array<scene::NodeId, 1> parents{0};
        scene::Builder builder;
        expect(throws<std::runtime_error>([&] { (void)fenced->emitScene(builder, parents); }))
            << "child emitter propagates its exception";
        expect(child->parent == math) << "throw restores the exact non-null wrapper parent";
        expect(space.parent() == &fencedNode) << "throw preserves the real child's MML parent";
        expectMmlFenceParents(fencedMml);
    };
};
