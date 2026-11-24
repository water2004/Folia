#include "support/folia_test.hpp"

import folia.math.adaptors.lite_adaptor;
import folia.math.core.mml_tree.mml_document;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.svg.wrapper_factory;
import folia.math.output.svg.xml_wrapper;

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace lite = folia::math::adaptors::lite;
namespace mml = folia::math::mml;
namespace svg = folia::math::output::svg;
namespace styles = folia::math::styles;

namespace {
class NoMeasurement final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("XML wrapper boundary test unexpectedly requested metrics");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("XML wrapper boundary test unexpectedly measured text");
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreakVisitor(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

class ProbeSvgXml final : public svg::SvgXmlNode {
    folia::math::BBox box_{folia::math::BBoxData{.w = 1.25, .h = .75, .d = .2}};

  public:
    using SvgXmlNode::SvgXmlNode;
    folia::math::BBox &getBBox() override {
        return box_;
    }
};
} // namespace

suite native_svg_xml_wrapper = [] {
    "addHDW snapshots holder styles before updating outer HTML styles"_test = [] {
        const auto catalog = svg::SvgWrapperFactory::defaultNodes();
        common::CommonWrapperRegistration const *xmlRegistration = nullptr;
        common::CommonWrapperRegistration const *htmlRegistration = nullptr;
        common::CommonWrapperRegistration const *annotationXmlRegistration = nullptr;
        for (auto const &entry : catalog) {
            if (entry.kind == mml::NodeKind::xml)
                xmlRegistration = &entry;
            if (entry.kind == mml::NodeKind::html)
                htmlRegistration = &entry;
            if (entry.kind == mml::NodeKind::annotation_xml)
                annotationXmlRegistration = &entry;
        }
        expect(xmlRegistration != nullptr) << "SVG catalog registers native XML wrapper";
        expect(htmlRegistration != nullptr) << "SVG catalog registers native HTML wrapper";
        expect(annotationXmlRegistration != nullptr)
            << "SVG catalog registers native annotation-xml wrapper";
        if (!xmlRegistration || !htmlRegistration || !annotationXmlRegistration)
            return;
        expect(xmlRegistration->data.styles.size() == 4_u)
            << "XML wrapper retains Common and SVG source class styles";
        expect(htmlRegistration->data.styles.size() == 4_u)
            << "HTML wrapper inherits the same source class styles";
        expect(xmlRegistration->data.styles.front().name == u"foreignObject[data-mjx-html]")
            << "SVG class selector precedes inherited Common XML styles";
        expect(annotationXmlRegistration->data.styles.size() == 1_u)
            << "annotation-xml keeps its own single Source style rule";
        if (!annotationXmlRegistration->data.styles.empty()) {
            expect(annotationXmlRegistration->data.styles.front().name ==
                   u"foreignObject[data-mjx-xml]")
                << "annotation-xml uses its Source foreignObject selector";
            const auto &properties = std::get<std::vector<folia::math::style_json::StyleEntry>>(
                annotationXmlRegistration->data.styles.front().value);
            expect(properties.size() == 3_u) << "annotation-xml retains three Source declarations";
            if (properties.size() == 3) {
                expect(properties[0].name == u"font-family" &&
                       std::get<std::u16string>(properties[0].value) == u"initial")
                    << "annotation-xml font-family declaration matches Source";
                expect(properties[1].name == u"line-height" &&
                       std::get<std::u16string>(properties[1].value) == u"normal")
                    << "annotation-xml line-height declaration matches Source";
                expect(properties[2].name == u"overflow" &&
                       std::get<std::u16string>(properties[2].value) == u"visible")
                    << "annotation-xml overflow declaration matches Source order";
            }
        }

        auto bundle = folia::math::fonts::makeNewcmFontData();
        lite::LiteAdaptor adaptor;
        auto documentDom = adaptor.parse(u"<html><body></body></html>", u"text/html");
        auto parent = adaptor.body(documentDom);
        auto container = adaptor.node(u"div");

        mml::MmlDocument document;
        auto &node = document.make(mml::NodeKind::xml);
        NoMeasurement measurement;
        common::CommonOutputJaxConfig config{{16, 8, 640, 1, "MathJaxNewcm", false},
                                             {},
                                             bundle.fonts,
                                             "MathJaxNewcm",
                                             common::InlineBreakPolicy::forced,
                                             common::HtmlMeasurement{adaptor, parent, container}};
        common::CommonOutputJax jax(document, std::move(config), measurement,
                                    svg::SvgWrapperFactory::defaultNodes(), makeLinebreakVisitor);
        auto &annotationNode = document.make(mml::NodeKind::annotation_xml);
        common::WrapperBuildArguments annotationArgs{jax.factory(), annotationNode, nullptr};
        auto annotationProduct = annotationXmlRegistration->creator.construct(annotationArgs);
        expect(annotationProduct != nullptr) << "annotation-xml registration constructs a wrapper";
        if (annotationProduct)
            expect(annotationXmlRegistration->creator.isInstance(*annotationProduct))
                << "annotation-xml registration recognizes SvgAnnotationXML";

        ProbeSvgXml wrapper(jax.factory(), node);
        styles::StyleList styleList{{"font-family", u"MathJax_Main"}, {"font-size", u"16px"}};
        auto content = adaptor.node(u"span");
        auto holder = wrapper.addHDW(content, styleList);

        const auto holderStyle = adaptor.getAttribute(holder, u"style");
        expect(holderStyle.has_value()) << "holder has its construction-time style attribute";
        if (holderStyle) {
            expect(holderStyle->find(u"font-family: MathJax_Main;") != std::u16string::npos)
                << "holder retains source font-family style";
            expect(holderStyle->find(u"font-size: 16px;") != std::u16string::npos)
                << "holder retains source font-size style";
            expect(holderStyle->find(u"height:") == std::u16string::npos)
                << "holder does not observe later StyleList mutations";
        }

        const auto hasStyle = [&](std::string_view name) {
            for (auto const &entry : styleList)
                if (entry.first == name)
                    return true;
            return false;
        };
        const auto styleValue = [&](std::string_view name) -> std::u16string_view {
            for (auto const &entry : styleList)
                if (entry.first == name)
                    return entry.second;
            return {};
        };
        expect(!hasStyle("font-family")) << "Source removes font-family from outer styles";
        expect(!hasStyle("font-size")) << "Source removes font-size from outer styles";
        expect(hasStyle("height")) << "Source adds measured height to outer styles";
        expect(hasStyle("width")) << "Source adds measured width to outer styles";
        expect(hasStyle("vertical-align")) << "Source adds baseline alignment to outer styles";
        expect(styleValue("height") == u"0.95em") << "height uses measured h+d times scale";
        expect(styleValue("width") == u"1.25em") << "width uses measured w times scale";
        expect(styleValue("vertical-align") == u"-0.2em")
            << "vertical alignment uses negative measured depth";
    };
};
