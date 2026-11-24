#include "support/folia_test.hpp"

import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.maction;
import folia.math.core.mml_tree.mml_nodes.mfenced;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;

using namespace boost::ut;
using namespace folia::math;
using namespace folia::math::mml;
using namespace folia::math::output::common;

namespace {
struct NoMeasurements final : MeasurementTarget {
    ExtendedMetrics measureMetrics(MetricRequest const &) override {
        throw std::logic_error("fence/action fixture supplies metrics");
    }

    UnknownBBox measureUnknownText(UnknownTextRequest const &) override {
        throw std::logic_error("fence/action fixture uses known NewCM glyphs");
    }
};

std::unique_ptr<Linebreaks> makeLinebreaks(CommonWrapperFactory &factory) {
    return std::make_unique<LinebreakVisitor>(factory);
}

CommonWrapperCatalog catalog() {
    CommonWrapperCatalog result;
    auto add = [&]<class Wrapper>(NodeKind kind) {
        result.push_back(
            {kind,
             {[](WrapperBuildArguments const &a) {
                  return std::make_unique<Wrapper>(a.factory, a.node, a.parent);
              },
              [](CommonWrapper const &w) { return dynamic_cast<Wrapper const *>(&w) != nullptr; }},
             {}});
    };
    add.operator()<CommonMfenced>(NodeKind::mfenced);
    add.operator()<CommonMaction>(NodeKind::maction);
    add.operator()<CommonMath>(NodeKind::math);
    add.operator()<CommonInferredMrow>(NodeKind::inferred_mrow);
    add.operator()<CommonMrow>(NodeKind::mrow);
    add.operator()<CommonMo>(NodeKind::mo);
    add.operator()<CommonTextNode>(NodeKind::text);
    return result;
}

MmlMo &makeMo(MmlDocument &document, std::u16string text) {
    auto &textNode = dynamic_cast<TextNode &>(document.factory().make(NodeKind::text));
    textNode.setText(std::move(text));
    const std::array<MmlNode *, 1> content{&textNode};
    return dynamic_cast<MmlMo &>(document.factory().make(NodeKind::mo, {}, content));
}
} // namespace

suite native_common_fence_action = [] {
    "CommonMfenced projects generated source Mo nodes in source order"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        MmlDocument document;
        AttributeSet attrs;
        attrs.set(u"open", u"(");
        attrs.set(u"close", u")");
        attrs.set(u"separators", u",");
        auto &firstNode = makeMo(document, u"x");
        auto &secondNode = makeMo(document, u"y");
        const std::array<MmlNode *, 2> children{&firstNode, &secondNode};
        auto &fenceNode = document.factory().make(NodeKind::mfenced, std::move(attrs), children);
        fenceNode.setInheritedAttributes({}, true, 0, false);
        auto &fencedMml = dynamic_cast<MmlMfenced &>(fenceNode);
        expect(dynamic_cast<MmlMo *>(fencedMml.open) != nullptr);
        expect(dynamic_cast<MmlMo *>(fencedMml.separators.at(0)) != nullptr);
        expect(dynamic_cast<MmlMo *>(fencedMml.close) != nullptr);
        expect(fencedMml.open->parent() == &fenceNode);
        expect(fencedMml.separators[0]->parent() == &fenceNode);
        expect(fencedMml.close->parent() == &fenceNode);

        CommonOptions options;
        ExtendedMetrics metrics;
        CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"}, measurement,
                            catalog(), &makeLinebreaks);
        auto &wrapper = dynamic_cast<CommonMfenced &>(jax.factory().wrap(fenceNode));
        expect(wrapper.mrow != nullptr);
        expect(wrapper.mrow->parent == &wrapper);
        expect(wrapper.mrow->childNodes.size() == 5_u);
        expect(wrapper.mrow->childNodes[0]->node == fencedMml.open);
        expect(wrapper.mrow->childNodes[1]->node == &firstNode);
        expect(wrapper.mrow->childNodes[2]->node == fencedMml.separators[0]);
        expect(wrapper.mrow->childNodes[3]->node == &secondNode);
        expect(wrapper.mrow->childNodes[4]->node == fencedMml.close);
        for (std::size_t index = 0; index < wrapper.mrow->childNodes.size(); ++index)
            expect(
                wrapper.mrow->childNodes[index]->parent ==
                (index == 1 || index == 3 ? static_cast<CommonWrapper *>(&wrapper) : wrapper.mrow));

        const auto projected = wrapper.getBBox();
        const auto rowBox = wrapper.mrow->getOuterBBox();
        expect(projected.w == rowBox.w);
        expect(projected.h == rowBox.h);
        expect(projected.d == rowBox.d);
        expect(projected.w > 0.0);
        expect(wrapper.breakCount() == wrapper.mrow->breakCount());
        auto projectedLine = wrapper.getLineBBox(0);
        auto rowLine = wrapper.mrow->getLineBBox(0);
        expect(projectedLine->w == rowLine->w);
        expect(projectedLine->h == rowLine->h);
        expect(projectedLine->d == rowLine->d);
    };

    "CommonMaction follows selected Mo and MML fallback projections"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        MmlDocument document;
        AttributeSet attrs;
        attrs.set(u"selection", 2.0);
        auto &first = makeMo(document, u"x");
        auto &second = makeMo(document, u"y");
        const std::array<MmlNode *, 2> children{&first, &second};
        auto &actionNode = document.factory().make(NodeKind::maction, std::move(attrs), children);
        const std::array<MmlNode *, 1> mathChildren{&actionNode};
        auto &math = document.factory().make(NodeKind::math, {}, mathChildren);
        document.setRoot(math);
        math.setInheritedAttributes();
        math.setTeXclass(nullptr);

        CommonOptions options;
        ExtendedMetrics metrics;
        CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"}, measurement,
                            catalog(), &makeLinebreaks);
        (void)jax.factory().wrap(math);
        auto &action = dynamic_cast<CommonMaction &>(*jax.wrapperFor(actionNode));
        expect(action.childNodes.size() == 2_u);
        expect(action.selected() == action.childNodes[1]);
        expect(action.selected()->parent == &action);
        expect(action.selected()->node == &second);
        expect(action.breakCount() == action.selected()->breakCount());
        const auto selectedBox = action.selected()->getOuterBBox();
        const auto projected = action.getBBox();
        expect(projected.w == selectedBox.w);
        expect(projected.h == selectedBox.h);
        expect(projected.d == selectedBox.d);
        expect(projected.w > 0.0);
        auto actionLine = action.getLineBBox(0);
        auto selectedLine = action.selected()->getLineBBox(0);
        expect(actionLine->w == selectedLine->w);
        expect(actionLine->h == selectedLine->h);
        expect(actionLine->d == selectedLine->d);

        actionNode.attributes().set(u"selection", std::numeric_limits<double>::quiet_NaN());
        action.invalidateBBox();
        auto *fallback = action.selected();
        expect(fallback != nullptr);
        expect(fallback->node->kind() == NodeKind::mrow);
        expect(fallback->node != &first && fallback->node != &second);
        expect(fallback->parent == &action);
        bool fallbackIsListed = false;
        for (auto *child : action.childNodes)
            fallbackIsListed = fallbackIsListed || child == fallback;
        expect(!fallbackIsListed);
        expect(action.breakCount() == fallback->breakCount());
        const auto fallbackBox = fallback->getOuterBBox();
        const auto fallbackProjected = action.getBBox();
        expect(fallbackProjected.w == fallbackBox.w);
        expect(fallbackProjected.h == fallbackBox.h);
        expect(fallbackProjected.d == fallbackBox.d);
    };
};
