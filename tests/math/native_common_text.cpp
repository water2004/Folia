#include "support/folia_test.hpp"
import folia.math.fonts.newcm_font_module_provider;
import folia.math.core.mml_tree.mml_document;
import folia.math.output.common;
import folia.math.output.svg.wrapper_factory;
import folia.math.output.svg.writer;

using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

namespace {
struct TestMeasurement final : common::MeasurementTarget {
    std::size_t unknownCalls{};
    std::string explicitFontFamily;

    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        return {};
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &request) override {
        ++unknownCalls;
        if (request.variant == "-explicitFont") {
            explicitFontFamily = request.font.family;
            return {0.6, 0.75, 0.2};
        }
        return {0.4 * static_cast<double>(request.text.size()), 0.6, 0.1};
    }
};

class FixtureLinebreaks final : public common::Linebreaks {
  public:
    using common::Linebreaks::Linebreaks;
    std::size_t breakRequests{};

    void breakToWidth(common::CommonWrapper &, double) override {
        ++breakRequests;
        throw std::logic_error("text fixture unexpectedly requested line breaking");
    }
};

std::unique_ptr<common::Linebreaks> makeVisitor(common::CommonWrapperFactory &factory) {
    return std::make_unique<FixtureLinebreaks>(factory);
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

folia::math::BBox expectedBox(double w, double h, double d, double scale = 1, double skew = 0) {
    folia::math::BBox result;
    result.w = w;
    result.h = h;
    result.d = d;
    result.scale = scale;
    result.sk = skew;
    return result;
}

common::CommonWrapperCatalog catalog() {
    using K = mml::NodeKind;
    return {registration<common::CommonWrapper>(K::math),
            registration<common::CommonWrapper>(K::mrow),
            registration<common::CommonWrapper>(K::inferred_mrow),
            registration<common::CommonWrapper>(K::mstyle),
            registration<common::CommonWrapper>(K::merror),
            registration<common::CommonMi>(K::mi),
            registration<common::CommonMn>(K::mn),
            registration<common::CommonMs>(K::ms),
            registration<common::CommonMspace>(K::mspace),
            registration<common::CommonMglyph>(K::mglyph),
            registration<common::CommonMtext>(K::mtext),
            registration<common::CommonTextNode>(K::text)};
}

folia::math::BBox const &buildAndGet(mml::MmlDocument &document, mml::MmlNode &expression,
                                     common::LayoutJob &job) {
    const std::array<mml::MmlNode *, 1> children{&expression};
    auto &math = document.factory().make(mml::NodeKind::math, {}, children);
    document.setRoot(math);
    math.setInheritedAttributes();
    math.setTeXclass(nullptr);
    (void)job.build().getBBox();
    return job.jax().wrapperFor(expression)->getBBox();
}
} // namespace

suite common_text_native = [] {
    "common_glyph_stroke_is_frozen_in_ems_and_explicit_in_both_svg_cache_modes"_test = [] {
        struct NoTextMeasurement final : common::MeasurementTarget {
            common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
                throw std::logic_error("glyph paint test supplies explicit metrics");
            }
            common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
                throw std::logic_error("glyph paint test requires a real NewCM outline");
            }
        };
        struct StrokeCase {
            std::optional<double> configured;
            double expected_em;
            std::string_view expected_svg;
        };
        // MathJax SVG.OPTIONS.blacker=3 and SvgTextNode.addStyles are the
        // independent source of the default expectation, not inherited root paint.
        const std::array cases{StrokeCase{{}, .003, "3"}, StrokeCase{0.0, 0.0, "0"},
                               StrokeCase{.007, .007, "7"}};
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        std::optional<scene::Metrics> default_metrics;
        for (auto const &source : cases) {
            const auto retained = [&] {
                NoTextMeasurement measurement;
                mml::MmlDocument document;
                auto &text = document.factory().make(mml::NodeKind::text);
                text.setText(u"x");
                mml::AttributeSet attributes;
                attributes.set(u"style", u"stroke-width: 100;");
                const std::array<mml::MmlNode *, 1> tokenChildren{&text};
                auto &token = document.factory().make(mml::NodeKind::mi, std::move(attributes),
                                                       tokenChildren);
                const std::array<mml::MmlNode *, 1> mathChildren{&token};
                auto &math = document.factory().make(mml::NodeKind::math, {}, mathChildren);
                document.setRoot(math);
                math.setInheritedAttributes();
                common::CommonOptions options;
                if (source.configured)
                    options.glyphStrokeWidth = *source.configured;
                common::CommonOutputJaxConfig config{
                    {16, 8, 1280, 1, "MathJaxNewcm", false}, options, bundle.fonts, "MathJaxNewcm"};
                common::LayoutJob job(document, config, measurement,
                                       svg::SvgWrapperFactory::defaultNodes(), makeVisitor);
                return job.buildScene();
            }();

            // Emission owners have retired; paint and the outline remain Scene values.
            std::size_t glyphs = 0;
            const auto inspect = [&](auto const &self, scene::Group const &group) -> void {
                for (auto const &entry : group.entries) {
                    if (auto const *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
                        self(self, **child);
                    } else if (auto const *primitive = std::get_if<scene::Primitive>(&entry)) {
                        if (auto const *run = std::get_if<scene::GlyphRun>(primitive)) {
                            for (auto const &glyph : run->glyphs) {
                                ++glyphs;
                                expect(glyph.resource != nullptr);
                                expect(glyph.paint.stroke_width == source.expected_em);
                                expect(std::holds_alternative<scene::InheritedPaint>(glyph.paint.fill));
                                expect(std::holds_alternative<scene::InheritedPaint>(glyph.paint.stroke));
                            }
                        }
                        expect(!std::holds_alternative<scene::TextRun>(*primitive));
                    }
                }
            };
            inspect(inspect, retained.root());
            expect(glyphs == 1_u);
            if (!default_metrics)
                default_metrics = retained.metrics();
            expect(retained.metrics() == *default_metrics)
                << "glyph paint must not change mathematical metrics";

            for (const auto mode : {svg::FontCacheMode::none, svg::FontCacheMode::local}) {
                svg::SvgWriteOptions options{.font_cache = mode,
                                              .font_resources = &bundle.svg_resources,
                                              .local_cache_prefixes = {"1-"}};
                const auto output = svg::write(retained, options);
                const auto start = output.find(mode == svg::FontCacheMode::none
                                                   ? "<path data-c=" : "<use data-c=");
                expect(start != std::string::npos);
                if (start == std::string::npos)
                    continue;
                const auto tag = std::string_view(output).substr(start, output.find('>', start) - start);
                const auto width = " stroke-width=\"" + std::string(source.expected_svg) + "\"";
                expect(tag.find(width) != std::string_view::npos)
                    << "glyph width, including zero, overrides the parent stroke width";
                expect(output.find("style=\"stroke-width: 100;\"") != std::string::npos);
                expect(output.find("<g stroke=\"currentColor\" fill=\"currentColor\" "
                                   "stroke-width=\"0\"") != std::string::npos);
            }
        }
    };

    "text_families_preserve_pinned_glyph_metrics_scale_remaps_quotes_and_sizes"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        TestMeasurement measurement;
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", false}, {}, bundle.fonts, "MathJaxNewcm"};

        auto check = [&](mml::NodeKind kind, std::u16string text, folia::math::BBox expected) {
            mml::MmlDocument document;
            auto &token = document.factory().make(mml::NodeKind::text);
            token.setText(std::move(text));
            mml::MmlNode *children[]{&token};
            auto &expression = document.factory().make(kind, {}, children);
            common::LayoutJob job(document, config, measurement, catalog(), makeVisitor);
            const auto &box = buildAndGet(document, expression, job);
            expect(eq(box.w, expected.w)) << "token width";
            expect(eq(box.h, expected.h)) << "token height";
            expect(eq(box.d, expected.d)) << "token depth";
            expect(eq(box.scale, expected.scale)) << "token scale";
            expect(eq(box.sk, expected.sk)) << "token skew";
            expect(eq(box.ic, expected.ic)) << "token italic correction";
            expect(eq(box.oc, expected.oc)) << "token alternate correction";
            expect(eq(box.dx, expected.dx)) << "token offset";
        };

        check(mml::NodeKind::mi, u"𝑨", expectedBox(.869, .711, 0, 1, .173));
        check(mml::NodeKind::mn, u"-2", expectedBox(1.278, .666, .083));
        check(mml::NodeKind::ms, u"hello", expectedBox(3, .694, .011));

        mml::MmlDocument nestedQuoteDocument;
        auto &nestedText = nestedQuoteDocument.factory().make(mml::NodeKind::text);
        nestedText.setText(u"hello");
        mml::MmlNode *nestedTextChildren[]{&nestedText};
        auto &nestedMtext =
            nestedQuoteDocument.factory().make(mml::NodeKind::mtext, {}, nestedTextChildren);
        mml::MmlNode *styleChildren[]{&nestedMtext};
        auto &nestedStyle =
            nestedQuoteDocument.factory().make(mml::NodeKind::mstyle, {}, styleChildren);
        mml::MmlNode *errorChildren[]{&nestedStyle};
        auto &error = nestedQuoteDocument.factory().make(mml::NodeKind::merror, {}, errorChildren);
        auto nestedConfig = config;
        nestedConfig.options.mtextFont.clear();
        common::LayoutJob nestedQuoteJob(nestedQuoteDocument, nestedConfig, measurement, catalog(),
                                         makeVisitor);
        (void)buildAndGet(nestedQuoteDocument, error, nestedQuoteJob);
        auto *nestedWrapper =
            dynamic_cast<common::CommonMtext *>(nestedQuoteJob.jax().wrapperFor(nestedMtext));
        expect(nestedWrapper != nullptr);
        if (nestedWrapper)
            expect(nestedWrapper->variant == "-explicitFont");

        mml::MmlDocument scaleDocument;
        auto &scaleToken = scaleDocument.factory().make(mml::NodeKind::text);
        scaleToken.setText(u"x");
        mml::MmlNode *tokenChildren[]{&scaleToken};
        auto &mi = scaleDocument.factory().make(mml::NodeKind::mi, {}, tokenChildren);
        mml::AttributeSet styleAttributes;
        styleAttributes.set(u"mathsize", std::u16string(u"150%"));
        mml::MmlNode *scaleChildren[]{&mi};
        auto &style = scaleDocument.factory().make(mml::NodeKind::mstyle,
                                                   std::move(styleAttributes), scaleChildren);
        common::LayoutJob scaleJob(scaleDocument, config, measurement, catalog(), makeVisitor);
        (void)buildAndGet(scaleDocument, style, scaleJob);
        auto *miWrapper = scaleJob.jax().wrapperFor(mi);
        expect(miWrapper != nullptr);
        if (miWrapper) {
            expect(eq(miWrapper->getBBox().scale, 1.5)) << "inherited math size";
            expect(eq(miWrapper->getBBox().w, .572)) << "scaled mi intrinsic width";
        }

        mml::MmlDocument explicitDocument;
        auto &explicitToken = explicitDocument.factory().make(mml::NodeKind::text);
        explicitToken.setText(u"x");
        mml::MmlNode *explicitTokenChildren[]{&explicitToken};
        mml::AttributeSet fontAttributes;
        fontAttributes.set(u"fontfamily", std::u16string(u"Arial"));
        auto &explicitMi = explicitDocument.factory().make(
            mml::NodeKind::mi, std::move(fontAttributes), explicitTokenChildren);
        common::LayoutJob explicitJob(explicitDocument, config, measurement, catalog(),
                                      makeVisitor);
        (void)buildAndGet(explicitDocument, explicitMi, explicitJob);
        auto *explicitWrapper = explicitJob.jax().wrapperFor(explicitMi);
        expect(explicitWrapper != nullptr);
        expect(measurement.explicitFontFamily == "Arial")
            << "measured family: " << measurement.explicitFontFamily;
        if (explicitWrapper) {
            expect(explicitWrapper->variant == "-explicitFont");
            expect(explicitWrapper->getBBox().w == .6);
            expect(explicitWrapper->getBBox().h == .75);
            expect(explicitWrapper->getBBox().d == .2);
        }

        mml::MmlDocument unknownDocument;
        auto &unknownToken = unknownDocument.factory().make(mml::NodeKind::text);
        unknownToken.setText(u"\U0010FFFF");
        mml::MmlNode *unknownChildren[]{&unknownToken};
        auto &unknownMi = unknownDocument.factory().make(mml::NodeKind::mi, {}, unknownChildren);
        common::LayoutJob unknownJob(unknownDocument, config, measurement, catalog(), makeVisitor);
        (void)buildAndGet(unknownDocument, unknownMi, unknownJob);
        expect(measurement.unknownCalls > 0_u);
        auto *unknownWrapper = unknownJob.jax().wrapperFor(unknownMi);
        expect(unknownWrapper != nullptr);
        if (unknownWrapper)
            expect(unknownWrapper->getBBox().w == .8);

        mml::MmlDocument document;
        mml::AttributeSet spaceAttributes;
        spaceAttributes.set(u"width", std::u16string(u"1em"));
        spaceAttributes.set(u"height", std::u16string(u".5em"));
        spaceAttributes.set(u"depth", std::u16string(u".2em"));
        auto &space = document.factory().make(mml::NodeKind::mspace, std::move(spaceAttributes));
        common::LayoutJob spaceJob(document, config, measurement, catalog(), makeVisitor);
        const auto &spaceBox = buildAndGet(document, space, spaceJob);
        expect(spaceBox.w == 1.0);
        expect(spaceBox.h == .5);
        expect(spaceBox.d == .2);

        mml::MmlDocument glyphDocument;
        mml::AttributeSet glyphAttributes;
        glyphAttributes.set(u"index", std::u16string(u"65"));
        auto &glyph =
            glyphDocument.factory().make(mml::NodeKind::mglyph, std::move(glyphAttributes));
        common::LayoutJob glyphJob(glyphDocument, config, measurement, catalog(), makeVisitor);
        const auto &glyphBox = buildAndGet(glyphDocument, glyph, glyphJob);
        expect(glyphBox.w == .75);
        expect(glyphBox.h == .716);
        expect(glyphBox.d == 0.0);
    };
};
