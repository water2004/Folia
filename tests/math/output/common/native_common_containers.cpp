#include "support/folia_test.hpp"

import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.mpadded;
import folia.math.core.mml_tree.mml_nodes.mi;
import folia.math.core.mml_tree.mml_nodes.semantics;
import folia.math.core.mml_tree.mml_nodes.tex_atom;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;

using namespace boost::ut;
using namespace folia::math;
using namespace folia::math::mml;
using namespace folia::math::output::common;

namespace {
struct RejectUnexpectedMeasurement final : MeasurementTarget {
    ExtendedMetrics measureMetrics(MetricRequest const &) override {
        throw std::logic_error("unexpected Common math metrics measurement");
    }

    UnknownBBox measureUnknownText(UnknownTextRequest const &) override {
        throw std::logic_error("NewCM-backed fixture unexpectedly requested unknown text");
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
             CommonWrapperClassData::commonDefaults()});
    };
    add.operator()<CommonMath>(NodeKind::math);
    add.operator()<CommonInferredMrow>(NodeKind::inferred_mrow);
    add.operator()<CommonMpadded>(NodeKind::mpadded);
    add.operator()<CommonSemantics>(NodeKind::semantics);
    add.operator()<CommonTeXAtom>(NodeKind::tex_atom);
    add.operator()<CommonMrow>(NodeKind::mrow);
    add.operator()<CommonMi>(NodeKind::mi);
    add.operator()<CommonMo>(NodeKind::mo);
    add.operator()<CommonTextNode>(NodeKind::text);
    // Annotation nodes are intentionally geometry-free children of semantics.
    add.operator()<CommonWrapper>(NodeKind::annotation);
    add.operator()<CommonWrapper>(NodeKind::annotation_xml);
    return result;
}

MmlMi &makeMi(MmlDocument &document, std::u16string text) {
    auto &textNode = dynamic_cast<TextNode &>(document.factory().make(NodeKind::text));
    textNode.setText(std::move(text));
    const std::array<MmlNode *, 1> content{&textNode};
    AttributeSet attributes;
    attributes.set(u"mathvariant", u"italic");
    return dynamic_cast<MmlMi &>(
        document.factory().make(NodeKind::mi, std::move(attributes), content));
}

std::u16string scalarText(std::uint32_t scalar) {
    if (scalar <= 0xffff)
        return std::u16string(1, static_cast<char16_t>(scalar));
    scalar -= 0x10000;
    return {static_cast<char16_t>(0xd800 + (scalar >> 10)),
            static_cast<char16_t>(0xdc00 + (scalar & 0x3ff))};
}

std::uint32_t italicGlyphWithCorrection(FontRepository const &fonts) {
    auto const& face = fonts.face("MathJaxNewcm");
    auto request = std::dynamic_pointer_cast<folia::math::fonts::NewcmFontRequest const>(face.glyphSource);
    if (!request) throw std::logic_error("pinned NewCM request provider is absent");
    for (auto const& key : request->keys()) {
        if (key.variant != "italic") continue;
        const auto* glyph = fonts.glyph(face.name, key.variant, key.codepoint);
        if (glyph && glyph->metrics.ic != 0.0 && glyph->resource)
            return key.codepoint;
    }
    throw std::logic_error("pinned NewCM italic face has no glyph with italic correction");
}

void addMathRoot(MmlDocument &document, MmlNode &child) {
    const std::array<MmlNode *, 1> children{&child};
    auto &root = document.factory().make(NodeKind::math, {}, children);
    document.setRoot(root);
    root.setInheritedAttributes({}, true, 0, false);
    (void)root.setTeXclass(nullptr);
}

template <class Wrapper> Wrapper &wrapped(CommonOutputJax &jax, MmlNode const &node) {
    auto *result = dynamic_cast<Wrapper *>(jax.wrapperFor(node));
    if (!result)
        throw std::logic_error("MML node was not recursively wrapped with the expected type");
    return *result;
}

CommonMath &wrapMathTree(CommonOutputJax &jax, MmlDocument const &document) {
    if (!document.root())
        throw std::logic_error("fixture has no Math root");
    return dynamic_cast<CommonMath &>(jax.factory().wrap(*document.root()));
}

void expectMathAncestor(CommonWrapper const &wrapper, CommonMath const &math) {
    auto const *parent = wrapper.parent;
    while (parent && parent != &math)
        parent = parent->parent;
    expect(parent == &math);
}
} // namespace

suite native_common_containers = [] {
    "CommonMpadded resolves relative dimensions and projects the owned child bbox"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectUnexpectedMeasurement measurement;
        MmlDocument document;
        auto &mi = makeMi(document, u"x");
        const std::array<MmlNode *, 1> rowChildren{&mi};
        auto &row = document.factory().make(NodeKind::mrow, {}, rowChildren);

        AttributeSet attrs;
        attrs.set(u"width", u"+50%");
        attrs.set(u"height", u"-50%");
        attrs.set(u"depth", u"+25%");
        attrs.set(u"lspace", u"0.25em");
        attrs.set(u"voffset", u"-0.125em");
        attrs.set(u"data-align", u"right");
        const std::array<MmlNode *, 1> paddedChildren{&row};
        auto &padded = document.factory().make(NodeKind::mpadded, std::move(attrs), paddedChildren);
        padded.properties().tex.vertical_box = u"vbox";
        addMathRoot(document, padded);
        expect(padded.texClass() == TexClass::ord);

        CommonOptions options;
        ExtendedMetrics metrics;
        metrics.display = true;
        CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"}, measurement,
                            catalog(), &makeLinebreaks);
        const double mathContainerWidth = jax.getLinebreakWidth();
        auto &math = wrapMathTree(jax, document);
        auto &wrapper = wrapped<CommonMpadded>(jax, padded);
        expect(dynamic_cast<CommonInferredMrow *>(wrapper.parent) != nullptr);
        expectMathAncestor(wrapper, math);
        auto &content = *wrapper.childNodes.front();
        const BBox original = content.getOuterBBox();
        const auto dimensions = wrapper.getDimens();
        expect(original.w == 0.572);
        expect(original.h == 0.442);
        expect(original.d == 0.011);
        expect(original.R == 0.0);
        expect(original.rscale == 1.0);
        expect(dimensions[0] == 0.442);
        expect(dimensions[1] == 0.011);
        expect(dimensions[2] == 0.572);
        expect(dimensions[3] == -0.221);
        expect(dimensions[4] == 0.002749999999999999);
        expect(dimensions[5] == 0.2859999999999999);
        expect(dimensions[6] == .25);
        expect(dimensions[7] == -.125);
        expect(dimensions[8] == 0.2859999999999999);
        expect(wrapper.getChildAlign(0) == "right");

        const auto paddedBox = wrapper.getBBox();
        expect(paddedBox.w == 0.8579999999999999);
        expect(paddedBox.h == 0.221);
        expect(paddedBox.d == 0.013749999999999998);
        expect(wrapper.getWrapWidth(0) == 0.8579999999999999);
        expect(wrapper.containerWidth() == mathContainerWidth);
        expect(wrapper.dimen(u"-100em", original, "w", 0) == 0.0);
    };

    "CommonMpadded sends actual overflow work through the owned linebreak visitor"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectUnexpectedMeasurement measurement;
        MmlDocument document;
        auto &mi = makeMi(document, u"x");
        const std::array<MmlNode *, 1> rowChildren{&mi};
        auto &row = document.factory().make(NodeKind::mrow, {}, rowChildren);
        AttributeSet attrs;
        attrs.set(u"width", u"0em");
        attrs.set(u"data-overflow", u"linebreak");
        const std::array<MmlNode *, 1> paddedChildren{&row};
        auto &padded = document.factory().make(NodeKind::mpadded, std::move(attrs), paddedChildren);
        addMathRoot(document, padded);

        CommonOptions options;
        ExtendedMetrics metrics;
        CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"}, measurement,
                            catalog(), &makeLinebreaks);
        (void)jax.getLinebreakWidth();
        auto &math = wrapMathTree(jax, document);
        auto &wrapper = wrapped<CommonMpadded>(jax, padded);
        expect(dynamic_cast<CommonInferredMrow *>(wrapper.parent) != nullptr);
        expectMathAncestor(wrapper, math);
        const double childWidth = wrapper.childNodes.front()->getOuterBBox().w;
        const double paddedWidth = wrapper.getBBox().w;
        expect(childWidth == 0.572);
        expect(paddedWidth == 0.0);
        expect(childWidth > paddedWidth);
    };

    "CommonSemantics exposes only first-child geometry and Source core dispatch"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectUnexpectedMeasurement measurement;
        MmlDocument document;
        auto &mi = makeMi(document, u"x");
        auto &annotation = document.factory().make(NodeKind::annotation);
        const std::array<MmlNode *, 2> semanticsChildren{&mi, &annotation};
        auto &semantics = document.factory().make(NodeKind::semantics, {}, semanticsChildren);
        addMathRoot(document, semantics);

        CommonOptions options;
        ExtendedMetrics metrics;
        CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"}, measurement,
                            catalog(), &makeLinebreaks);
        auto &math = wrapMathTree(jax, document);
        auto &wrapper = wrapped<CommonSemantics>(jax, semantics);
        expectMathAncestor(wrapper, math);
        expect(wrapper.childNodes.size() == 2_u);
        auto &presentation = *wrapper.childNodes.front();
        const auto sourceBox = presentation.getBBox();
        const auto projected = wrapper.getBBox();
        expect(sourceBox.w == 0.572);
        expect(sourceBox.h == 0.442);
        expect(sourceBox.d == 0.011);
        expect(projected.w == 0.572);
        expect(projected.h == 0.442);
        expect(projected.d == 0.011);
        expect(wrapper.breakCount() == 0_u);
        expect(wrapper.childNodes[1]->node == &annotation);
    };

    "CommonSemantics delegates embellished break count to the actual Mo core"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectUnexpectedMeasurement measurement;
        MmlDocument document;
        auto &text = dynamic_cast<TextNode &>(document.factory().make(NodeKind::text));
        text.setText(u"+");
        const std::array<MmlNode *, 1> moChildren{&text};
        AttributeSet moAttrs;
        moAttrs.set(u"linebreak", u"goodbreak");
        auto &mo = document.factory().make(NodeKind::mo, std::move(moAttrs), moChildren);
        auto &annotation = document.factory().make(NodeKind::annotation_xml);
        const std::array<MmlNode *, 2> semanticsChildren{&mo, &annotation};
        auto &semantics = document.factory().make(NodeKind::semantics, {}, semanticsChildren);
        addMathRoot(document, semantics);

        CommonOptions options;
        ExtendedMetrics metrics;
        CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"}, measurement,
                            catalog(), &makeLinebreaks);
        auto &math = wrapMathTree(jax, document);
        auto &wrapper = wrapped<CommonSemantics>(jax, semantics);
        expectMathAncestor(wrapper, math);
        auto *core = dynamic_cast<CommonMo *>(wrapper.coreMO());
        expect(core != nullptr);
        const auto projected = wrapper.getBBox();
        expect(projected.w == 0.778);
        expect(projected.h == 0.583);
        expect(projected.d == 0.083);
        expect(core->getBBox().ic == 0.0);
        expect(wrapper.breakCount() == 0_u);
        expect(wrapper.breakCount() == core->embellishedBreakCount());
    };

    "CommonTeXAtom preserves the Source inferred-row boundary for italic correction"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectUnexpectedMeasurement measurement;
        MmlDocument document;
        const auto codepoint = italicGlyphWithCorrection(bundle.fonts);
        expect(codepoint == 33_u);
        auto &mi = makeMi(document, scalarText(codepoint));
        const std::array<MmlNode *, 1> atomChildren{&mi};
        auto &atom = document.factory().make(NodeKind::tex_atom, {}, atomChildren);
        addMathRoot(document, atom);
        expect(atom.texClass() == TexClass::ord);

        CommonOptions options;
        ExtendedMetrics metrics;
        CommonOutputJax jax(document, {metrics, options, bundle.fonts, "MathJaxNewcm"}, measurement,
                            catalog(), &makeLinebreaks);
        auto &math = wrapMathTree(jax, document);
        auto &wrapper = wrapped<CommonTeXAtom>(jax, atom);
        expectMathAncestor(wrapper, math);
        auto &child = *wrapper.childNodes.front();
        auto &glyph = wrapped<CommonMi>(jax, mi);
        expect(glyph.getBBox().w == 0.375);
        expect(glyph.getBBox().h == 0.716);
        expect(glyph.getBBox().d == 0.0);
        expect(glyph.getBBox().ic == 0.068);
        expect(child.getBBox().w == 0.375);
        expect(child.getBBox().h == 0.716);
        expect(child.getBBox().d == 0.0);
        expect(child.getBBox().ic == 0.0);
        const auto result = wrapper.getBBox();
        expect(result.ic == 0.0);
        expect(result.w == 0.375);
        expect(result.h == 0.716);
        expect(result.d == 0.0);
    };
};
