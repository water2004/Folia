#include "support/folia_test.hpp"

import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.math;
import folia.math.core.mml_tree.mml_nodes.mglyph;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.scene;
import folia.math.output.svg.mglyph;
import folia.math.output.svg.wrapper_factory;

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace mml = folia::math::mml;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

namespace {
class NoMeasurements final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("mglyph fixture supplies metrics");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("mglyph fixture uses known geometry");
    }
};

class IndexMeasurements final : public common::MeasurementTarget {
  public:
    std::size_t calls{};
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("mglyph fixture supplies metrics");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &request) override {
        if (request.text != u"A" || request.variant != "-explicitFont" ||
            request.font.family != "MathJaxNewcm" || request.font.italic || request.font.bold)
            throw std::logic_error("unexpected mglyph index measurement request");
        ++calls;
        // Pinned Source LiteAdaptor measurement for this explicit-font character.
        return {.6, .75, .2};
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreaks(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

common::CommonOutputJaxConfig config(common::FontRepository const &fonts) {
    common::ExtendedMetrics metrics{16, 8, 640, 1, "MathJaxNewcm", false};
    return {metrics, {}, fonts, "MathJaxNewcm"};
}

scene::Group const *findMglyph(scene::Group const &group) {
    if (group.metadata.source_tag == "mglyph")
        return &group;
    for (const auto &entry : group.entries) {
        if (const auto *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child) {
                if (auto *found = findMglyph(**child))
                    return found;
            }
        }
    }
    return nullptr;
}

void collectImages(scene::Group const &group, std::vector<scene::Image const *> &images) {
    for (const auto &entry : group.entries) {
        if (const auto *primitive = std::get_if<scene::Primitive>(&entry)) {
            if (const auto *image = std::get_if<scene::Image>(primitive))
                images.push_back(image);
        } else if (const auto *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child)
                collectImages(**child, images);
        }
    }
}
} // namespace

suite native_svg_mglyph = [] {
    "SVG catalog installs SvgMglyph with the shared Common metadata"_test = [] {
        const auto catalog = svg::SvgWrapperFactory::defaultNodes();
        const auto found = std::ranges::find(catalog, mml::NodeKind::mglyph,
                                             &common::CommonWrapperRegistration::kind);
        expect(found != catalog.end());
        if (found == catalog.end())
            return;
        expect(found->data.styles.empty());
        expect(static_cast<bool>(found->creator.construct));
        expect(static_cast<bool>(found->creator.isInstance));
    };

    "SVG mglyph image emits owned values with Source size and transform"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        mml::MmlDocument document;
        mml::AttributeSet attributes;
        attributes.set(u"src", u"data:image/svg+xml,<svg id=\"x&y\">");
        attributes.set(u"alt", u"a<&\U0001F642");
        attributes.set(u"width", u"2em");
        attributes.set(u"height", u"1.5em");
        attributes.set(u"valign", u".25em");
        auto &glyph = document.factory().make(mml::NodeKind::mglyph, std::move(attributes));
        mml::AttributeSet emptyAltAttributes;
        emptyAltAttributes.set(u"src", u"data:image/svg+xml,empty-alt");
        auto &emptyAltGlyph =
            document.factory().make(mml::NodeKind::mglyph, std::move(emptyAltAttributes));
        const std::array<mml::MmlNode *, 2> children{&glyph, &emptyAltGlyph};
        auto &math = document.factory().make(mml::NodeKind::math, {}, children);
        document.setRoot(math);
        math.setInheritedAttributes();
        math.setTeXclass(nullptr);

        common::LayoutJob job(document, config(bundle.fonts), measurement,
                              svg::SvgWrapperFactory::defaultNodes(), &makeLinebreaks);
        const auto result = job.buildScene();
        const auto *group = findMglyph(result.root());
        expect(group != nullptr);
        if (!group)
            return;
        expect(group->metadata.source_tag == "mglyph");

        std::vector<scene::Image const *> images;
        collectImages(result.root(), images);
        expect(images.size() == 2u);
        if (images.size() < 2u)
            return;
        const auto *image = images.front();
        expect((image->size == scene::Size{2, 1.5}));
        expect(image->href == "data:image/svg+xml,<svg id=\"x&y\">");
        expect(image->alt == "a<&\xF0\x9F\x99\x82");
        expect(images.back()->alt.empty());
        const std::array<scene::TransformOperation, 2> expected{
            scene::Translation{0, 1.75, scene::TranslationSeparator::space},
            scene::Affine{1, 0, 0, -1, 0, 0},
        };
        expect(std::ranges::equal(image->transform.operations(), expected));
    };

    "SVG mglyph image uses only the first standard content root"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        mml::MmlDocument document;
        mml::AttributeSet attributes;
        attributes.set(u"src", u"data:image/svg+xml,first-root");
        auto &glyph = document.factory().make(mml::NodeKind::mglyph, std::move(attributes));
        const std::array<mml::MmlNode *, 1> children{&glyph};
        auto &math = document.factory().make(mml::NodeKind::math, {}, children);
        document.setRoot(math);
        math.setInheritedAttributes();
        common::LayoutJob job(document, config(bundle.fonts), measurement,
                              svg::SvgWrapperFactory::defaultNodes(), &makeLinebreaks);
        (void)job.build();
        auto *wrapper = dynamic_cast<svg::SvgMglyph *>(job.jax().wrapperFor(glyph));
        expect(wrapper != nullptr);
        if (!wrapper)
            return;
        scene::Builder builder;
        const std::array parents{builder.add_group(), builder.add_group()};
        const auto output = wrapper->emitScene(builder, parents);
        expect(output.size() == 2u);
        const auto value = std::move(builder).freeze();
        expect(value.root().entries.size() == 2u);
        if (value.root().entries.size() != 2)
            return;
        for (std::size_t i = 0; i < 2; ++i) {
            const auto *parent =
                std::get_if<std::shared_ptr<scene::Group const>>(&value.root().entries[i]);
            expect(parent != nullptr);
            if (!parent || !*parent)
                continue;
            std::vector<scene::Image const *> images;
            collectImages(**parent, images);
            expect(images.size() == (i == 0 ? 1u : 0u));
        }
    };

    "SVG mglyph index retains the Common character wrapper owned by the factory"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        IndexMeasurements measurement;
        mml::MmlDocument document;
        mml::AttributeSet attributes;
        attributes.set(u"fontfamily", u"MathJaxNewcm");
        attributes.set(u"index", u"65");
        auto &glyph = document.factory().make(mml::NodeKind::mglyph, std::move(attributes));
        const std::array<mml::MmlNode *, 1> children{&glyph};
        auto &math = document.factory().make(mml::NodeKind::math, {}, children);
        document.setRoot(math);
        math.setInheritedAttributes();
        math.setTeXclass(nullptr);

        common::LayoutJob job(document, config(bundle.fonts), measurement,
                              svg::SvgWrapperFactory::defaultNodes(), &makeLinebreaks);
        const auto result = job.buildScene();
        auto *base = job.jax().wrapperFor(glyph);
        expect(base != nullptr);
        if (!base)
            return;
        auto *wrapper = dynamic_cast<svg::SvgMglyph *>(base);
        expect(wrapper != nullptr);
        if (!wrapper)
            return;
        auto *character = wrapper->charWrapper();
        expect(character != nullptr);
        if (!character)
            return;
        expect(character->parent == wrapper);
        expect(job.factory().wrapperCount() >= 3u)
            << "the index character remains in the request factory's wrapper arena";
        expect(findMglyph(result.root()) != nullptr);
        expect(measurement.calls > 0u);
    };
};
