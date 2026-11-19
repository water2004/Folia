#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "support/source_svg_paint.hpp"
#include "support/folia_test.hpp"

import folia.math.core.mml_tree.mml_document;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.scene;
import folia.math.output.svg.wrapper_factory;
import folia.math.output.svg.writer;
import folia.math.util.utf16;

#include "support/mathml_scene.hpp"

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace mml = folia::math::mml;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

#include "fixtures/mglyph_scene_source.hpp"

namespace {
bool close(double a, double b) {
    return std::abs(a - b) <= 1e-9;
}

class SourceMeasurements final : public common::MeasurementTarget {
    MglyphSourceCase const &source_;
    std::size_t next_{};
    bool matched_{true};

  public:
    explicit SourceMeasurements(MglyphSourceCase const &source) : source_(source) {}

    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("mglyph Source witness unexpectedly requested font metrics");
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &request) override {
        if (next_ >= source_.measurements.size()) {
            matched_ = false;
            throw std::logic_error(
                "mglyph requested unknown text absent from Source measurement capture: " +
                folia::math::utf16::encode(request.text));
        }
        const auto &expected = source_.measurements[next_++];
        const bool requestMatches =
            request.text == expected.text && request.variant == expected.variant &&
            request.font.family == expected.font_family && request.font.italic == expected.italic &&
            request.font.bold == expected.bold;
        if (!requestMatches) {
            matched_ = false;
            throw std::logic_error("mglyph unknown-text request differs from Source capture: " +
                                   folia::math::utf16::encode(request.text));
        }
        return {expected.w, expected.h, expected.d};
    }

    [[nodiscard]] bool matchesSource() const noexcept {
        return matched_ && next_ == source_.measurements.size();
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreakVisitor(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

std::string serialize(scene::Scene const &value,
                      folia::math::fonts::NewcmFontDataBundle const &bundle,
                      MglyphSourceCase const &source) {
    svg::SvgWriteOptions options;
    options.font_cache =
        source.font_cache == "local" ? svg::FontCacheMode::local : svg::FontCacheMode::none;
    options.font_resources = &bundle.svg_resources;
    options.local_cache_prefixes.reserve(source.local_cache_prefixes.size());
    for (auto prefix : source.local_cache_prefixes)
        options.local_cache_prefixes.emplace_back(prefix);
    return svg::write(value, options);
}

struct ImageSnapshot {
    scene::Image value;
    std::string parent_source_tag;
};

void collectImages(scene::Group const &group, std::vector<ImageSnapshot> &images) {
    for (auto const &entry : group.entries) {
        if (auto const *primitive = std::get_if<scene::Primitive>(&entry)) {
            if (auto const *image = std::get_if<scene::Image>(primitive))
                images.push_back({*image, group.metadata.source_tag});
        } else if (auto const *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child)
                collectImages(**child, images);
        }
    }
}

void visitMml(mml::MmlNode &node, std::vector<mml::MmlNode *> &nodes) {
    nodes.push_back(&node);
    for (auto *child : node.children())
        if (child)
            visitMml(*child, nodes);
}

std::optional<std::u16string_view>
sourceAttribute(MglyphSourceCase const &source, std::size_t nodeIndex, std::u16string_view wanted) {
    if (nodeIndex >= source.nodes.size())
        return std::nullopt;
    const auto &node = source.nodes[nodeIndex];
    if (node.attribute_first > source.attributes.size() ||
        node.attribute_count > source.attributes.size() - node.attribute_first)
        return std::nullopt;
    for (const auto &attribute :
         source.attributes.subspan(node.attribute_first, node.attribute_count))
        if (attribute.name == wanted && attribute.kind == mathml_scene_test::AttributeKind::text)
            return attribute.text;
    return std::nullopt;
}

struct Captured {
    scene::Scene value;
    std::string before_retirement;
    folia::math::BBox root_bbox;
    std::vector<folia::math::BBox> mglyph_boxes;
    std::vector<std::array<double, 3>> image_layout_sizes;
    std::vector<std::optional<std::u16string>> legacy_texts;
    std::vector<ImageSnapshot> images;
    std::size_t source_mglyph_nodes{};
    std::size_t merror_nodes{};
    bool measurements_match{};
};

Captured capture(MglyphSourceCase const &source,
                 folia::math::fonts::NewcmFontDataBundle const &bundle) {
    std::optional<scene::Scene> completed;
    std::string before;
    folia::math::BBox rootBox;
    std::vector<folia::math::BBox> glyphBoxes;
    std::vector<std::array<double, 3>> imageLayoutSizes;
    std::vector<std::optional<std::u16string>> legacyTexts;
    std::vector<ImageSnapshot> images;
    std::size_t glyphNodes = 0, errorNodes = 0;
    bool measurementsMatch = false;
    {
        mml::MmlDocument document;
        auto &root = mathml_scene_test::constructRoot(document, source.nodes, source.attributes);
        document.setRoot(root);
        root.setInheritedAttributes();
        SourceMeasurements measurement(source);
        common::CommonOptions options;
        options.displayOverflow = "linebreak";
        options.linebreaks.inlineBreaks = true;
        common::CommonOutputJaxConfig config{
            {source.em, source.ex, source.width, 1.0, "MathJaxNewcm", source.display},
            std::move(options),
            bundle.fonts,
            "MathJaxNewcm",
            common::InlineBreakPolicy::forced};
        common::LayoutJob job(document, std::move(config), measurement,
                              svg::SvgWrapperFactory::defaultNodes(), makeLinebreakVisitor);
        auto &commonRoot = job.build();
        rootBox = commonRoot.getBBox();

        std::vector<mml::MmlNode *> nodes;
        visitMml(root, nodes);
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            auto *node = nodes[i];
            if (node->kind() == mml::NodeKind::merror)
                ++errorNodes;
            if (node->kind() != mml::NodeKind::mglyph)
                continue;
            ++glyphNodes;
            auto *wrapper = dynamic_cast<common::CommonMglyph *>(job.jax().wrapperFor(*node));
            if (!wrapper)
                throw std::logic_error("MML mglyph has no native CommonMglyph wrapper");
            glyphBoxes.push_back(wrapper->getBBox());
            imageLayoutSizes.push_back({wrapper->width, wrapper->height, wrapper->valign});
            std::optional<std::u16string> charText;
            if (auto *character = wrapper->charWrapper()) {
                auto *text =
                    character->node ? dynamic_cast<mml::TextNode *>(character->node) : nullptr;
                if (!text)
                    throw std::logic_error("legacy mglyph charWrapper does not own a TextNode");
                charText = text->getText();
                if (std::find(nodes.begin(), nodes.end(), text) != nodes.end())
                    throw std::logic_error(
                        "legacy mglyph charWrapper TextNode leaked into MML children");
            }
            legacyTexts.push_back(std::move(charText));
        }
        auto value = job.jax().buildScene(commonRoot);
        collectImages(value.root(), images);
        before = serialize(value, bundle, source);
        measurementsMatch = measurement.matchesSource();
        completed.emplace(std::move(value));
    }
    return {std::move(*completed),
            std::move(before),
            rootBox,
            std::move(glyphBoxes),
            std::move(imageLayoutSizes),
            std::move(legacyTexts),
            std::move(images),
            glyphNodes,
            errorNodes,
            measurementsMatch};
}

void reportDifference(std::string_view name, std::string_view expected, std::string_view actual) {
    std::size_t first = 0;
    while (first < expected.size() && first < actual.size() && expected[first] == actual[first])
        ++first;
    const auto begin = first > 48 ? first - 48 : 0;
    std::cerr << "mglyph SVG mismatch case=" << name << " first_byte=" << first
              << " expected_size=" << expected.size() << " actual_size=" << actual.size()
              << "\nexpected: " << expected.substr(begin, 112)
              << "\nactual:   " << actual.substr(begin, 112) << '\n';
}

bool imageAttributesOrdered(MglyphSourceCase const &source) {
    if (source.kind != MglyphSourceKind::image)
        return source.image_attributes.empty();
    constexpr std::array<std::string_view, 6> expected{
        "width", "height", "transform", "preserveAspectRatio", "aria-label", "href"};
    if (source.image_attributes.size() != expected.size())
        return false;
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (source.image_attributes[i].image_index != 0 ||
            source.image_attributes[i].name != expected[i])
            return false;
    return true;
}
} // namespace

suite native_svg_mglyph_source = [] {
    "pinned_MathML_mglyph_Source_cases_match_scene_images_legacy_measurements_and_retirement"_test =
        [] {
            auto bundle = folia::math::fonts::makeNewcmFontData();
            for (auto const &source : sourceMglyphCases) {
                expect(source.error_name.empty())
                    << "Source conversion completed case=" << source.name;
                expect(imageAttributesOrdered(source))
                    << "Source image attributes retain actual ordered shape case=" << source.name;
                const auto rootDisplay = sourceAttribute(source, 0, u"display");
                const bool rootIsBlock = rootDisplay && *rootDisplay == u"block";
                expect(source.display == rootIsBlock)
                    << "actual Source MathItem display follows the root display attribute case="
                    << source.name;
                const auto captured = capture(source, bundle);
                expect(bool(close(captured.root_bbox.w, source.root_w) &&
                            close(captured.root_bbox.h, source.root_h) &&
                            close(captured.root_bbox.d, source.root_d)))
                    << "Common root bbox matches Source case=" << source.name;
                expect(captured.source_mglyph_nodes == (source.mglyph_bbox ? 1u : 0u))
                    << "actual source MML mglyph count matches wrapper observation case="
                    << source.name;
                expect(captured.merror_nodes ==
                       (source.kind == MglyphSourceKind::verification ? 1u : 0u))
                    << "verification merror is distinguished from image success case="
                    << source.name;
                expect(captured.images.size() == (source.kind == MglyphSourceKind::image ? 1u : 0u))
                    << "Scene Image primitive count matches Source branch case=" << source.name;
                expect(captured.measurements_match)
                    << "legacy/MML error measurements use actual Source requests and values case="
                    << source.name;
                if (source.mglyph_bbox && captured.mglyph_boxes.size() == 1) {
                    const auto &bbox = *source.mglyph_bbox;
                    const auto &actual = captured.mglyph_boxes[0];
                    expect(bool(close(actual.w, bbox.w) && close(actual.h, bbox.h) &&
                                close(actual.d, bbox.d) && close(actual.scale, bbox.scale) &&
                                close(actual.rscale, bbox.rscale) && close(actual.L, bbox.L) &&
                                close(actual.R, bbox.R)))
                        << "CommonMglyph bbox and scale match Source case=" << source.name;
                }
                if (source.kind == MglyphSourceKind::image && captured.images.size() == 1 &&
                    captured.image_layout_sizes.size() == 1) {
                    const auto &image = captured.images[0];
                    expect(bool(close(image.value.size.width, captured.image_layout_sizes[0][0]) &&
                                close(image.value.size.height, captured.image_layout_sizes[0][1])))
                        << "Scene image size is the actual Common layout size case=" << source.name;
                    std::optional<std::size_t> mglyphIndex;
                    for (std::size_t i = 0; i < source.nodes.size(); ++i)
                        if (source.nodes[i].kind == "mglyph") {
                            mglyphIndex = i;
                            break;
                        }
                    expect(mglyphIndex.has_value())
                        << "Source MML provides the actual mglyph node case=" << source.name;
                    const auto src =
                        mglyphIndex ? sourceAttribute(source, *mglyphIndex, u"src") : std::nullopt;
                    const auto alt =
                        mglyphIndex ? sourceAttribute(source, *mglyphIndex, u"alt") : std::nullopt;
                    expect(bool(src && image.value.href == folia::math::utf16::encode(*src)))
                        << "Scene image href retains authored data URI case=" << source.name;
                    expect(bool(image.value.alt == (alt ? folia::math::utf16::encode(*alt) : "")))
                        << "Scene image alt preserves absent/empty/authored value case="
                        << source.name;
                    expect(image.parent_source_tag == "mglyph")
                        << "Scene image is owned by the standard mglyph content group case="
                        << source.name;
                    const auto operations = image.value.transform.operations();
                    expect(operations.size() == 2_u)
                        << "Scene image retains Source translation and y-flip operations case="
                        << source.name;
                    if (operations.size() == 2) {
                        const auto *translation = std::get_if<scene::Translation>(&operations[0]);
                        const auto *flip = std::get_if<scene::Affine>(&operations[1]);
                        expect(bool(translation && close(translation->x, 0)))
                            << "Scene image translation x is zero case=" << source.name;
                        expect(bool(translation &&
                                    close(translation->y, captured.image_layout_sizes[0][1] +
                                                              captured.image_layout_sizes[0][2])))
                            << "Scene image translation uses Source height plus valign case="
                            << source.name;
                        expect(bool(flip && *flip == scene::Affine{1, 0, 0, -1, 0, 0}))
                            << "Scene image has Source y-axis flip case=" << source.name;
                    }
                }
                if (source.kind == MglyphSourceKind::legacy) {
                    expect(captured.images.empty())
                        << "legacy index branch creates no Image case=" << source.name;
                    const bool hasLegacyText =
                        captured.legacy_texts.size() == 1 && captured.legacy_texts[0].has_value();
                    expect(bool(hasLegacyText))
                        << "legacy index TextNode remains owned outside source children case="
                        << source.name;
                    if (captured.legacy_texts.size() == 1) {
                        if (captured.legacy_texts[0].has_value() && !source.measurements.empty()) {
                            expect(*captured.legacy_texts[0] == source.measurements[0].text)
                                << "legacy Unicode codepoint matches Source measurement text case="
                                << source.name;
                        }
                    }
                }
                const auto after = serialize(captured.value, bundle, source);
                const auto expectedSvg = source_svg_paint::materialize(source.svg);
                if (captured.before_retirement != expectedSvg)
                    reportDifference(source.name, expectedSvg, captured.before_retirement);
                expect(bool(captured.before_retirement == expectedSvg))
                    << "Source SVG with explicit CSS byte match case=" << source.name;
                expect(bool(after == expectedSvg))
                    << "owned Scene serializes after MML/job/measurement retirement case="
                    << source.name;
                std::cout << source.name << ": "
                          << (captured.before_retirement == expectedSvg ? "SVG byte-equal"
                                                                       : "SVG mismatch")
                          << '\n';
            }
        };
};
