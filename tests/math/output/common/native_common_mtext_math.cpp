#include "support/folia_test.hpp"
import folia.math.fonts.newcm_font_module_provider;
import folia.math.core.mml_tree.mml_document;
import folia.math.output.common;

using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;

namespace {
std::size_t breakCalls{};
double breakWidth{};

struct Measurement final : common::MeasurementTarget {
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        return {};
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &request) override {
        if (request.variant == "-explicitFont")
            return {.6 * static_cast<double>(request.text.size()), .75, .2};
        return {.4 * static_cast<double>(request.text.size()), .6, .1};
    }
};

struct BreakProbe final : common::Linebreaks {
    using common::Linebreaks::Linebreaks;
    std::size_t calls{};
    double width{};

    void breakToWidth(common::CommonWrapper &, double requested) override {
        ++calls;
        width = requested;
        breakCalls = calls;
        breakWidth = requested;
    }
};

std::unique_ptr<common::Linebreaks> makeBreakProbe(common::CommonWrapperFactory &factory) {
    auto probe = std::make_unique<BreakProbe>(factory);
    // Retain only observations; LayoutJob owns the service for this scope.
    breakCalls = 0;
    breakWidth = 0;
    return probe;
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
    return {registration<common::CommonMath>(K::math),
            registration<common::CommonInferredMrow>(K::inferred_mrow),
            registration<common::CommonMrow>(K::mrow), registration<common::CommonMtext>(K::mtext),
            registration<common::CommonTextNode>(K::text)};
}
} // namespace

suite common_mtext_math_native = [] {
    "mtext_break_widths_lines_and_display_math_overflow_keep_source_roles"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        Measurement measurement;
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", true}, {}, bundle.fonts, "MathJaxNewcm"};
        config.options.mtextFont = "Arial";

        mml::MmlDocument document;
        auto &text = document.factory().make(mml::NodeKind::text);
        text.setText(u"alpha beta gamma delta epsilon");
        mml::MmlNode *textChildren[]{&text};
        auto &mtext = document.factory().make(mml::NodeKind::mtext, {}, textChildren);
        mml::MmlNode *rowChildren[]{&mtext};
        auto &row = document.factory().make(mml::NodeKind::mrow, {}, rowChildren);
        mml::AttributeSet mathAttributes;
        mathAttributes.set(u"overflow", std::u16string(u"linebreak"));
        mathAttributes.set(u"maxwidth", std::u16string(u"4em"));
        mml::MmlNode *mathChildren[]{&row};
        auto &math =
            document.factory().make(mml::NodeKind::math, std::move(mathAttributes), mathChildren);
        document.setRoot(math);

        common::LayoutJob job(document, config, measurement, catalog(), makeBreakProbe);
        auto &root = job.build();
        auto *textWrapper = dynamic_cast<common::CommonMtext *>(job.jax().wrapperFor(mtext));
        expect(textWrapper != nullptr);
        if (!textWrapper)
            return;
        expect(textWrapper->variant == "-explicitFont");
        expect(textWrapper->getBBox().w == 18.0);

        textWrapper->setBreakAt(std::array<std::size_t, 2>{0, 1});
        textWrapper->setBreakAt(std::array<std::size_t, 2>{0, 2});
        textWrapper->setBreakAt(std::array<std::size_t, 2>{0, 3});
        textWrapper->setBreakAt(std::array<std::size_t, 2>{0, 4});
        const std::array<double, 5> expectedWidths{3, 2.4, 3, 3, 4.2};
        for (std::size_t i = 0; i < expectedWidths.size(); ++i) {
            const auto line = textWrapper->getLineBBox(i);
            expect(line->w == expectedWidths[i]);
            expect(line->h == .75);
            expect(line->d == .2);
        }

        (void)root.getBBox();
        expect(breakCalls == 1_u);
        expect(breakWidth == 4.0);
        expect(job.jax().containerWidth() == 4.0);
        expect(job.jax().currentMetrics().display);
    };
};
