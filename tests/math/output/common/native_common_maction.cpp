#include "support/folia_test.hpp"

import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.maction;
import folia.math.core.mml_tree.mml_nodes.math;
import folia.math.core.mml_tree.mml_nodes.mrow;
import folia.math.core.mml_tree.mml_nodes.mspace;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace mml = folia::math::mml;

namespace {
class NoMeasurements final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("maction fixture supplies metrics");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("maction fixture uses known mspace geometry");
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreaks(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

common::CommonOutputJaxConfig config(common::FontRepository const &fonts) {
    common::ExtendedMetrics metrics{16, 8, 640, 1, "MathJaxNewcm", false};
    return {metrics, {}, fonts, "MathJaxNewcm"};
}

mml::MmlMspace &makeSpace(mml::MmlDocument &document, std::u16string width = u"1em") {
    auto &space = dynamic_cast<mml::MmlMspace &>(document.factory().make(mml::NodeKind::mspace));
    space.attributes().set(u"width", std::move(width));
    space.attributes().set(u"height", u".6em");
    space.attributes().set(u"depth", u".2em");
    return space;
}

common::CommonMaction &makeAction(common::CommonOutputJax &jax, mml::MmlDocument &document,
                                  std::u16string offsets, bool includeOffsets = true) {
    auto &first = makeSpace(document);
    auto &second = makeSpace(document, u"2em");
    mml::AttributeSet attributes;
    if (includeOffsets)
        attributes.set(u"data-offsets", std::move(offsets));
    const std::array<mml::MmlNode *, 2> children{&first, &second};
    auto &action = document.factory().make(mml::NodeKind::maction, std::move(attributes), children);
    const std::array<mml::MmlNode *, 1> mathChildren{&action};
    auto &math = document.factory().make(mml::NodeKind::math, {}, mathChildren);
    document.setRoot(math);
    math.setInheritedAttributes();
    math.setTeXclass(nullptr);
    (void)jax.factory().wrap(math);
    auto *wrapper = dynamic_cast<common::CommonMaction *>(jax.wrapperFor(action));
    if (!wrapper)
        throw std::logic_error("maction wrapper was not registered");
    return *wrapper;
}
} // namespace

suite native_common_maction = [] {
    "CommonMaction applies Source defaults and whitespace-split offset lengths"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        mml::MmlDocument document;
        common::CommonOutputJax jax(document, config(bundle.fonts), measurement,
                                    common::CommonWrapperFactory::standardNodes(), &makeLinebreaks);
        auto &action = makeAction(jax, document, u" \t1em   2px\n");
        expect(std::abs(action.tipDx() - 1.0) < 1e-10) << "first offset remains in em";
        expect(std::abs(action.tipDy() - 2.0 / jax.pxPerEm()) < 1e-10)
            << "second offset converts from px with the active font metrics";
    };

    "CommonMaction defaults missing and empty offset entries independently"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        mml::MmlDocument defaultsDocument;
        common::CommonOutputJax defaultsJax(defaultsDocument, config(bundle.fonts), measurement,
                                            common::CommonWrapperFactory::standardNodes(),
                                            &makeLinebreaks);
        auto &defaults = makeAction(defaultsJax, defaultsDocument, std::u16string{}, false);
        expect(std::abs(defaults.tipDx() - .2) < 1e-10);
        expect(std::abs(defaults.tipDy() - .1) < 1e-10);

        mml::MmlDocument whitespaceDocument;
        common::CommonOutputJax whitespaceJax(whitespaceDocument, config(bundle.fonts), measurement,
                                              common::CommonWrapperFactory::standardNodes(),
                                              &makeLinebreaks);
        auto &whitespace = makeAction(whitespaceJax, whitespaceDocument, u" \t ");
        expect(std::abs(whitespace.tipDx() - .2) < 1e-10);
        expect(std::abs(whitespace.tipDy() - .1) < 1e-10)
            << "whitespace-only offsets default both dimensions";

        mml::MmlDocument partialDocument;
        common::CommonOutputJax partialJax(partialDocument, config(bundle.fonts), measurement,
                                           common::CommonWrapperFactory::standardNodes(),
                                           &makeLinebreaks);
        auto &partial = makeAction(partialJax, partialDocument, u".5ex");
        // Pinned MathJax ts/util/lengths.ts RELUNITS.ex is .431 (also observed
        // by the Source witness as 2ex = .862), so .5ex resolves to .2155.
        expect(std::abs(partial.tipDx() - .2155) < 1e-10)
            << "first offset follows the pinned Source relative-ex conversion";
        expect(std::abs(partial.tipDy() - .1) < 1e-10)
            << "missing second offset retains its own Source default";
    };

    "CommonOutputJax builds a nested math Scene from its existing request factory"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        std::optional<folia::math::output::scene::Scene> retained;
        {
            mml::MmlDocument document;
            auto &tipContent = makeSpace(document, u"1.5em");
            const std::array<mml::MmlNode *, 1> tipChildren{&tipContent};
            auto &tipMath = document.factory().make(mml::NodeKind::math, {}, tipChildren);
            auto &selected = makeSpace(document);
            const std::array<mml::MmlNode *, 2> actionChildren{&selected, &tipMath};
            auto &actionNode = document.factory().make(mml::NodeKind::maction, {}, actionChildren);
            const std::array<mml::MmlNode *, 1> rootChildren{&actionNode};
            auto &root = document.factory().make(mml::NodeKind::math, {}, rootChildren);
            document.setRoot(root);
            root.setInheritedAttributes();
            root.setTeXclass(nullptr);

            common::CommonOutputJax jax(document, config(bundle.fonts), measurement,
                                        common::CommonWrapperFactory::standardNodes(),
                                        &makeLinebreaks);
            (void)jax.factory().wrap(root);
            auto *action = dynamic_cast<common::CommonMaction *>(jax.wrapperFor(actionNode));
            if (!action) {
                expect(false) << "root factory creates CommonMaction";
                return;
            }
            actionNode.attributes().set(u"selection", 2.0);
            auto *tipWrapper = action->selected();
            if (!tipWrapper || tipWrapper->node != &tipMath) {
                expect(false)
                    << "selected auxiliary math is already wrapped by the request factory";
                return;
            }

            retained.emplace(jax.buildScene(*tipWrapper));
            expect(retained->metrics().width > 0.0) << "auxiliary subtree completes its own Scene";
            expect(std::abs(retained->metrics().width - tipWrapper->getOuterBBox().w) < 1e-10)
                << "nested Scene uses the same Common layout measurements";
            expect(document.root() == &root)
                << "nested Scene construction leaves document root intact";
            expect(tipMath.parent() == &actionNode)
                << "nested construction preserves MML parentage";

            mml::MmlDocument otherDocument;
            common::CommonOutputJax otherJax(otherDocument, config(bundle.fonts), measurement,
                                             common::CommonWrapperFactory::standardNodes(),
                                             &makeLinebreaks);
            expect(throws<std::invalid_argument>([&] { (void)otherJax.buildScene(*tipWrapper); }))
                << "a different request rejects this wrapper";
        }
        expect(retained.has_value()) << "completed nested Scene outlives its request";
        if (retained)
            expect(retained->metrics().width > 0.0)
                << "Scene remains readable after wrapper and document retirement";
    };

    "CommonMaction retains Source selected-child clamp and fallback behavior"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        mml::MmlDocument document;
        common::CommonOutputJax jax(document, config(bundle.fonts), measurement,
                                    common::CommonWrapperFactory::standardNodes(), &makeLinebreaks);
        auto &action = makeAction(jax, document, u"");
        auto *first = action.childNodes[0];
        auto *last = action.childNodes[1];

        action.node->attributes().set(u"selection", -3.0);
        expect(action.selected() == first) << "negative selections clamp to the first child";
        action.node->attributes().set(u"selection", 10.0);
        expect(action.selected() == last) << "large selections clamp to the last child";
        action.node->attributes().set(u"selection", 1.5);
        auto *fallback = action.selected();
        expect(fallback != nullptr);
        if (fallback) {
            expect(fallback->node->kind() == mml::NodeKind::mrow)
                << "fractional array lookup uses the MML empty-row fallback";
            expect(fallback != first && fallback != last)
                << "fractional lookup does not select a neighboring child";
        }
    };

    "CommonMaction delegates multiline break count and line boxes to its selected child"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        mml::MmlDocument document;

        auto &beforeBreak = makeSpace(document, u".5em");
        auto &newline = dynamic_cast<mml::MmlMspace &>(
            document.factory().make(mml::NodeKind::mspace));
        newline.attributes().set(u"linebreak", u"newline");
        expect(newline.hasNewline()) << "the break has no explicit height or depth";
        auto &afterBreak = makeSpace(document, u"2em");
        const std::array<mml::MmlNode *, 3> selectedChildren{
            &beforeBreak, &newline, &afterBreak};
        auto &selectedRow = document.factory().make(mml::NodeKind::mrow, {}, selectedChildren);

        auto &unselected = makeSpace(document, u"1em");
        mml::AttributeSet attributes;
        attributes.set(u"selection", 2.0);
        const std::array<mml::MmlNode *, 2> actionChildren{&unselected, &selectedRow};
        auto &actionNode =
            document.factory().make(mml::NodeKind::maction, std::move(attributes), actionChildren);
        const std::array<mml::MmlNode *, 1> mathChildren{&actionNode};
        auto &math = document.factory().make(mml::NodeKind::math, {}, mathChildren);
        document.setRoot(math);
        math.setInheritedAttributes();
        math.setTeXclass(nullptr);

        common::CommonOutputJax jax(document, config(bundle.fonts), measurement,
                                    common::CommonWrapperFactory::standardNodes(), &makeLinebreaks);
        (void)jax.factory().wrap(math);
        auto *action = dynamic_cast<common::CommonMaction *>(jax.wrapperFor(actionNode));
        expect(action != nullptr);
        if (!action)
            return;

        auto *selected = action->selected();
        expect(selected != nullptr);
        if (!selected)
            return;
        expect(selected->node == &selectedRow);
        (void)selected->getBBox();
        (void)action->getBBox();
        expect(selected->breakCount() == 1u)
            << "the actual selected MML row contains one explicit newline break";
        if (selected->breakCount() != 1)
            return;
        expect(action->breakCount() == selected->breakCount())
            << "CommonMaction reports its selected child's break count";
        expect(selected->lineBBox.size() == selected->breakCount() + 1u)
            << "the selected row has one computed line BBox per actual line";

        for (std::size_t lineIndex = 0; lineIndex <= selected->breakCount(); ++lineIndex) {
            const auto selectedLine = selected->getLineBBox(lineIndex);
            const auto actionLine = action->getLineBBox(lineIndex);
            expect(selectedLine != nullptr && actionLine != nullptr);
            if (!selectedLine || !actionLine)
                continue;
            expect(std::abs(actionLine->w - selectedLine->w) < 1e-10);
            expect(std::abs(actionLine->h - selectedLine->h) < 1e-10);
            expect(std::abs(actionLine->d - selectedLine->d) < 1e-10)
                << "CommonMaction line geometry delegates to the selected child's real line BBox";
        }
    };
};
