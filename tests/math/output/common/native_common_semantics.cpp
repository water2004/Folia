#include "math/support/source_svg_paint.hpp"
#include "support/folia_test.hpp"
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.svg.writer;

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace mathml = folia::math::mml;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

#include "math/support/mathml_scene.hpp"
#include "math/fixtures/common-source/semantics-fenced-mathml-scene.inc"

namespace {
class RejectMeasurement final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("semantics fixture unexpectedly requested metrics");
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("semantics fixture unexpectedly measured unknown text");
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

struct CapturedScene {
    scene::Scene value;
    std::string before_retirement;
    bool embellished_break{};
};

CapturedScene captureSourceCase(mathml_scene_test::Case const &source,
                                folia::math::fonts::NewcmFontDataBundle const &bundle) {
    RejectMeasurement measurement;
    mathml::MmlDocument document;
    auto &root = mathml_scene_test::constructRoot(document, source.nodes, source.attributes);
    document.setRoot(root);
    root.setInheritedAttributes();

    common::CommonOutputJaxConfig config{{source.metrics.em, source.metrics.ex,
                                          source.metrics.container_width, 1.0, "MathJaxNewcm",
                                          source.metrics.display},
                                         sourceOptions(),
                                         bundle.fonts,
                                         "MathJaxNewcm",
                                         common::InlineBreakPolicy::forced};
    common::LayoutJob job(document, std::move(config), measurement,
                          common::CommonWrapperFactory::standardNodes(), makeLinebreakVisitor);
    auto result = job.buildScene();
    auto before = svg::write(
        result, {.font_cache = svg::FontCacheMode::none, .font_resources = &bundle.svg_resources});
    bool embellishedBreak = false;
    const auto inspect = [&](auto &&self, mathml::MmlNode &node) -> void {
        if (node.kind() == mathml::NodeKind::semantics && node.isEmbellished()) {
            auto *wrapper = dynamic_cast<common::CommonSemantics *>(job.jax().wrapperFor(node));
            if (wrapper && wrapper->breakCount() != 0)
                embellishedBreak = true;
        }
        for (auto *child : node.children())
            if (child)
                self(self, *child);
    };
    inspect(inspect, root);
    return {std::move(result), std::move(before), embellishedBreak};
}
} // namespace

suite common_semantics_native = [] {
    "pinned_MathML_semantics_emits_only_its_presentation_child_and_survives_retirement"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        bool foundEmbellishedBreak = false;
        for (auto const &source : sourceSemanticsCases) {
            auto captured = captureSourceCase(source, bundle);
            const auto expectedSvg = source_svg_paint::materialize(source.source_svg);
            expect(captured.before_retirement == expectedSvg) << "case " << source.name;
            const auto after_retirement =
                svg::write(captured.value, {.font_cache = svg::FontCacheMode::none,
                                            .font_resources = &bundle.svg_resources});
            expect(after_retirement == expectedSvg) << "retired case " << source.name;
            expect(after_retirement.find("data-mml-node=\"annotation\"") == std::string::npos)
                << "annotation is not emitted in " << source.name;
            expect(after_retirement.find("data-mml-node=\"annotation-xml\"") == std::string::npos)
                << "annotation-xml is not emitted in " << source.name;
            foundEmbellishedBreak = foundEmbellishedBreak || captured.embellished_break;
        }
        expect(foundEmbellishedBreak) << "Source semantics cases include an embellished break";
    };

    "semantics_with_no_children_emits_only_its_standard_container"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mathml::MmlDocument document;
        auto &node = document.factory().make(mathml::NodeKind::semantics);
        document.setRoot(node);
        node.setInheritedAttributes();
        common::CommonOutputJaxConfig config{{16, 8, 640, 1, "MathJaxNewcm", false},
                                             sourceOptions(),
                                             bundle.fonts,
                                             "MathJaxNewcm",
                                             common::InlineBreakPolicy::forced};
        common::LayoutJob job(document, std::move(config), measurement,
                              common::CommonWrapperFactory::standardNodes(), makeLinebreakVisitor);
        expect(node.kind() == mathml::NodeKind::semantics) << "empty test node kind";
        auto &baseWrapper = job.factory().wrap(node);
        auto *wrapper = dynamic_cast<common::CommonSemantics *>(&baseWrapper);
        expect(wrapper != nullptr) << "empty test has CommonSemantics wrapper";
        if (!wrapper)
            return;
        // setInheritedAttributes repairs arity-one semantics with an empty mrow;
        // clear wrappers here to exercise the emitter's explicit no-child branch.
        wrapper->childNodes.clear();
        scene::Builder builder;
        const std::array<scene::NodeId, 1> parents{0};
        auto nodes = wrapper->emitScene(builder, parents);
        expect(nodes.size() == 1_u);
        expect(nodes.front().has_value());
        if (nodes.front())
            expect(builder.entry_count(*nodes.front()) == 0_u)
                << "empty semantics standard container has no child entries";
    };

    "semantics_with_null_presentation_child_fails_explicitly"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mathml::MmlDocument document;
        auto &node = document.factory().make(mathml::NodeKind::semantics);
        document.setRoot(node);
        node.setInheritedAttributes();
        common::CommonOutputJaxConfig config{{16, 8, 640, 1, "MathJaxNewcm", false},
                                             sourceOptions(),
                                             bundle.fonts,
                                             "MathJaxNewcm",
                                             common::InlineBreakPolicy::forced};
        common::LayoutJob job(document, std::move(config), measurement,
                              common::CommonWrapperFactory::standardNodes(), makeLinebreakVisitor);
        expect(node.kind() == mathml::NodeKind::semantics) << "null test node kind";
        auto &baseWrapper = job.factory().wrap(node);
        auto *wrapper = dynamic_cast<common::CommonSemantics *>(&baseWrapper);
        expect(wrapper != nullptr) << "null test has CommonSemantics wrapper";
        if (!wrapper)
            return;
        wrapper->childNodes = {nullptr};
        scene::Builder builder;
        const std::array<scene::NodeId, 1> parents{0};
        expect(throws<std::logic_error>([&] { (void)wrapper->emitScene(builder, parents); }));
    };
};
