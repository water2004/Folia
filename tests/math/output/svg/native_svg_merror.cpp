#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "math/support/source_svg_paint.hpp"
#include "support/folia_test.hpp"

import folia.math.adaptors.lite_adaptor;
import folia.math.core.mml_tree.mml_document;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.scene.builder;
import folia.math.output.svg.wrapper_factory;
import folia.math.output.svg.writer;
import folia.math.util.utf16;

#include "math/support/mathml_scene.hpp"

using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

#include "math/fixtures/common-source/merror-mathml-scene.inc"

namespace {
class LiteUnknownTextMeasurement final : public common::MeasurementTarget {
    folia::math::adaptors::lite::LiteAdaptor &adaptor_;

  public:
    explicit LiteUnknownTextMeasurement(folia::math::adaptors::lite::LiteAdaptor &adaptor) noexcept
        : adaptor_(adaptor) {}

    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("merror fixture unexpectedly requested font metrics");
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &request) override {
        const std::array<folia::math::DOMNodeRef, 1> children{adaptor_.text(request.text)};
        const auto textElement = adaptor_.node(
            u"text", std::span<folia::math::AttributeData const>{},
            std::span<folia::math::DOMNodeRef const>{children}, u"http://www.w3.org/2000/svg");
        const auto [width, height] = adaptor_.nodeSize(textElement, 1000, true);
        (void)height;
        return {width, .75, .2};
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

struct CapturedScene {
    scene::Scene value;
    std::string before_retirement;
};

CapturedScene captureMerrorSourceCase(MerrorSourceCase const &source,
                                      folia::math::fonts::NewcmFontDataBundle const &bundle) {
    mml::MmlDocument document;
    auto &root =
        mathml_scene_test::constructRoot(document, source.source.nodes, source.source.attributes);
    document.setRoot(root);
    root.setInheritedAttributes();
    folia::math::adaptors::lite::LiteAdaptor adaptor;
    LiteUnknownTextMeasurement measurement(adaptor);
    common::CommonOutputJaxConfig config{{source.source.metrics.em, source.source.metrics.ex,
                                          source.source.metrics.container_width, 1.0,
                                          "MathJaxNewcm", source.source.metrics.display},
                                         sourceOptions(),
                                         bundle.fonts,
                                         "MathJaxNewcm",
                                         common::InlineBreakPolicy::forced};
    common::LayoutJob job(document, std::move(config), measurement,
                          svg::SvgWrapperFactory::defaultNodes(), makeLinebreakVisitor);
    auto completed = job.buildScene();
    auto before = serialize(completed, bundle);
    return {std::move(completed), std::move(before)};
}

void collectGroups(scene::Group const &group, std::vector<scene::Group const *> &all,
                   std::vector<scene::Group const *> &merrors) {
    all.push_back(&group);
    if (group.metadata.source_tag == "merror")
        merrors.push_back(&group);
    for (auto const &entry : group.entries)
        if (auto const *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child)
                collectGroups(**child, all, merrors);
        }
}

bool isCss(scene::PaintColor const &paint, std::string_view value) {
    const auto *css = std::get_if<scene::CssColor>(&paint);
    if (!css)
        return false;
    return css->value == value;
}

bool hasStylesheet(scene::Paint const &paint, std::string_view fill, std::string_view stroke) {
    if (!paint.stylesheet)
        return false;
    return isCss(paint.stylesheet->fill, fill) && isCss(paint.stylesheet->stroke, stroke);
}

bool sourceClassStylesMatch(folia::math::output::common::CommonWrapperClassData const &data) {
    if (data.styles.size() != std::size(sourceMerrorStyleRules))
        return false;
    for (std::size_t i = 0; i < std::size(sourceMerrorStyleRules); ++i) {
        auto const &expected = sourceMerrorStyleRules[i];
        auto const &rule = data.styles[i];
        if (rule.name != expected.selector)
            return false;
        auto const *properties =
            std::get_if<std::vector<folia::math::style_json::StyleEntry>>(&rule.value);
        if (!properties)
            return false;
        if (properties->size() != 2 || (*properties)[0].name != u"fill" ||
            (*properties)[1].name != u"stroke")
            return false;
        auto const *fill = std::get_if<std::u16string>(&(*properties)[0].value);
        auto const *stroke = std::get_if<std::u16string>(&(*properties)[1].value);
        if (!fill || !stroke)
            return false;
        if (*fill != expected.fill || *stroke != expected.stroke)
            return false;
    }
    return true;
}

bool isMerrorNode(mathml_scene_test::Node const &node) {
    return node.kind == "merror";
}

std::vector<std::string> sourceMerrorTitles(mathml_scene_test::Case const &source) {
    std::vector<std::string> result;
    for (auto const &node : source.nodes) {
        if (!isMerrorNode(node))
            continue;
        std::string title;
        for (auto const &attribute :
             source.attributes.subspan(node.attribute_first, node.attribute_count)) {
            if (attribute.name == u"title" &&
                attribute.kind == mathml_scene_test::AttributeKind::text)
                title = folia::math::utf16::encode(attribute.text);
        }
        result.push_back(std::move(title));
    }
    return result;
}

bool isRuleBackground(scene::Group const &group, MerrorSourceBBox const &sourceBox) {
    if (group.entries.empty())
        return false;
    const auto *primitive = std::get_if<scene::Primitive>(&group.entries.front());
    const auto *rule = primitive ? std::get_if<scene::Rule>(primitive) : nullptr;
    if (!rule)
        return false;
    if (rule->role != scene::ElementRole::error_background || !rule->paint.stylesheet)
        return false;
    constexpr double epsilon = 1e-12;
    const auto close = [=](double a, double b) { return std::abs(a - b) <= epsilon; };
    return close(rule->bounds.x, 0) && close(rule->bounds.y, -sourceBox.depth) &&
           close(rule->bounds.width, sourceBox.width) &&
           close(rule->bounds.height, sourceBox.height + sourceBox.depth) &&
           isCss(rule->paint.stylesheet->fill, "yellow") &&
           std::holds_alternative<scene::NoPaint>(rule->paint.stylesheet->stroke);
}

bool validateMerrorStructure(MerrorSourceCase const &source, scene::Scene const &value,
                             std::size_t &linkedGroups, std::size_t &viewportGroups,
                             std::size_t &blueGroups, std::size_t &styledGroups) {
    std::vector<scene::Group const *> all, merrors;
    collectGroups(value.root(), all, merrors);
    if (merrors.size() != source.box_count ||
        sourceMerrorTitles(source.source).size() != merrors.size())
        return false;

    bool valid = true;
    const auto titles = sourceMerrorTitles(source.source);
    for (std::size_t i = 0; i < merrors.size(); ++i) {
        auto const &group = *merrors[i];
        valid = valid && isRuleBackground(group, source.boxes[i]);
        const auto titleIndex =
            std::find_if(group.entries.begin(), group.entries.end(), [](auto const &entry) {
                const auto *primitive = std::get_if<scene::Primitive>(&entry);
                if (!primitive)
                    return false;
                return std::holds_alternative<scene::Title>(*primitive);
            });
        const bool expectedTitle = !titles[i].empty();
        valid = valid && (expectedTitle == (titleIndex != group.entries.end()));
        if (expectedTitle && titleIndex != group.entries.end()) {
            const auto *primitive = std::get_if<scene::Primitive>(&*titleIndex);
            const auto *title = primitive ? std::get_if<scene::Title>(primitive) : nullptr;
            if (!title)
                return false;
            valid = valid && title->text == titles[i];
            valid = valid && std::next(group.entries.begin()) == titleIndex;
        }
        for (auto const &entry : group.entries) {
            const auto *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry);
            if (!child)
                continue;
            if (!*child)
                continue;
            const auto &childGroup = **child;
            const bool excluded = childGroup.link.has_value() || childGroup.viewport.has_value();
            if (excluded)
                valid = valid && !childGroup.paint.stylesheet;
            else {
                valid = valid && hasStylesheet(childGroup.paint, "red", "red");
                ++styledGroups;
            }
        }
    }
    for (auto const *group : all) {
        if (group->link) {
            ++linkedGroups;
            valid = valid && !group->paint.stylesheet;
        }
        if (group->viewport) {
            ++viewportGroups;
            valid = valid && !group->paint.stylesheet;
        }
        if (isCss(group->paint.fill, "blue"))
            ++blueGroups;
    }
    return valid;
}

void reportDifference(std::string_view name, std::string_view expected, std::string_view actual) {
    std::size_t first = 0;
    while (first < expected.size() && first < actual.size() && expected[first] == actual[first])
        ++first;
    const auto begin = first > 48 ? first - 48 : 0;
    std::cerr << "merror SVG mismatch case=" << name << " first_byte=" << first
              << " expected_size=" << expected.size() << " actual_size=" << actual.size()
              << "\nexpected: " << expected.substr(begin, 112)
              << "\nactual:   " << actual.substr(begin, 112) << '\n';
}
} // namespace

suite native_svg_merror = [] {
    "pinned_MathML_merror_matches_SVG_and_preserves_owned_styles_title_and_background"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        const auto catalog = svg::SvgWrapperFactory::defaultNodes();
        const auto merrorRegistration =
            std::find_if(catalog.begin(), catalog.end(),
                         [](auto const &entry) { return entry.kind == mml::NodeKind::merror; });
        expect(bool(merrorRegistration != catalog.end())) << "SVG catalog registers merror";
        if (merrorRegistration == catalog.end())
            return;
        expect(bool(sourceClassStylesMatch(merrorRegistration->data)))
            << "SVG default catalog merror registration matches actual Source class style rules";
        bool foundAuthoredBlue = false;
        bool foundLinkedChild = false;
        bool foundNestedSvgViewport = false;
        bool foundDirectStyledChild = false;
        for (auto const &source : sourceMerrorCases) {
            auto captured = captureMerrorSourceCase(source, bundle);
            const auto afterRetirement = serialize(captured.value, bundle);
            const auto expectedSvg = source_svg_paint::materialize(source.source.source_svg);
            const bool svgMatches = captured.before_retirement == expectedSvg;
            const bool retiredMatches = afterRetirement == expectedSvg;
            if (!svgMatches)
                reportDifference(source.source.name, expectedSvg,
                                 captured.before_retirement);
            expect(bool(svgMatches)) << "Source SVG bytes match for " << source.source.name;
            expect(bool(retiredMatches))
                << "Scene remains serializable after MML document, job, and adaptor retirement for "
                << source.source.name;

            std::size_t linkedGroups = 0, viewportGroups = 0, blueGroups = 0, styledGroups = 0;
            const bool structure = validateMerrorStructure(
                source, captured.value, linkedGroups, viewportGroups, blueGroups, styledGroups);
            expect(bool(structure))
                << "merror Scene has Source bbox, Rule/Title order, and scoped paint for "
                << source.source.name;
            if (source.source.name == "merror-authored-blue") {
                foundAuthoredBlue = blueGroups >= 2 && styledGroups > 0;
                expect(bool(foundAuthoredBlue))
                    << "authored blue child paint survives the separate red/red stylesheet value";
            }
            if (source.source.source_svg.find("href=\"https://example.test/merror\"") !=
                std::string_view::npos) {
                foundLinkedChild = linkedGroups > 0;
                foundNestedSvgViewport =
                    source.source.source_svg.find("<svg", source.source.source_svg.find("<svg") +
                                                              4) != std::string_view::npos &&
                    viewportGroups > 0;
                expect(bool(foundLinkedChild))
                    << "linked child group is excluded from direct-g styling";
                expect(bool(foundNestedSvgViewport))
                    << "assembled child nested SVG uses a viewport without direct-g stylesheet "
                       "paint";
            }
            foundDirectStyledChild = foundDirectStyledChild || styledGroups > 0;
        }
        expect(bool(foundDirectStyledChild))
            << "ordinary direct g children retain red/red stylesheet paint";
        expect(bool(foundAuthoredBlue)) << "authored paint assertion ran on Source blue case";
        expect(bool(foundLinkedChild)) << "Source corpus includes linked child selector boundary";
        expect(bool(foundNestedSvgViewport))
            << "Source corpus includes nested SVG viewport selector boundary";
    };
};
