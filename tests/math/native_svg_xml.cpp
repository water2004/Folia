#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "support/source_svg_paint.hpp"
#include "support/folia_test.hpp"

import folia.math.adaptors.lite_adaptor;
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.xml_dom_adaptor;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.scene.builder;
import folia.math.output.svg.wrapper_factory;
import folia.math.output.svg.writer;
import folia.math.util.utf16;

#include "support/mathml_scene.hpp"

using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;
namespace lite = folia::math::adaptors::lite;

#include "fixtures/common-source/xml-mathml-scene.inc"

namespace {
class XmlMeasurement final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("XML Source fixture unexpectedly requested font metrics");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &request) override {
        throw std::logic_error("XML Source fixture unexpectedly requested unknown text: " +
                               folia::math::utf16::encode(request.text));
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreakVisitor(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

common::CommonOptions sourceOptions(XmlSourceCase const &source) {
    common::CommonOptions options;
    options.displayOverflow = "linebreak";
    options.linebreaks.inlineBreaks = true;
    options.scale = source.scale;
    switch (source.hdw) {
    case XmlHdwPolicy::automatic:
        options.htmlHDW = common::CommonOptions::HtmlHDW::automatic;
        break;
    case XmlHdwPolicy::use:
        options.htmlHDW = common::CommonOptions::HtmlHDW::use;
        break;
    case XmlHdwPolicy::force:
        options.htmlHDW = common::CommonOptions::HtmlHDW::force;
        break;
    case XmlHdwPolicy::ignore:
        options.htmlHDW = common::CommonOptions::HtmlHDW::ignore;
        break;
    }
    return options;
}

std::string serialize(scene::Scene const &value,
                      folia::math::fonts::NewcmFontDataBundle const &bundle) {
    svg::SvgWriteOptions options;
    options.font_cache = svg::FontCacheMode::none;
    options.font_resources = &bundle.svg_resources;
    return svg::write(value, options);
}

struct CapturedScene {
    scene::Scene value;
    std::string before_retirement;
};

CapturedScene capture(XmlSourceCase const &source,
                      folia::math::fonts::NewcmFontDataBundle const &bundle) {
    if (source.observations.empty())
        throw std::invalid_argument("XML fixture is missing Source observations");
    std::optional<scene::Scene> completed;
    std::string before;
    {
        mml::MmlDocument document;
        lite::LiteAdaptor adaptor;
        adaptor.canMeasureNodes = source.can_measure_nodes;
        auto xmlAdaptor = mml::XmlDomAdaptor::create();
        std::vector<folia::math::DOMDocumentRef> payloadDocuments(source.payloads.size());
        auto &root = mathml_scene_test::constructRoot(
            document, source.source.nodes, source.source.attributes,
            [&](mml::XMLNode &node, std::size_t payloadIndex) {
                if (payloadIndex >= source.payloads.size())
                    throw std::out_of_range("XML fixture payload index is invalid");
                auto parsed = adaptor.parse(source.payloads[payloadIndex], u"text/html");
                auto body = adaptor.body(parsed);
                auto payload = adaptor.firstChild(body);
                if (!payload)
                    throw std::logic_error("XML fixture payload parsed without a root child");
                payloadDocuments[payloadIndex] = std::move(parsed);
                node.setXML(xmlAdaptor->fromDOM(payload), xmlAdaptor);
            });
        document.setRoot(root);
        root.setInheritedAttributes();

        auto requestDocument =
            adaptor.parse(u"<html><head></head><body></body></html>", u"text/html");
        auto start = adaptor.body(requestDocument);
        auto parent = adaptor.parent(start);
        if (!parent || folia::math::utf16::encode(adaptor.kind(start)) != source.start_kind ||
            folia::math::utf16::encode(adaptor.kind(parent)) != source.start_parent_kind)
            throw std::logic_error(
                "XML request measurement parent differs from actual Source parent");
        auto measurementContainer = adaptor.node(u"div");
        adaptor.append(adaptor.body(requestDocument), measurementContainer);
        XmlMeasurement measurement;
        const auto &metrics = source.source.metrics;
        common::CommonOutputJaxConfig config{
            {metrics.em, metrics.ex, metrics.container_width,
             source.observations.front().metrics_scale, "", metrics.display},
            sourceOptions(source),
            bundle.fonts,
            "MathJaxNewcm",
            common::InlineBreakPolicy::forced,
            common::HtmlMeasurement{adaptor, parent, measurementContainer}};
        common::LayoutJob job(document, std::move(config), measurement,
                              svg::SvgWrapperFactory::defaultNodes(), makeLinebreakVisitor);
        auto built = job.buildScene();
        before = serialize(built, bundle);
        completed.emplace(std::move(built));
    }
    return {std::move(*completed), std::move(before)};
}

void collectForeign(scene::Group const &group, std::vector<scene::ForeignContent const *> &out) {
    if (group.foreign_content)
        out.push_back(&*group.foreign_content);
    for (auto const &entry : group.entries) {
        if (auto const *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child)
                collectForeign(**child, out);
        }
    }
}

bool close(double a, double b) {
    return std::abs(a - b) <= 1e-9;
}

bool sourceStylesMatch(common::CommonWrapperClassData const &data,
                       std::span<XmlSourceStyleEntry const> expectedStyles) {
    if (expectedStyles.empty())
        return false;
    std::size_t expectedRuleCount = 0;
    for (std::size_t i = 0; i < expectedStyles.size();) {
        ++expectedRuleCount;
        const auto selector = expectedStyles[i].selector;
        while (i < expectedStyles.size()) {
            if (expectedStyles[i].selector != selector)
                break;
            ++i;
        }
    }
    if (data.styles.size() != expectedRuleCount)
        return false;

    std::size_t ruleIndex = 0;
    for (std::size_t i = 0; i < expectedStyles.size(); ++ruleIndex) {
        const auto selector = expectedStyles[i].selector;
        if (ruleIndex >= data.styles.size())
            return false;
        if (data.styles[ruleIndex].name != selector)
            return false;
        const auto *properties = std::get_if<std::vector<folia::math::style_json::StyleEntry>>(
            &data.styles[ruleIndex].value);
        if (!properties)
            return false;
        std::size_t propertyIndex = 0;
        while (i < expectedStyles.size()) {
            if (expectedStyles[i].selector != selector)
                break;
            const auto &expected = expectedStyles[i++];
            if (propertyIndex >= properties->size())
                return false;
            const auto &actual = (*properties)[propertyIndex++];
            if (actual.name != expected.property)
                return false;
            if (expected.kind == XmlSourceStyleKind::text) {
                const auto *value = std::get_if<std::u16string>(&actual.value);
                if (!value)
                    return false;
                if (*value != expected.text)
                    return false;
            } else {
                const auto *value = std::get_if<double>(&actual.value);
                if (!value)
                    return false;
                if (!close(*value, expected.number))
                    return false;
            }
        }
        if (propertyIndex != properties->size())
            return false;
    }
    return true;
}

bool validateForeign(XmlSourceCase const &source, scene::Scene const &value) {
    std::vector<scene::ForeignContent const *> foreign;
    collectForeign(value.root(), foreign);
    if (foreign.size() != source.observations.size())
        return false;
    for (std::size_t i = 0; i < foreign.size(); ++i) {
        auto const &geometry = foreign[i]->geometry;
        auto const &bbox = source.observations[i];
        const double em = source.source.metrics.em * bbox.metrics_scale * bbox.rscale;
        if (!close(geometry.x, 0) || !close(geometry.y, -bbox.h * em) ||
            !close(geometry.width, bbox.w * em) ||
            !close(geometry.height, (bbox.h + bbox.d) * em) ||
            !close(geometry.em_per_pixel, 1.0 / em))
            return false;
        if (foreign[i]->content.kind.empty())
            return false;
    }
    return true;
}

void reportDifference(std::string_view name, std::string_view expected, std::string_view actual) {
    std::size_t first = 0;
    while (first < expected.size() && first < actual.size() && expected[first] == actual[first])
        ++first;
    const auto begin = first > 48 ? first - 48 : 0;
    std::cerr << "XML SVG mismatch case=" << name << " first_byte=" << first
              << " expected_size=" << expected.size() << " actual_size=" << actual.size()
              << "\nexpected: " << expected.substr(begin, 112)
              << "\nactual:   " << actual.substr(begin, 112) << '\n';
}
} // namespace

suite native_svg_xml = [] {
    "pinned_MathML_XML_foreignObject_matches_Source_after_request_retirement"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        const auto catalog = svg::SvgWrapperFactory::defaultNodes();
        const common::CommonWrapperRegistration *xmlRegistration = nullptr;
        const common::CommonWrapperRegistration *htmlRegistration = nullptr;
        const common::CommonWrapperRegistration *annotationXmlRegistration = nullptr;
        for (auto const &entry : catalog) {
            if (entry.kind == mml::NodeKind::xml)
                xmlRegistration = &entry;
            if (entry.kind == mml::NodeKind::html)
                htmlRegistration = &entry;
            if (entry.kind == mml::NodeKind::annotation_xml)
                annotationXmlRegistration = &entry;
        }
        expect(bool(xmlRegistration != nullptr)) << "SVG catalog has XML wrapper registration";
        expect(bool(htmlRegistration != nullptr)) << "SVG catalog has HTML wrapper registration";
        expect(bool(annotationXmlRegistration != nullptr))
            << "SVG catalog has annotation-xml wrapper registration";
        if (xmlRegistration != nullptr)
            expect(bool(sourceStylesMatch(xmlRegistration->data, sourceXmlStyleEntries)))
                << "native XML registration styles match pinned Source style entries";
        if (htmlRegistration != nullptr)
            expect(bool(sourceStylesMatch(htmlRegistration->data, sourceXmlStyleEntries)))
                << "native HTML registration styles match pinned Source style entries";
        if (annotationXmlRegistration != nullptr)
            expect(bool(sourceStylesMatch(annotationXmlRegistration->data,
                                          sourceAnnotationXmlStyleEntries)))
                << "native annotation-xml registration styles match pinned Source style entries";

        bool sawAutomaticForce = false;
        bool sawAutomaticForceDimensions = false;
        bool sawAuthoredStyleFallback = false;
        for (auto const &source : sourceXmlCases) {
            auto captured = capture(source, bundle);
            const auto afterRetirement = serialize(captured.value, bundle);
            const auto expectedSvg = source_svg_paint::materialize(source.source.source_svg);
            const bool matches = captured.before_retirement == expectedSvg;
            const bool retiredMatches = afterRetirement == expectedSvg;
            if (!matches)
                reportDifference(source.source.name, expectedSvg,
                                 captured.before_retirement);
            expect(bool(matches)) << "independent Source SVG matches case=" << source.source.name;
            expect(bool(retiredMatches))
                << "Scene survives MML/job/adaptor retirement case=" << source.source.name;
            const bool foreignValid = validateForeign(source, captured.value);
            expect(bool(foreignValid))
                << "Scene owns Source foreignObject geometry case=" << source.source.name;
            if (source.hdw == XmlHdwPolicy::automatic && !source.can_measure_nodes &&
                !source.observations.empty()) {
                const auto &observation = source.observations.front();
                const bool resolvedForce = observation.source_html_hdw == "force";
                sawAutomaticForce = sawAutomaticForce || resolvedForce;
                expect(bool(resolvedForce)) << "automatic htmlHDW resolves to forced dimensions "
                                               "when measurement is unavailable";
                if (observation.html.find("data-mjx-hdw=") != std::string_view::npos) {
                    const bool dimensions = observation.w > 0 && observation.h > 0;
                    sawAutomaticForceDimensions = sawAutomaticForceDimensions || dimensions;
                    expect(bool(dimensions))
                        << "automatic forced policy consumes authored nonzero HDW when present";
                }
            }
            if (source.source.name == "xml-authored-font-style-removed-times-fallback" &&
                !source.observations.empty()) {
                bool authored = false;
                for (auto const &node : source.source.nodes) {
                    if (node.kind != "annotation-xml")
                        continue;
                    authored = std::ranges::any_of(
                        source.source.attributes.subspan(node.attribute_first,
                                                         node.attribute_count),
                        [](auto const &attribute) {
                            return attribute.name == u"style" &&
                                   attribute.text.find(u"font-family: ProbeFamily") !=
                                       std::u16string_view::npos;
                        });
                }
                sawAuthoredStyleFallback = authored && source.observations[0].family == u"Times" &&
                                           source.observations[0].metrics_family.empty();
                expect(bool(sawAuthoredStyleFallback))
                    << "Source parent font style is removed before wrapper style lookup; actual "
                       "Times fallback is recorded";
            }
        }
        expect(bool(sawAutomaticForce)) << "automatic force policy was observed";
        expect(bool(sawAutomaticForceDimensions))
            << "automatic force policy with authored HDW dimensions was observed";
        expect(bool(sawAuthoredStyleFallback))
            << "authored parent style fallback observation was checked";
    };
};
