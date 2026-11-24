#include "support/folia_test.hpp"
import folia.mermaid;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

// Font selection is a deterministic test double. It never invents dimensions:
// every numeric measurement, wrapping, or font-metric request is an error.
// Packet and Info have fixed geometry, so they can exercise real SVG emission
// with this provider. Actual font shaping is covered by the Pango/DirectWrite
// regression providers, not by these facade/forwarding tests.
class RejectingMeasurer final : public TextMeasurer {
public:
    bool select_family = false;
    mutable std::size_t family_calls = 0, measurement_calls = 0;
    mutable std::vector<std::u16string> requested_families;

    std::expected<std::u16string, std::string> resolve_family(
        std::u16string_view families, MermaidFontContext) const override {
        ++family_calls;
        requested_families.emplace_back(families);
        if (!select_family) return std::unexpected("Test font resolver rejected");
        return std::u16string{u"Arial"};
    }
    std::expected<MermaidTextMetrics, std::string> measure(
        std::u16string_view, const MermaidFont&, bool,
        std::span<const MermaidTextStyle>, bool) const override {
        ++measurement_calls;
        return std::unexpected("Test text measurement rejected");
    }
    std::expected<std::vector<std::u16string>, std::string> wrap(
        std::u16string_view, const MermaidFont&, double,
        std::span<const MermaidTextStyle>, bool) const override {
        ++measurement_calls;
        return std::unexpected("Test text wrapping rejected");
    }
    std::expected<MermaidSvgFontMetrics, std::string> svg_font_metrics(
        const MermaidFont&) const override {
        ++measurement_calls;
        return std::unexpected("Test font metrics rejected");
    }
};

suite renderer = [] {
    "mermaid_renderer_umbrella_exports_themes_and_matches_existing_pipeline"_test = [] {
        expect(mermaid_themes().size() == 11_u);
        for (const auto& theme : mermaid_themes()) {
            ThemeOptions options{.theme = theme.id};
            const auto session = make_theme_session(options);
            expect(fatal(session.has_value()));
            for (const auto source : {u"packet\n0-7: \"Header\"", u"info showInfo"}) {
                RejectingMeasurer measurer;
                measurer.select_family = true;
                const auto typed = render_diagram(source, measurer, options);
                const auto configured = render_diagram(source, measurer, *session);
                auto prepared = prepare_native_diagram(source, *session);
                expect(fatal(prepared.has_value()));
                const auto original = render_measured_diagram(std::move(*prepared), measurer);
                expect(fatal(typed.has_value())) << (typed ? "" : typed.error());
                expect(fatal(configured.has_value() && original.has_value()));
                expect(typed->svg == original->svg);
                expect(configured->svg == original->svg);
                expect(typed->width == original->width && typed->height == original->height);
                expect(typed->width > 0 && typed->height > 0);
                expect(measurer.family_calls == 3_u);
                expect(measurer.measurement_calls == 0_u);
            }
        }
    };

    "mermaid_renderer_forwards_custom_variables_and_source_theme_precedence"_test = [] {
        ThemeOptions options{.theme = MermaidTheme::base};
        options.variables.mapping.emplace(u"textColor",
            ConfigValue{.kind = ConfigKind::string, .string = u"#123456"});
        options.variables.mapping.emplace(u"fontFamily",
            ConfigValue{.kind = ConfigKind::string, .string = u"Arial, sans-serif"});
        RejectingMeasurer measurer;
        measurer.select_family = true;
        const auto custom = render_diagram(u"info showInfo", measurer, options);
        expect(fatal(custom.has_value()));
        expect(custom->svg.find("fill=\"#123456\"") != std::string::npos);
        expect(custom->svg.find("font-family=\"Arial\"") != std::string::npos);
        expect(fatal(!measurer.requested_families.empty()));
        expect(bool(measurer.requested_families.front() == u"Arial, sans-serif"));

        const auto forest = make_theme_session({.theme = MermaidTheme::forest});
        expect(fatal(forest.has_value()));
        const auto source = u"packet\n0-7: \"Header\"";
        const auto before = render_diagram(source, measurer, *forest);
        const auto explicit_dark = render_diagram(source, measurer, ThemeOptions{.theme = MermaidTheme::dark});
        const auto local_dark = render_diagram(
            u"---\nconfig: {theme: dark}\n---\npacket\n0-7: \"Header\"", measurer, *forest);
        const auto after = render_diagram(source, measurer, *forest);
        expect(fatal(before.has_value() && explicit_dark.has_value() && local_dark.has_value() && after.has_value()));
        expect(local_dark->svg == explicit_dark->svg);
        expect(before->svg == after->svg);
        expect(bool(forest->current_config()->find(u"theme")->string == u"forest"));
        expect(measurer.measurement_calls == 0_u);
    };

    "mermaid_renderer_rejects_input_and_configuration_before_host_calls"_test = [] {
        RejectingMeasurer measurer;
        for (const auto source : {u"", u"unrecognized diagram", u"packet\n0-7: \"unterminated"}) {
            const auto result = render_diagram(source, measurer);
            expect(!result);
        }
        expect(!render_diagram(u"info", measurer,
            ThemeOptions{.theme = static_cast<MermaidTheme>(999)}));
        expect(!render_diagram(u"info", measurer, ThemeOptions{.variables = ConfigValue{}}));
        ThemeOptions invalid{.theme = MermaidTheme::base};
        invalid.variables.mapping.emplace(u"primaryColor",
            ConfigValue{.kind = ConfigKind::string, .string = u"bad-color"});
        expect(!render_diagram(u"info", measurer, invalid));

        FrontendLimits limits;
        limits.source_units = 2;
        expect(!render_diagram(u"info showInfo", measurer, ThemeOptions{}, {}, limits));
        const auto session = make_theme_session();
        expect(fatal(session.has_value()));
        expect(!render_diagram(u"info showInfo", measurer, *session, {}, limits));
        expect(measurer.family_calls == 0_u);
        expect(measurer.measurement_calls == 0_u);
    };

    "mermaid_renderer_propagates_host_font_and_measurement_errors"_test = [] {
        for (const auto source : {u"packet\n0-7: \"Header\"", u"info showInfo"}) {
            RejectingMeasurer measurer;
            const auto result = render_diagram(source, measurer);
            expect(fatal(!result));
            expect(result.error() == "Test font resolver rejected");
            expect(measurer.family_calls == 1_u);
            expect(measurer.measurement_calls == 0_u);
        }
        RejectingMeasurer measurer;
        measurer.select_family = true;
        const auto pie = render_diagram(u"pie\n\"Slice\": 1", measurer);
        expect(fatal(!pie));
        expect(pie.error() == "Test text measurement rejected");
        expect(measurer.family_calls == 1_u);
        expect(measurer.measurement_calls == 1_u);
        const auto recovered = render_diagram(u"info showInfo", measurer);
        expect(recovered.has_value());
    };

    "mermaid_renderer_forwards_math_service_and_its_errors"_test = [] {
        RejectingMeasurer measurer;
        measurer.select_family = true;
        std::size_t math_calls = 0;
        const MathLabelRender math = [&](std::u16string_view tex, double size)
            -> std::expected<MathFragment, std::string> {
            ++math_calls;
            expect(bool(tex == u"x"));
            expect(size > 0);
            return std::unexpected("Test mathematical renderer rejected");
        };
        const auto result = render_diagram(
            u"---\nconfig: {look: classic, htmlLabels: true}\n---\nflowchart LR\nA[\"$$x$$\"]",
            measurer, ThemeOptions{}, math);
        expect(fatal(!result));
        expect(result.error() == "Test mathematical renderer rejected");
        expect(math_calls == 1_u);
        expect(measurer.measurement_calls == 0_u);
    };
};
}
