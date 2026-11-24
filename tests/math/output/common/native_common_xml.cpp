#include "support/folia_test.hpp"

import folia.math.adaptors.lite_adaptor;
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.math;
import folia.math.core.mml_tree.mml_nodes.mspace;
import folia.math.core.mml_tree.xml_dom_adaptor;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.util.utf16;

using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;
namespace lite = folia::math::adaptors::lite;

namespace {
class NoTextMeasurement final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("XML wrapper fixture unexpectedly measured metrics");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("XML wrapper fixture unexpectedly measured text");
    }
};

class XmlProbe final : public common::CommonXmlNode {
  public:
    using CommonXmlNode::CommonXmlNode;
    static inline std::size_t addHdwCalls{};
    folia::math::DOMNodeRef addHDW(folia::math::DOMNodeRef html,
                                   folia::math::styles::StyleList &) override {
        ++addHdwCalls;
        return html;
    }
};

class RecordingLite final : public lite::LiteBase {
  public:
    mutable std::vector<std::u16string> measuredKinds;
    mutable bool throwOnContent{};
    mutable std::size_t calls{};
    RecordingLite() {
        canMeasureNodes = false;
    }

    folia::math::PageBBox nodeBBox(folia::math::DOMNodeRef const &node) const override {
        ++calls;
        measuredKinds.emplace_back(kind(node));
        if (throwOnContent && kind(node) == u"mjx-xml-block")
            throw std::runtime_error("synthetic nodeBBox failure");
        if (kind(node) == u"mjx-xml-block")
            return {.left = 10, .top = 25, .right = 210, .bottom = 65};
        if (kind(node) == u"mjx-baseline")
            return {.left = 0, .top = 30, .right = 0, .bottom = 30};
        return LiteBase::nodeBBox(node);
    }
};

class CountingLite final : public lite::LiteBase {
  public:
    mutable std::vector<std::u16string> measuredKinds;
    CountingLite() {
        canMeasureNodes = false;
    }

    folia::math::PageBBox nodeBBox(folia::math::DOMNodeRef const &node) const override {
        measuredKinds.emplace_back(kind(node));
        return LiteBase::nodeBBox(node);
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreaks(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

common::CommonWrapperCatalog xmlCatalog() {
    using Args = common::WrapperBuildArguments;
    using Creator = folia::math::tree::FactoryClass<common::CommonWrapper, Args>;
    auto result = common::CommonWrapperFactory::standardNodes();
    result.push_back({mml::NodeKind::xml,
                      Creator{[](Args const &args) -> std::unique_ptr<common::CommonWrapper> {
                                  return std::make_unique<XmlProbe>(args.factory, args.node,
                                                                    args.parent);
                              },
                              [](common::CommonWrapper const &wrapper) {
                                  return dynamic_cast<XmlProbe const *>(&wrapper) != nullptr;
                              }},
                      common::CommonWrapperClassData::commonDefaults()});
    return result;
}

folia::math::DOMNodeRef makeInputXml(lite::LiteBase &adaptor, bool withHdw = false) {
    folia::math::DOMAttributes attributes;
    if (withHdw)
        attributes.push_back({u"data-mjx-hdw", u"1em 2px 3ex"});
    return adaptor.node(u"span", attributes);
}

common::CommonOutputJaxConfig makeConfig(common::FontRepository const &fonts,
                                         folia::math::AbstractDOMAdaptor *adaptor = nullptr,
                                         folia::math::DOMNodeRef parent = {},
                                         folia::math::DOMNodeRef container = {},
                                         common::CommonOptions options = {}) {
    common::CommonOutputJaxConfig config{
        {16, 8, 1280, .5, {}, false}, std::move(options), fonts, "MathJaxNewcm"};
    if (adaptor)
        config.htmlMeasurement.emplace(
            common::HtmlMeasurement{*adaptor, std::move(parent), std::move(container)});
    return config;
}

bool near(double actual, double expected) {
    return std::abs(actual - expected) < 1e-9;
}

mml::MmlMath &appendScaleMath(mml::MmlDocument &document, std::u16string_view width, bool display) {
    auto &math = dynamic_cast<mml::MmlMath &>(document.factory().make(mml::NodeKind::math));
    math.attributes().set(u"overflow", u"scale");
    math.attributes().set(u"display", display ? u"block" : u"inline");
    auto &space = dynamic_cast<mml::MmlMspace &>(document.factory().make(mml::NodeKind::mspace));
    space.attributes().set(u"width", std::u16string(width));
    document.append(math, space);
    document.setRoot(math);
    math.setInheritedAttributes();
    return math;
}

mml::XMLNode &appendXml(mml::MmlDocument &document, folia::math::DOMNodeRef const &payload,
                        std::shared_ptr<mml::XmlDomAdaptor> const &owner) {
    auto &raw = document.factory().make(mml::NodeKind::xml);
    auto &xml = dynamic_cast<mml::XMLNode &>(raw);
    xml.setXML(owner->fromDOM(payload), owner);
    auto &math = document.factory().make(mml::NodeKind::math);
    document.append(math, xml);
    document.setRoot(math);
    math.setInheritedAttributes();
    return xml;
}

XmlProbe &buildXml(common::LayoutJob &job, mml::XMLNode &xml) {
    (void)job.build();
    auto *wrapper = dynamic_cast<XmlProbe *>(job.jax().wrapperFor(xml));
    if (!wrapper)
        throw std::logic_error("XML wrapper registration did not produce XmlProbe");
    return *wrapper;
}
} // namespace

suite native_common_xml = [] {
    "XML font styles use an explicit typed metrics family when the parent family is empty"_test = [] {
        auto fonts = folia::math::fonts::makeNewcmFontData();
        NoTextMeasurement measurement;
        CountingLite adaptor;
        auto parent = adaptor.node(u"div");
        auto container = adaptor.node(u"div");
        auto config = makeConfig(fonts.fonts, &adaptor, parent, container);
        config.metrics.family = "ExplicitMetrics";

        auto owner = mml::XmlDomAdaptor::create();
        mml::MmlDocument document;
        auto &xml = appendXml(document, makeInputXml(adaptor), owner);
        common::LayoutJob job(document, std::move(config), measurement, xmlCatalog(),
                              makeLinebreaks);
        auto &wrapper = buildXml(job, xml);
        expect(wrapper.parent != nullptr);
        if (!wrapper.parent)
            return;
        wrapper.parent->styles.emplace();
        wrapper.parent->styles->set(u"font-family", folia::math::styles::StyleValue(u""));

        const auto styles = wrapper.getFontStyles();
        expect(styles.size() == 2_u) << "XML font styles retain family and size only";
        if (styles.size() != 2_u)
            return;
        expect(styles[0].first == "font-family");
        expect(styles[0].second == u"ExplicitMetrics")
            << "typed Common metrics family is used after the empty parent family";
        expect(styles[1].first == "font-size");
        expect(styles[1].second == u"16px")
            << "font size uses em times the wrapper relative scale";
    };

    "automatic htmlHDW follows the typed adaptor capability and rejects absent services"_test = [] {
        auto fonts = folia::math::fonts::makeNewcmFontData();
        NoTextMeasurement measurement;
        CountingLite adaptor;
        auto parent = adaptor.node(u"div");
        auto container = adaptor.node(u"div");
        auto config = makeConfig(fonts.fonts, &adaptor, parent, container);
        mml::MmlDocument document;
        auto owner = mml::XmlDomAdaptor::create();
        auto &xml = appendXml(document, makeInputXml(adaptor), owner);
        common::LayoutJob job(document, config, measurement, xmlCatalog(), makeLinebreaks);
        (void)job.build();
        expect(job.jax().options().htmlHDW == common::CommonOptions::HtmlHDW::force)
            << "Lite adaptor forces Source HDW measurement mode";
        const auto &services = job.jax().htmlMeasurement();
        expect(&services.adaptor == &adaptor) << "request keeps its typed measurement adaptor";
        expect(services.parent == parent) << "request keeps the math-start parent";
        expect(services.container == container) << "request keeps its output container";
        auto *wrapper = dynamic_cast<XmlProbe *>(job.jax().wrapperFor(xml));
        expect(wrapper != nullptr) << "XML wrapper is registered in the request";
        if (wrapper) {
            (void)wrapper->getBBox();
            expect(adaptor.measuredKinds ==
                   std::vector<std::u16string>{u"mjx-xml-block", u"mjx-baseline"})
                << "real Lite measurement observes Source content and baseline nodes";
            expect(!adaptor.parent(container)) << "real Lite measurement cleans the container";
        }
        expect(throws<std::logic_error>([&] {
            auto missing = makeConfig(fonts.fonts);
            common::LayoutJob noServices(document, std::move(missing), measurement, xmlCatalog(),
                                         makeLinebreaks);
            auto &wrapper = buildXml(noServices, xml);
            (void)wrapper.getBBox();
        })) << "XML output explicitly requires request services";
    };

    "XML measurement uses nodeBBox results and removes request nodes after success or throw"_test =
        [] {
            auto fonts = folia::math::fonts::makeNewcmFontData();
            NoTextMeasurement measurement;
            RecordingLite adaptor;
            auto parent = adaptor.node(u"div");
            auto container = adaptor.node(u"div");
            auto owner = mml::XmlDomAdaptor::create();
            mml::MmlDocument document;
            auto &xml = appendXml(document, makeInputXml(adaptor), owner);
            common::CommonOptions scaledOptions;
            scaledOptions.scale = .5;
            common::CommonOutputJaxConfig config =
                makeConfig(fonts.fonts, &adaptor, parent, container, scaledOptions);
            common::LayoutJob job(document, config, measurement, xmlCatalog(), makeLinebreaks);
            auto &wrapper = buildXml(job, xml);
            auto *mathWrapper = wrapper.parent;
            expect(mathWrapper != nullptr) << "root math wrapper is available for style precedence";
            if (mathWrapper) {
                mathWrapper->styles.emplace();
                mathWrapper->styles->set(u"font-family",
                                         folia::math::styles::StyleValue(u"ParentFont"));
            }

            const auto &bbox = wrapper.getBBox();
            expect(bbox.w == 25.0) << "XML width follows actual content measurement bounds";
            expect(bbox.h == .625) << "XML height uses baseline-to-content top distance";
            expect(bbox.d == 4.375) << "XML depth uses measured content bottom";
            expect(adaptor.calls == 2_u) << "both content and baseline are measured";
            expect(adaptor.measuredKinds ==
                   std::vector<std::u16string>{u"mjx-xml-block", u"mjx-baseline"})
                << "Source measures content before baseline";
            expect(!adaptor.parent(container)) << "success removes the request container";
            expect(adaptor.childNodes(container).empty()) << "success removes the temporary node";

            const auto styles = wrapper.getFontStyles();
            expect(styles.size() == 2_u) << "font styles contain family and size";
            expect(styles[0].second == u"ParentFont") << "parent style outranks output font family";
            expect(styles[1].second == u"16px") << "font size uses em times relative scale";
            XmlProbe::addHdwCalls = 0;
            const auto html = wrapper.getHTML();
            expect(html != nullptr) << "Source HTML wrapper is returned as a DOM node";
            expect(XmlProbe::addHdwCalls == 1_u) << "non-unit output scale calls backend addHDW";
            expect(adaptor.kind(html) == u"mjx-html") << "getHTML creates the Source outer element";
            expect(adaptor.getAttribute(html, u"variant") ==
                   std::optional<std::u16string>{
                       folia::math::utf16::decode(wrapper.parent->variant).text})
                << "Source parent variant is retained";
            const auto serialized = adaptor.serializeXML(html);
            expect(serialized.find(u"ParentFont") != std::u16string::npos)
                << "parent font style is present in the generated HTML wrapper";

            RecordingLite throwingAdaptor;
            throwingAdaptor.throwOnContent = true;
            auto throwingParent = throwingAdaptor.node(u"div");
            auto throwingContainer = throwingAdaptor.node(u"div");
            auto throwingOwner = mml::XmlDomAdaptor::create();
            mml::MmlDocument throwingDocument;
            auto &throwingXml =
                appendXml(throwingDocument, makeInputXml(throwingAdaptor), throwingOwner);
            auto throwingConfig =
                makeConfig(fonts.fonts, &throwingAdaptor, throwingParent, throwingContainer);
            common::LayoutJob throwingJob(throwingDocument, std::move(throwingConfig), measurement,
                                          xmlCatalog(), makeLinebreaks);
            auto &throwingWrapper = buildXml(throwingJob, throwingXml);
            expect(throws<std::runtime_error>([&] { (void)throwingWrapper.getBBox(); }))
                << "nodeBBox failure propagates from Common XML measurement";
            expect(!throwingAdaptor.parent(throwingContainer)) << "throw removes request container";
            expect(throwingAdaptor.childNodes(throwingContainer).empty())
                << "throw removes temporary measurement node";
        };

    "Source HDW dimensions retain post-length conversion scale"_test =
        [] {
            auto fonts = folia::math::fonts::makeNewcmFontData();
            NoTextMeasurement measurement;
            RecordingLite adaptor;
            auto parent = adaptor.node(u"div");
            auto container = adaptor.node(u"div");
            common::CommonOptions options;
            options.htmlHDW = common::CommonOptions::HtmlHDW::use;
            auto config = makeConfig(fonts.fonts, &adaptor, parent, container, options);
            mml::MmlDocument document;
            auto owner = mml::XmlDomAdaptor::create();
            auto &xml = appendXml(document, makeInputXml(adaptor, true), owner);
            common::LayoutJob job(document, std::move(config), measurement, xmlCatalog(),
                                  makeLinebreaks);
            auto &wrapper = buildXml(job, xml);
            const auto &bbox = wrapper.getBBox();
            const double pxPerEm =
                job.jax().currentMetrics().ex / job.jax().face().parameters.required("x_height");
            expect(near(bbox.h, 2.0))
                << "HDW em uses Source-relative units before metric-scale adjustment";
            expect(near(bbox.d, 4.0 / pxPerEm))
                << "HDW px uses Source pxPerEm and metric-scale adjustment";
            expect(near(bbox.w, 2.586))
                << "HDW ex uses Source TEX x-height ratio and metric-scale adjustment";
            expect(adaptor.calls == 0_u) << "accepted use HDW skips browser measurement";
        };

    "Common Scene resolves Source output scale and display overflow fitting"_test = [] {
        auto fonts = folia::math::fonts::makeNewcmFontData();
        NoTextMeasurement measurement;
        common::CommonOptions options;
        options.scale = 2;

        mml::MmlDocument inlineDocument;
        (void)appendScaleMath(inlineDocument, u"1000px", false);
        common::CommonOutputJaxConfig inlineConfig{
            {16, 8, 100, 1, {}, false}, options, fonts.fonts, "MathJaxNewcm"};
        common::LayoutJob inlineJob(inlineDocument, std::move(inlineConfig), measurement,
                                    xmlCatalog(), makeLinebreaks);
        auto inlineScene = inlineJob.buildScene();
        expect(near(inlineScene.display_layout().font_scale, 2.0))
            << "non-display output scale is metrics scale times Common option scale";

        mml::MmlDocument displayDocument;
        (void)appendScaleMath(displayDocument, u"1000px", true);
        common::CommonOutputJaxConfig displayConfig{
            {16, 8, 100, 1, {}, true}, options, fonts.fonts, "MathJaxNewcm"};
        common::LayoutJob displayJob(displayDocument, std::move(displayConfig), measurement,
                                     xmlCatalog(), makeLinebreaks);
        auto displayScene = displayJob.buildScene();
        const double pxPerEm = displayJob.jax().pxPerEm();
        const double width = displayScene.metrics().width;
        const double expected = 2 * (96 / pxPerEm) / width;
        expect(width > 96 / pxPerEm) << "fixture exceeds Source's available display width";
        expect(near(displayScene.display_layout().font_scale, expected))
            << "display overflow scale fits outer Common width within containerWidth minus 4px";
        expect(displayScene.display_layout().display)
            << "computed font scale preserves Common display layout metadata";

        mml::MmlDocument zeroWidthDocument;
        (void)appendScaleMath(zeroWidthDocument, u"1000px", true);
        common::CommonOutputJaxConfig zeroWidthConfig{
            {16, 8, 4, 1, {}, true}, options, fonts.fonts, "MathJaxNewcm"};
        common::LayoutJob zeroWidthJob(zeroWidthDocument, std::move(zeroWidthConfig), measurement,
                                       xmlCatalog(), makeLinebreaks);
        auto zeroWidthScene = zeroWidthJob.buildScene();
        expect(zeroWidthScene.display_layout().font_scale == 0)
            << "containerWidth four pixels resolves Source display scale to zero";
    };
};
