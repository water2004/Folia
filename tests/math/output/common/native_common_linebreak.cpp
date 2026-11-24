#include <array>
#include <optional>
#include <string_view>
#include "support/folia_test.hpp"

import folia.math.fonts.newcm_font_module_provider;
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.core.mml_tree.mml_nodes.mspace;
import folia.math.output.common;

using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;

namespace {
struct NoUnknownText final : common::MeasurementTarget {
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("linebreak fixture should not request metrics");
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("linebreak fixture uses glyphs in the pinned font");
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreakVisitor(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

template <class Wrapper> common::CommonWrapperRegistration registration(mml::NodeKind kind) {
    using Args = common::WrapperBuildArguments;
    using Product = common::CommonWrapper;
    return {
        kind,
        folia::math::tree::FactoryClass<Product, Args>{
            [](Args const &args) -> std::unique_ptr<Product> {
                return std::make_unique<Wrapper>(args.factory, args.node, args.parent);
            },
            [](Product const &value) { return dynamic_cast<Wrapper const *>(&value) != nullptr; }},
        common::CommonWrapperClassData::commonDefaults()};
}
} // namespace

suite common_linebreak_native = [] {
    "mspace line leading preserves the Source default and authored lengths"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        NoUnknownText measurement;
        struct LeadingCase {
            std::optional<std::u16string_view> attribute;
            double expected;
        };
        const std::array<LeadingCase, 4> cases{{
            {std::nullopt, .2},
            {u"", .2},
            {u".4em", .4},
            {u"0", 0},
        }};
        for (const auto &item : cases) {
            mml::MmlDocument document;
            mml::AttributeSet attributes;
            attributes.set(u"linebreak", u"newline");
            if (item.attribute)
                attributes.set(u"data-lineleading", std::u16string(*item.attribute));
            auto &space = document.factory().make(mml::NodeKind::mspace, std::move(attributes));
            const std::array<mml::MmlNode *, 1> children{&space};
            auto &math = document.factory().make(mml::NodeKind::math, {}, children);
            document.setRoot(math);
            math.setInheritedAttributes();
            common::ExtendedMetrics metrics{16, 8, 640, 1, "MathJaxNewcm", false};
            common::CommonOutputJax jax(document, {metrics, {}, bundle.fonts, "MathJaxNewcm"},
                                        measurement, common::CommonWrapperFactory::standardNodes(),
                                        &makeLinebreakVisitor);
            (void)jax.factory().wrap(math);
            auto *wrapper = dynamic_cast<common::CommonMspace *>(jax.wrapperFor(space));
            expect(wrapper != nullptr);
            if (!wrapper)
                continue;
            for (const auto index : {0u, 1u}) {
                const auto line = wrapper->getLineBBox(index);
                expect(line != nullptr);
                if (line)
                    expect(line->lineLeading == std::optional<double>{item.expected});
            }
        }
    };

    "linebreak_visitor_uses_source_math_roots_and_recurses_into_children"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        NoUnknownText measurement;
        mml::MmlDocument document;
        common::CommonOptions options;
        options.linebreaks.width = "100%";
        common::ExtendedMetrics metrics;
        metrics.family = "MathJaxNewcm";
        common::CommonWrapperCatalog catalog{
            registration<common::CommonMath>(mml::NodeKind::math),
            registration<common::CommonInferredMrow>(mml::NodeKind::inferred_mrow),
            registration<common::CommonMtext>(mml::NodeKind::mtext),
            registration<common::CommonTextNode>(mml::NodeKind::text),
            registration<common::CommonMrow>(mml::NodeKind::mrow),
            registration<common::CommonMfrac>(mml::NodeKind::mfrac),
            registration<common::CommonMsqrt>(mml::NodeKind::msqrt),
            registration<common::CommonMroot>(mml::NodeKind::mroot),
            registration<common::CommonMspace>(mml::NodeKind::mspace),
            registration<common::CommonMo>(mml::NodeKind::mo)};
        common::CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"},
                                    measurement, std::move(catalog), &makeLinebreakVisitor);

        const auto makeText = [&document](std::u16string value) -> mml::MmlNode & {
            auto &token = document.factory().make(mml::NodeKind::text);
            token.setText(std::move(value));
            std::array<mml::MmlNode *, 1> children{&token};
            return document.factory().make(mml::NodeKind::mtext, {}, children);
        };
        const auto makeInferred = [&document](mml::MmlNode &child) -> mml::MmlNode & {
            std::array<mml::MmlNode *, 1> children{&child};
            return document.factory().make(mml::NodeKind::inferred_mrow, {}, children);
        };
        const auto wrapInMath = [&document, &jax,
                                 &makeInferred](mml::MmlNode &child) -> common::CommonWrapper & {
            auto &inferred = makeInferred(child);
            std::array<mml::MmlNode *, 1> children{&inferred};
            auto &math = document.factory().make(mml::NodeKind::math, {}, children);
            math.setInheritedAttributes();
            math.setTeXclass(nullptr);
            (void)jax.factory().wrap(math);
            auto *wrapper = jax.wrapperFor(child);
            expect(wrapper != nullptr);
            return *wrapper;
        };

        // Plain MathML text takes the real math -> inferred row -> token path.
        auto &textSource = makeText(u"x x");
        auto &textWrapper = wrapInMath(textSource);
        const double textWidth = textWrapper.getBBox().w;
        expect(textWidth > 0.0);
        jax.linebreaks().breakToWidth(textWrapper, textWidth * .65);
        auto *textToken = dynamic_cast<common::CommonMtext *>(&textWrapper);
        expect(textToken != nullptr);
        if (textToken) {
            expect(textToken->breakPoints.size() == 1_u);
            expect(textToken->breakPoints.front().has_value());
            if (textToken->breakPoints.front()) {
                expect((*textToken->breakPoints.front())[0] == 0_u);
                expect((*textToken->breakPoints.front())[1] == 1_u);
            }
            expect(textToken->breakCount() == 1_u);
            expect(textToken->lineBBox.empty());
        }

        // A regular row with a postfix close operator exercises row traversal
        // and Source's unmatched-close signed depth path.
        auto &rowText = makeText(u"y y");
        auto &closeText = document.factory().make(mml::NodeKind::text);
        closeText.setText(u")");
        std::array<mml::MmlNode *, 1> closeTextChildren{&closeText};
        auto &closeNode = dynamic_cast<mml::MmlMo &>(
            document.factory().make(mml::NodeKind::mo, {}, closeTextChildren));
        closeNode.attributes().set(u"linebreak", u"newline");
        closeNode.attributes().set(u"form", u"postfix");
        closeNode.attributes().set(u"fence", true);
        std::array<mml::MmlNode *, 2> rowChildren{&rowText, &closeNode};
        auto &rowNode = document.factory().make(mml::NodeKind::mrow, {}, rowChildren);
        auto &rowWrapper = wrapInMath(rowNode);
        expect(rowWrapper.getBBox().w > 0.0);
        jax.linebreaks().breakToWidth(rowWrapper, rowWrapper.getBBox().w * .65);
        expect(rowWrapper.childNodes.size() == 2_u);
        auto *rowTextWrapper = rowWrapper.childNodes.size() > 0
                                   ? dynamic_cast<common::CommonMtext *>(rowWrapper.childNodes[0])
                                   : nullptr;
        expect(rowTextWrapper != nullptr);
        if (rowTextWrapper)
            expect(rowTextWrapper->breakPoints.size() == 1_u) << "row text break";

        // The prefix operator is first in an explicit row nested under an
        // inferred row. Its previous embellished operator is found only when
        // getPrevious follows physical parent links across that inferred row.
        auto makeMo = [&document](std::u16string value, std::u16string linebreak,
                                  std::u16string form) -> mml::MmlMo & {
            auto &token = document.factory().make(mml::NodeKind::text);
            token.setText(std::move(value));
            std::array<mml::MmlNode *, 1> children{&token};
            auto &mo = dynamic_cast<mml::MmlMo &>(
                document.factory().make(mml::NodeKind::mo, {}, children));
            mo.attributes().set(u"linebreak", std::move(linebreak));
            mo.attributes().set(u"form", std::move(form));
            mo.attributes().set(u"fence", true);
            return mo;
        };
        auto &priorOperator = makeMo(u"a", u"auto", u"infix");
        auto &nestedOpen = makeMo(u"(", u"goodbreak", u"prefix");
        auto &nestedTail = makeText(u"r");
        std::array<mml::MmlNode *, 2> innerChildren{&nestedOpen, &nestedTail};
        auto &innerRow = document.factory().make(mml::NodeKind::mrow, {}, innerChildren);
        std::array<mml::MmlNode *, 1> inferredChildren{&innerRow};
        auto &nestedInferred =
            document.factory().make(mml::NodeKind::inferred_mrow, {}, inferredChildren);
        std::array<mml::MmlNode *, 2> outerChildren{&priorOperator, &nestedInferred};
        auto &outerRow = document.factory().make(mml::NodeKind::mrow, {}, outerChildren);
        auto &outerWrapper = wrapInMath(outerRow);
        expect(outerWrapper.getBBox().w > 0.0);
        jax.linebreaks().breakToWidth(outerWrapper, outerWrapper.getBBox().w * .4);
        auto *nestedOpenWrapper = jax.wrapperFor(nestedOpen);
        expect(nestedOpenWrapper != nullptr);
        if (nestedOpenWrapper) {
            expect(nestedOpenWrapper->breakCount() == 1_u);
            auto *moWrapper = dynamic_cast<common::CommonMo *>(nestedOpenWrapper);
            expect(moWrapper != nullptr);
            if (moWrapper)
                expect(moWrapper->breakStyle == "before");
        }

        // Mspace break styling is dispatched through its concrete wrapper.
        auto &spacePrefix = makeText(u"z");
        auto &spaceNode =
            dynamic_cast<mml::MmlMspace &>(document.factory().make(mml::NodeKind::mspace));
        spaceNode.attributes().set(u"width", u"2em");
        spaceNode.attributes().set(u"linebreak", u"goodbreak");
        spaceNode.attributes().set(u"linebreakstyle", u"after");
        std::array<mml::MmlNode *, 2> spaceChildren{&spacePrefix, &spaceNode};
        auto &spaceRowNode = document.factory().make(mml::NodeKind::mrow, {}, spaceChildren);
        auto &spaceRow = wrapInMath(spaceRowNode);
        expect(spaceRow.getBBox().w > 0.0);
        jax.linebreaks().breakToWidth(spaceRow, spaceRow.getBBox().w * .5);
        auto *spaceWrapper = spaceRow.childNodes.size() > 1
                                 ? dynamic_cast<common::CommonMspace *>(spaceRow.childNodes[1])
                                 : nullptr;
        expect(spaceWrapper != nullptr);
        if (spaceWrapper)
            expect(spaceWrapper->breakStyle == "after") << "space style";

        // Test a close operator as the math child itself, with a complete
        // Source parent chain registered by wrapping the Math root.
        auto &isolatedCloseText = document.factory().make(mml::NodeKind::text);
        isolatedCloseText.setText(u")");
        std::array<mml::MmlNode *, 1> isolatedCloseChildren{&isolatedCloseText};
        auto &isolatedClose = dynamic_cast<mml::MmlMo &>(
            document.factory().make(mml::NodeKind::mo, {}, isolatedCloseChildren));
        isolatedClose.attributes().set(u"linebreak", u"newline");
        isolatedClose.attributes().set(u"form", u"postfix");
        isolatedClose.attributes().set(u"fence", true);
        auto &closeWrapper = wrapInMath(isolatedClose);
        expect(closeWrapper.getBBox().w > 0.0);
        jax.linebreaks().breakToWidth(closeWrapper, closeWrapper.getBBox().w * .5);
        // A sole operator is embellished by its inferred parent; Source keeps
        // the local style empty and exposes the break on that projection.
        expect(closeWrapper.breakCount() == 0_u) << "no split before first operator";
        expect(dynamic_cast<common::CommonMo &>(closeWrapper).embellishedBreakCount() == 1_u);

        // Fraction arguments are inferred rows in MML; the visitor recurses
        // through each row to place a break in each text wrapper.
        auto &numeratorText = makeText(u"a a");
        auto &denominatorText = makeText(u"b b");
        auto &numerator = makeInferred(numeratorText);
        auto &denominator = makeInferred(denominatorText);
        std::array<mml::MmlNode *, 2> fractionChildren{&numerator, &denominator};
        auto &fractionNode = document.factory().make(mml::NodeKind::mfrac, {}, fractionChildren);
        auto &fractionWrapper = wrapInMath(fractionNode);
        expect(fractionWrapper.getBBox().w > 0.0);
        jax.linebreaks().breakToWidth(fractionWrapper, fractionWrapper.getBBox().w * .6);
        expect(fractionWrapper.childNodes.size() == 2_u);
        auto *numeratorRow =
            fractionWrapper.childNodes.size() > 0
                ? dynamic_cast<common::CommonWrapper *>(fractionWrapper.childNodes[0])
                : nullptr;
        auto *denominatorRow =
            fractionWrapper.childNodes.size() > 1
                ? dynamic_cast<common::CommonWrapper *>(fractionWrapper.childNodes[1])
                : nullptr;
        expect(numeratorRow != nullptr);
        expect(denominatorRow != nullptr);
        auto *numeratorWrapper =
            numeratorRow && !numeratorRow->childNodes.empty()
                ? dynamic_cast<common::CommonMtext *>(numeratorRow->childNodes[0])
                : nullptr;
        auto *denominatorWrapper =
            denominatorRow && !denominatorRow->childNodes.empty()
                ? dynamic_cast<common::CommonMtext *>(denominatorRow->childNodes[0])
                : nullptr;
        expect(numeratorWrapper != nullptr);
        expect(denominatorWrapper != nullptr);
        if (numeratorWrapper)
            expect(numeratorWrapper->breakPoints.size() == 1_u) << "numerator break";
        if (denominatorWrapper)
            expect(denominatorWrapper->breakPoints.size() == 1_u) << "denominator break";

        // Radical bases are inferred rows as well; inspect their nested text.
        auto &radicalText = makeText(u"c c");
        std::array<mml::MmlNode *, 1> radicalChildren{&radicalText};
        auto &radicalNode = document.factory().make(mml::NodeKind::msqrt, {}, radicalChildren);
        auto &radicalWrapper = wrapInMath(radicalNode);
        expect(radicalWrapper.getBBox().w > 0.0);
        jax.linebreaks().breakToWidth(radicalWrapper, radicalWrapper.getBBox().w * .65);
        auto *radicalRow = radicalWrapper.childNodes.size() > 0
                               ? dynamic_cast<common::CommonWrapper *>(radicalWrapper.childNodes[0])
                               : nullptr;
        auto *radicalTextWrapper =
            radicalRow && !radicalRow->childNodes.empty()
                ? dynamic_cast<common::CommonMtext *>(radicalRow->childNodes[0])
                : nullptr;
        expect(radicalTextWrapper != nullptr) << "sqrt text wrapper";
        if (radicalTextWrapper)
            expect(radicalTextWrapper->breakPoints.size() == 1_u) << "sqrt break";

        auto &indexedText = makeText(u"d d");
        auto &indexedBase = makeInferred(indexedText);
        auto &index = makeText(u"3");
        std::array<mml::MmlNode *, 2> mrootChildren{&indexedBase, &index};
        auto &mrootNode = document.factory().make(mml::NodeKind::mroot, {}, mrootChildren);
        auto &mrootWrapper = wrapInMath(mrootNode);
        expect(mrootWrapper.getBBox().w > 0.0);
        jax.linebreaks().breakToWidth(mrootWrapper, mrootWrapper.getBBox().w * .65);
        auto *mrootRow = mrootWrapper.childNodes.size() > 0
                             ? dynamic_cast<common::CommonWrapper *>(mrootWrapper.childNodes[0])
                             : nullptr;
        auto *indexedWrapper = mrootRow && !mrootRow->childNodes.empty()
                                   ? dynamic_cast<common::CommonMtext *>(mrootRow->childNodes[0])
                                   : nullptr;
        expect(indexedWrapper != nullptr) << "indexed root text wrapper";
        if (indexedWrapper)
            expect(indexedWrapper->breakPoints.size() == 1_u) << "indexed root break";
    };
};
