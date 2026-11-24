#include "support/folia_test.hpp"
import folia.math.output.common;

namespace {
using namespace folia::math;
using namespace folia::math::output::common;
using namespace boost::ut;
namespace scene = folia::math::output::scene;

struct TestMeasurementTarget final : MeasurementTarget {
    ExtendedMetrics measureMetrics(MetricRequest const &request) override {
        return {.em = 16,
                .ex = 8,
                .containerWidth = 1280,
                .scale = 1,
                .family = request.family,
                .display = request.display};
    }

    UnknownBBox measureUnknownText(UnknownTextRequest const &) override {
        return {};
    }
};

template <class Wrapper> tree::FactoryClass<CommonWrapper, WrapperBuildArguments> wrapperClass() {
    return {[](WrapperBuildArguments const &args) -> std::unique_ptr<CommonWrapper> {
                return std::make_unique<Wrapper>(args.factory, args.node, args.parent);
            },
            [](CommonWrapper const &wrapper) {
                return dynamic_cast<Wrapper const *>(&wrapper) != nullptr;
            }};
}

class UnexpectedBreaks final : public Linebreaks {
  public:
    using Linebreaks::Linebreaks;

    void breakToWidth(CommonWrapper &, double) override {
        throw std::logic_error("unexpected linebreak request in wrapper unit test");
    }
};

CommonWrapperCatalog testCatalog() {
    CommonWrapperCatalog catalog;
    const auto add = [&](mml::NodeKind kind, auto creator) {
        catalog.push_back({kind, std::move(creator), CommonWrapperClassData::commonDefaults()});
    };
    add(mml::NodeKind::text, wrapperClass<CommonTextNode>());
    add(mml::NodeKind::mi, wrapperClass<CommonMi>());
    add(mml::NodeKind::mn, wrapperClass<CommonMn>());
    add(mml::NodeKind::mo, wrapperClass<CommonMo>());
    add(mml::NodeKind::mrow, wrapperClass<CommonMrow>());
    add(mml::NodeKind::inferred_mrow, wrapperClass<CommonInferredMrow>());
    add(mml::NodeKind::math, wrapperClass<CommonMath>());
    return catalog;
}

FontRepository const &testFonts() {
    static const FontRepository repository = [] {
        FontRepositoryBuilder builder;
        FontFaceData face;
        face.name = "MathJaxNewcm";
        for (const auto *name : {"normal", "italic", "bold", "bold-italic"}) {
            FontVariant variant;
            variant.name = name;
            variant.cache_id = name;
            face.variants.emplace(name, std::move(variant));
        }
        builder.addFace(std::move(face));
        builder.addParameter("MathJaxNewcm", "x_height", .442);
        builder.addParameter("MathJaxNewcm", "rule_thickness", .04);
        return std::move(builder).freeze();
    }();
    return repository;
}

mml::MmlNode &token(mml::MmlDocument &document, mml::NodeKind kind, std::u16string value) {
    auto &text = dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
    text.setText(std::move(value));
    std::array<mml::MmlNode *, 1> children{&text};
    return document.factory().make(kind, {}, children);
}

CommonOutputJaxConfig wrapperConfig(CommonOptions options = {},
                                    InlineBreakPolicy policy = InlineBreakPolicy::opportunistic) {
    ExtendedMetrics metrics{.em = 16,
                            .ex = 8,
                            .containerWidth = 1280,
                            .scale = 1,
                            .family = "MathJaxNewcm",
                            .display = false};
    return {metrics, std::move(options), testFonts(), "MathJaxNewcm", policy};
}

std::unique_ptr<Linebreaks> makeUnexpectedBreaks(CommonWrapperFactory &factory) {
    return std::make_unique<UnexpectedBreaks>(factory);
}

CommonOutputJax makeJax(mml::MmlDocument &document, TestMeasurementTarget &target,
                        CommonOptions options = {}) {
    return CommonOutputJax(document, wrapperConfig(std::move(options)), target, testCatalog(),
                           &makeUnexpectedBreaks);
}

struct WrapperProbe final : CommonWrapper {
    using CommonWrapper::CommonWrapper;

    std::shared_ptr<LineBBox> lineFrom(CommonWrapper &child, std::size_t index) {
        return getChildLineBBox(child, index);
    }


    double convertLength(LengthInput const &value, double size = 1,
                         std::optional<double> scale = {}) const {
        return CommonWrapper::length2em(value, size, scale);
    }
};
} // namespace

suite native_common_wrapper_source_cases = [] {
    "common_link_paint_is_owned_separately_from_authored_child_paint"_test = [] {
        struct LinkCase {
            std::string_view name;
            std::u16string_view mathcolor;
            std::u16string_view style;
        };
        constexpr std::array cases{
            LinkCase{"inherited child", {}, {}},
            LinkCase{"authored mathcolor", u"teal", {}},
            LinkCase{"authored inline style", u"teal", u"fill: rebeccapurple; stroke: green"},
        };
        for (auto const &source : cases) {
            auto retained = [&] {
                // Empty rows exercise link ownership and style without text measurement.
                TestMeasurementTarget target;
                mml::MmlDocument document;
                mml::AttributeSet attributes;
                attributes.set(u"href", u"https://example.test/math-link");
                if (!source.mathcolor.empty())
                    attributes.set(u"mathcolor", std::u16string(source.mathcolor));
                if (!source.style.empty())
                    attributes.set(u"style", std::u16string(source.style));
                auto &row = document.factory().make(mml::NodeKind::mrow, std::move(attributes));
                document.setRoot(row);
                auto jax = makeJax(document, target);
                auto &wrapper = jax.factory().wrap(row);
                scene::Builder builder;
                scene::Paint surrounding;
                surrounding.fill = scene::CssColor{"red"};
                surrounding.stroke = scene::CssColor{"red"};
                builder.set_paint(0, std::move(surrounding));
                const std::array<scene::NodeId, 1> parents{0};
                const auto emitted = wrapper.emitScene(builder, parents);
                expect(emitted.size() == 1_u) << source.name;
                auto value = std::move(builder).freeze();
                row.attributes().set(u"href", u"https://example.test/changed");
                row.attributes().set(u"mathcolor", u"orange");
                return value;
            }();

            // The MML document, wrappers, builder, and measurement owner are now gone.
            expect(retained.root().entries.size() == 1_u) << source.name;
            if (retained.root().entries.size() != 1)
                continue;
            const auto *entry = std::get_if<std::shared_ptr<scene::Group const>>(
                &retained.root().entries.front());
            expect(entry && *entry) << source.name;
            if (!entry || !*entry)
                continue;
            auto const &group = **entry;
            expect(group.link.has_value()) << source.name;
            if (!group.link)
                continue;
            expect(group.link->href == "https://example.test/math-link") << source.name;
            expect(group.link->paint ==
                   scene::PaintColors{scene::CssColor{"blue"}, scene::CssColor{"blue"}})
                << source.name << ": Source link color is independent of surrounding red";
            if (source.mathcolor.empty()) {
                expect(std::holds_alternative<scene::InheritedPaint>(group.paint.fill));
                expect(std::holds_alternative<scene::InheritedPaint>(group.paint.stroke));
            } else {
                expect(group.paint.fill == scene::PaintColor{scene::CssColor{"teal"}});
                expect(group.paint.stroke == scene::PaintColor{scene::CssColor{"teal"}});
            }
            const auto style = std::ranges::find(group.metadata.ordered_attributes, "style",
                                                  &scene::SemanticAttribute::name);
            if (source.style.empty()) {
                expect(style == group.metadata.ordered_attributes.end());
            } else {
                expect(style != group.metadata.ordered_attributes.end());
                if (style != group.metadata.ordered_attributes.end()) {
                    expect(style->value.find("fill: rebeccapurple") != std::string::npos);
                    expect(style->value.find("stroke: green") != std::string::npos);
                }
            }
            expect(group.entries.size() == 1_u) << source.name;
            if (group.entries.size() != 1)
                continue;
            const auto *primitive = std::get_if<scene::Primitive>(&group.entries.front());
            const auto *hitbox = primitive ? std::get_if<scene::Rule>(primitive) : nullptr;
            expect(hitbox != nullptr) << source.name;
            if (hitbox) {
                expect(hitbox->role == scene::ElementRole::link_hitbox);
                expect(std::holds_alternative<scene::NoPaint>(hitbox->paint.fill));
                expect(std::holds_alternative<scene::NoPaint>(hitbox->paint.stroke));
            }
        }
    };

    "common_rows_without_href_do_not_acquire_link_paint_or_hitboxes"_test = [] {
        const scene::Link unstyled{"https://example.test/direct-scene", {}, {}};
        expect(std::holds_alternative<scene::InheritedPaint>(unstyled.paint.fill));
        expect(std::holds_alternative<scene::InheritedPaint>(unstyled.paint.stroke));
        for (const bool emptyHref : {false, true}) {
            TestMeasurementTarget target;
            mml::MmlDocument document;
            mml::AttributeSet attributes;
            if (emptyHref)
                attributes.set(u"href", u"");
            auto &row = document.factory().make(mml::NodeKind::mrow, std::move(attributes));
            document.setRoot(row);
            auto jax = makeJax(document, target);
            auto &wrapper = jax.factory().wrap(row);
            scene::Builder builder;
            const std::array<scene::NodeId, 1> parents{0};
            const auto emitted = wrapper.emitScene(builder, parents);
            expect(emitted.size() == 1_u);
            if (emitted.size() != 1 || !emitted.front())
                continue;
            const auto id = *emitted.front();
            expect(!builder.link(id).has_value());
            expect(builder.entry_count(id) == 0_u);
            expect(std::holds_alternative<scene::InheritedPaint>(builder.paint(id).fill));
            expect(std::holds_alternative<scene::InheritedPaint>(builder.paint(id).stroke));
        }
    };

    "real_common_wrapper_length2em_source_values_and_scale"_test = [] {
        TestMeasurementTarget target;
        mml::MmlDocument document;
        auto jax = makeJax(document, target);
        auto &row = document.factory().make(mml::NodeKind::mrow);
        document.setRoot(row);
        auto &wrapper = dynamic_cast<WrapperProbe &>(
            jax.factory().adopt(std::make_unique<WrapperProbe>(jax.factory(), row)));

        expect(wrapper.convertLength(LengthInput{0.0}) == 0.0);
        const auto negativeZero = wrapper.convertLength(LengthInput{-0.0});
        expect(negativeZero == 0.0);
        expect(wrapper.convertLength(LengthInput{2.0}) == 2.0);
        expect(wrapper.convertLength(LengthInput{2.0}, .5) == 1.0);

        const auto rule = testFonts().face("MathJaxNewcm").parameters.required("rule_thickness");
        expect(wrapper.convertLength(LengthInput{std::u16string{u"medium"}}) == rule);
        expect(wrapper.convertLength(LengthInput{std::u16string{u"thin"}}) == rule * 2.0 / 3.0);
        expect(wrapper.convertLength(LengthInput{std::u16string{u"thick"}}) == rule * 5.0 / 3.0);
        expect(wrapper.convertLength(LengthInput{std::u16string{u"2px"}}) == 2.0 / jax.pxPerEm());
        expect(wrapper.convertLength(LengthInput{std::u16string{u"2px"}}, 1, 2.0) ==
               2.0 / jax.pxPerEm() / 2.0);
        expect(wrapper.convertLength(LengthInput{std::u16string{u"40%"}}, .5) == .2);

        expect(wrapper.convertLength(LengthInput{std::numeric_limits<double>::quiet_NaN()}) == 1.0);
        expect(wrapper.convertLength(LengthInput{std::u16string{}}) == 1.0);
    };
    "style_removal_and_scale_match_pinned_style_case"_test = [] {
        TestMeasurementTarget target;
        mml::MmlDocument document;
        auto jax = makeJax(document, target);
        mml::AttributeSet attrs;
        attrs.set(u"style",
                  u"font-size: 150%; padding: 2px; margin-left: 3px; border: 1px solid red");
        auto &row = document.factory().make(mml::NodeKind::mrow, attrs);
        document.setRoot(row);
        auto &wrapper = jax.factory().wrap(row);
        expect(wrapper.styles.has_value());
        expect(wrapper.styleData.has_value());
        expect(wrapper.removedStyles.at("fontSize") == "150%");
        // Pinned Source case style-variant-scale-and-child-order records 1.5.
        expect(wrapper.bbox.scale == 1.5);
    };

    "tex_spacing_matches_pinned_a_plus_b_case"_test = [] {
        TestMeasurementTarget target;
        mml::MmlDocument document;
        auto &a = token(document, mml::NodeKind::mi, u"a");
        auto &plus = token(document, mml::NodeKind::mo, u"+");
        auto &b = token(document, mml::NodeKind::mi, u"b");
        std::array<mml::MmlNode *, 3> children{&a, &plus, &b};
        auto &math = document.factory().make(mml::NodeKind::math, {}, children);
        math.setInheritedAttributes();
        document.setRoot(math);
        LayoutJob job(document, wrapperConfig({}, InlineBreakPolicy::forced), target, testCatalog(),
                      &makeUnexpectedBreaks);
        auto &root = job.build();
        (void)root.getBBox();
        auto &wrapper = *root.childNodes.front();
        expect(wrapper.childNodes.size() == 3_u);
        expect(wrapper.childNodes[1]->node->kind() == mml::NodeKind::mo);
        // Values are from common-wrapper-native-cases/expected.json, pinned
        // by the accompanying Source/CJS hashes and the source case "a+b".
        expect(wrapper.childNodes[1]->bbox.L == 0.0_d);
        expect(wrapper.childNodes[2]->bbox.L == 2.0 / 9.0);
    };

    "mathml_spacing_matches_pinned_a_plus_b_case"_test = [] {
        TestMeasurementTarget target;
        CommonOptions options;
        options.mathmlSpacing = true;
        mml::MmlDocument document;
        auto &a = token(document, mml::NodeKind::mi, u"a");
        auto &plus = token(document, mml::NodeKind::mo, u"+");
        auto &b = token(document, mml::NodeKind::mi, u"b");
        std::array<mml::MmlNode *, 3> children{&a, &plus, &b};
        auto &math = document.factory().make(mml::NodeKind::math, {}, children);
        math.setInheritedAttributes();
        document.setRoot(math);
        LayoutJob job(document, wrapperConfig(std::move(options), InlineBreakPolicy::forced),
                      target, testCatalog(), &makeUnexpectedBreaks);
        auto &root = job.build();
        (void)root.getBBox();
        auto &wrapper = *root.childNodes.front();
        expect(wrapper.childNodes[1]->bbox.L == 0.0_d);
        expect(wrapper.childNodes[1]->bbox.R == 2.0 / 9.0);
        expect(wrapper.childNodes[2]->bbox.L == 0.0_d);
    };

    "invalid_explicit_mathvariant_records_source_warning_and_falls_back"_test = [] {
        TestMeasurementTarget target;
        mml::MmlDocument document;
        auto jax = makeJax(document, target);

        mml::AttributeSet invalidAttributes;
        invalidAttributes.set(u"mathvariant", u"not-a-source-variant");
        auto &invalidText =
            dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
        invalidText.setText(u"x");
        std::array<mml::MmlNode *, 1> invalidChildren{&invalidText};
        auto &invalid = document.factory().make(mml::NodeKind::mi, std::move(invalidAttributes),
                                                invalidChildren);
        document.setRoot(invalid);

        auto &invalidWrapper = jax.factory().wrap(invalid);
        expect(invalidWrapper.variant == "normal");
        const auto &diagnostics = jax.diagnostics();
        expect(diagnostics.size() == 1_u);
        expect(diagnostics[0].code == CommonDiagnosticCode::unknown_math_variant);
        expect(diagnostics[0].node_kind == mml::NodeKind::mi);
        expect(diagnostics[0].value == "not-a-source-variant");

        mml::AttributeSet validAttributes;
        validAttributes.set(u"mathvariant", u"normal");
        auto &validText =
            dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
        validText.setText(u"y");
        std::array<mml::MmlNode *, 1> validChildren{&validText};
        auto &valid =
            document.factory().make(mml::NodeKind::mi, std::move(validAttributes), validChildren);
        auto &validWrapper = jax.factory().wrap(valid);
        expect(validWrapper.variant == "normal");
        expect(jax.diagnostics().size() == 1_u);

        mml::AttributeSet nullAttributes;
        nullAttributes.set(u"mathvariant", mml::AttributeNull{});
        auto &nullText =
            dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
        nullText.setText(u"z");
        std::array<mml::MmlNode *, 1> nullChildren{&nullText};
        auto &explicitNull =
            document.factory().make(mml::NodeKind::mi, std::move(nullAttributes), nullChildren);
        auto &nullWrapper = jax.factory().wrap(explicitNull);
        expect(nullWrapper.variant == "normal");
        expect(jax.diagnostics().size() == 2_u);
        expect(jax.diagnostics()[1].value == "null");
    };

    "base_probe_preserves_unmodified_child_line_identity"_test = [] {
        TestMeasurementTarget target;
        mml::MmlDocument document;
        auto jax = makeJax(document, target);
        auto &a = token(document, mml::NodeKind::mi, u"a");
        std::array<mml::MmlNode *, 1> children{&a};
        auto &row = document.factory().make(mml::NodeKind::mrow, {}, children);
        document.setRoot(row);
        auto &probe = dynamic_cast<WrapperProbe &>(
            jax.factory().adopt(std::make_unique<WrapperProbe>(jax.factory(), row)));
        auto *child = probe.childNodes.front();
        auto childLine = child->getLineBBox(0);
        auto parentLine = probe.lineFrom(*child, 0);
        // This is a direct base-helper probe. The pinned Source case records
        // the same alias rule when styleData and side spacing are absent.
        expect(parentLine == childLine);
    };

    "bbox_false_returns_cached_bbox_when_already_computed"_test = [] {
        TestMeasurementTarget target;
        mml::MmlDocument document;
        auto jax = makeJax(document, target);
        auto &row = document.factory().make(mml::NodeKind::mrow);
        document.setRoot(row);
        auto &wrapper = jax.factory().wrap(row);
        const auto cached = wrapper.getBBox();
        const auto nonSaving = wrapper.getBBox(false);
        expect(nonSaving.w == cached.w);
        expect(nonSaving.h == cached.h);
        expect(nonSaving.d == cached.d);
        expect(wrapper.bboxComputed);
    };

    "bbox_false_starts_from_zero_without_mutating_unsaved_wrapper_state"_test = [] {
        TestMeasurementTarget target;
        mml::MmlDocument document;
        auto jax = makeJax(document, target);
        mml::AttributeSet attrs;
        attrs.set(u"style", u"font-size: 150%; margin-left: 3px");
        attrs.set(u"lspace", u".2em");
        attrs.set(u"rspace", u".3em");
        auto &text = dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
        text.setText(u"+");
        std::array<mml::MmlNode *, 1> children{&text};
        auto &mo = document.factory().make(mml::NodeKind::mo, std::move(attrs), children);
        auto &left = token(document, mml::NodeKind::mi, u"a");
        auto &right = token(document, mml::NodeKind::mi, u"b");
        std::array<mml::MmlNode *, 3> rowChildren{&left, &mo, &right};
        auto &row = document.factory().make(mml::NodeKind::mrow, {}, rowChildren);
        row.setInheritedAttributes();
        row.setTeXclass(nullptr);
        document.setRoot(row);
        (void)jax.factory().wrap(row);
        auto &wrapper = *jax.wrapperFor(mo);

        expect(wrapper.bbox.scale == 1.5);
        expect(wrapper.bbox.L == .2);
        expect(wrapper.bbox.R == .3);
        expect(!wrapper.bboxComputed);
        const auto result = wrapper.getBBox(false);
        expect(result.scale == 1.0);
        expect(result.rscale == 1.0);
        expect(result.L == 0.0);
        expect(result.R == 0.0);
        expect(wrapper.bbox.scale == 1.5);
        expect(!wrapper.bboxComputed);
    };

    "fontfamily_attribute_overrides_removed_inline_fontfamily"_test = [] {
        TestMeasurementTarget target;
        mml::MmlDocument document;
        auto jax = makeJax(document, target);
        mml::AttributeSet attributes;
        attributes.set(u"style", u"font-family: InlineMath");
        attributes.set(u"fontfamily", u"MathAttribute");
        auto &text = dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
        text.setText(u"x");
        std::array<mml::MmlNode *, 1> children{&text};
        auto &mi = document.factory().make(mml::NodeKind::mi, std::move(attributes), children);
        document.setRoot(mi);

        auto &wrapper = jax.factory().wrap(mi);
        expect(wrapper.variant == "-explicitFont");
        expect(wrapper.styles->get(u"fontFamily") == u"MathAttribute");
    };
};
