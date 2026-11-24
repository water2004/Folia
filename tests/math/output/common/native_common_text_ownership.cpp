#include "support/folia_test.hpp"
import folia.math.fonts.newcm_font_module_provider;
import folia.math.core.mml_tree.mml_document;
import folia.math.output.common;

using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;

namespace {
struct Measurement final : common::MeasurementTarget {
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        return {};
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        return {};
    }
};

class NoBreaks final : public common::Linebreaks {
  public:
    using common::Linebreaks::Linebreaks;

    void breakToWidth(common::CommonWrapper &, double) override {
        throw std::logic_error("ownership fixture unexpectedly requested line breaking");
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreaks(common::CommonWrapperFactory &factory) {
    return std::make_unique<NoBreaks>(factory);
}

template <class Wrapper> common::CommonWrapperRegistration registration(mml::NodeKind kind) {
    using Args = common::WrapperBuildArguments;
    using Creator = folia::math::tree::FactoryClass<common::CommonWrapper, Args>;
    return {kind,
            Creator{[](Args const &args) -> std::unique_ptr<common::CommonWrapper> {
                        return std::make_unique<Wrapper>(args.factory, args.node, args.parent);
                    },
                    [](common::CommonWrapper const &value) {
                        return dynamic_cast<Wrapper const *>(&value) != nullptr;
                    }},
            common::CommonWrapperClassData::commonDefaults()};
}

common::CommonWrapperCatalog catalog() {
    using K = mml::NodeKind;
    return {registration<common::CommonWrapper>(K::math),
            registration<common::CommonInferredMrow>(K::inferred_mrow),
            registration<common::CommonMs>(K::ms), registration<common::CommonMglyph>(K::mglyph),
            registration<common::CommonTextNode>(K::text)};
}

} // namespace

suite common_text_ownership_native = [] {
    "synthetic_ms_quotes_are_ordered_indexed_and_factory_owned"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        Measurement measurement;
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", false}, {}, bundle.fonts, "MathJaxNewcm"};
        mml::MmlDocument document;
        auto &text = document.factory().make(mml::NodeKind::text);
        text.setText(u"hello");
        mml::MmlNode *textChildren[]{&text};
        auto &ms = document.factory().make(mml::NodeKind::ms, {}, textChildren);
        auto &math = document.factory().make(mml::NodeKind::math);
        document.append(math, ms);
        document.setRoot(math);

        common::LayoutJob job(document, config, measurement, catalog(), makeLinebreaks);
        (void)job.build();
        auto *wrapper = dynamic_cast<common::CommonMs *>(job.jax().wrapperFor(ms));
        expect(wrapper != nullptr);
        if (wrapper) {
            expect(wrapper->childNodes.size() == 3_u);
            if (wrapper->childNodes.size() == 3) {
                expect(dynamic_cast<mml::TextNode &>(*wrapper->childNodes[0]->node)
                           .getText()
                           .compare(u"\u201c") == 0);
                expect(dynamic_cast<mml::TextNode &>(*wrapper->childNodes[1]->node)
                           .getText()
                           .compare(u"hello") == 0);
                expect(dynamic_cast<mml::TextNode &>(*wrapper->childNodes[2]->node)
                           .getText()
                           .compare(u"\u201d") == 0);
                for (auto *child : wrapper->childNodes)
                    expect(job.jax().wrapperFor(*child->node) == child);
            }
        }
        expect(job.factory().wrapperCount() == 6_u);
    };

    "mglyph_character_wrapper_is_indexed_but_remains_outside_children"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        Measurement measurement;
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", false}, {}, bundle.fonts, "MathJaxNewcm"};
        mml::MmlDocument document;
        mml::AttributeSet attributes;
        attributes.set(u"index", std::u16string(u"128512"));
        auto &glyph = document.factory().make(mml::NodeKind::mglyph, std::move(attributes));
        auto &math = document.factory().make(mml::NodeKind::math);
        document.append(math, glyph);
        document.setRoot(math);

        common::LayoutJob job(document, config, measurement, catalog(), makeLinebreaks);
        (void)job.build();
        auto *wrapper = dynamic_cast<common::CommonMglyph *>(job.jax().wrapperFor(glyph));
        expect(wrapper != nullptr);
        if (wrapper) {
            expect(wrapper->childNodes.empty());
            expect(wrapper->charWrapper() != nullptr);
            if (wrapper->charWrapper()) {
                auto *character = wrapper->charWrapper();
                expect(dynamic_cast<mml::TextNode &>(*character->node)
                           .getText()
                           .compare(u"\U0001F600") == 0);
                expect(character->parent == wrapper);
                expect(job.jax().wrapperFor(*character->node) == character);
            }
        }
        expect(job.factory().wrapperCount() == 4_u);
    };
};
