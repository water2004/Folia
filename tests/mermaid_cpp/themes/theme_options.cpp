#include "support/folia_test.hpp"
import folia.mermaid.theme_options;
import folia.mermaid.render_entry;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "mermaid_cpp/support/theme_fixture.hpp"

DiagramSvg render_unmeasured(std::u16string_view source, const ConfigSession& session) {
    // Packet has explicit bit geometry and Info a fixed viewport; neither needs
    // fabricated text measurements to exercise the actual production SVG path.
    auto diagram = prepare_native_diagram(source, session);
    if (!diagram) throw std::runtime_error(diagram.error());
    auto svg = render_native_diagram(*diagram);
    if (!svg) throw std::runtime_error(svg.error());
    return std::move(*svg);
}

suite theme_options = [] {
    "mermaid_theme_options_catalog_has_canonical_unique_names"_test = [] {
        const std::array expected{
            u"default", u"dark", u"base", u"forest", u"neutral", u"neo",
            u"neo-dark", u"redux", u"redux-dark", u"redux-color", u"redux-dark-color"};
        const auto catalog = mermaid_themes();
        expect(catalog.size() == expected.size());
        std::set<MermaidTheme> ids;
        std::set<std::u16string_view> names;
        for (const auto& entry : catalog) {
            expect(ids.insert(entry.id).second);
            expect(names.insert(entry.name).second);
            expect(std::ranges::find(expected, entry.name) != expected.end());
            expect(mermaid_theme_name(entry.id) == entry.name);
            expect(find_mermaid_theme(entry.name) == entry.id);
        }
        expect(!find_mermaid_theme(u"Dark"));
        expect(!find_mermaid_theme(u"missing-theme"));
        expect(!mermaid_theme_name(static_cast<MermaidTheme>(-1)));
        expect(!mermaid_theme_name(static_cast<MermaidTheme>(999)));
    };

    "mermaid_theme_options_match_initialize_for_all_presets_and_renderers"_test = [] {
        for (const auto& entry : mermaid_themes()) {
            auto typed = make_theme_session({.theme = entry.id});
            auto original = ConfigSession::create();
            expect(fatal(typed.has_value())) << hex(entry.name);
            expect(fatal(original.has_value()));
            ConfigValue config{.kind = ConfigKind::mapping};
            config.mapping.emplace(u"theme",
                ConfigValue{.kind = ConfigKind::string, .string = std::u16string{entry.name}});
            expect(fatal(original->initialize(config).has_value()));
            expect(snapshot(*typed->current_config()) == snapshot(*original->current_config()));
            expect(snapshot(*typed->user_config()) == snapshot(*original->user_config()));
            for (const auto source : {u"packet\n0-7: \"Header\"\n8-15: \"Data\"", u"info showInfo"}) {
                const auto actual = render_unmeasured(source, *typed);
                const auto reference = render_unmeasured(source, *original);
                expect(actual.svg == reference.svg) << hex(entry.name);
                expect(actual.width == reference.width && actual.height == reference.height);
                expect(actual.width > 0 && actual.height > 0);
                expect(actual.svg.find("<svg") != std::string::npos);
            }
            // Preparation uses the real per-family adapters. Geometry tests
            // separately provide platform-measured text to those renderers.
            for (const auto source : {
                u"---\nconfig: {look: classic}\n---\nflowchart LR\nA --> B",
                u"---\nconfig: {look: classic}\n---\nsequenceDiagram\nA->>B: Request",
                u"---\nconfig: {look: classic}\n---\nerDiagram\nCUSTOMER ||--o{ ORDER : places"}) {
                const auto actual = prepare_native_diagram(source, *typed);
                const auto reference = prepare_native_diagram(source, *original);
                expect(fatal(actual.has_value())) << hex(entry.name) << (actual ? "" : actual.error());
                expect(fatal(reference.has_value()));
                expect(actual->content.index() == reference->content.index());
                expect(snapshot(actual->prepared.effective_config) ==
                    snapshot(reference->prepared.effective_config));
                expect(bool(field(actual->prepared.effective_config, u"theme").string == entry.name));
                if (const auto* flow = std::get_if<FlowchartRenderInput>(&actual->content))
                    expect(flow->theme.fill == std::get<FlowchartRenderInput>(reference->content).theme.fill);
                if (const auto* sequence = std::get_if<SequenceRenderInput>(&actual->content))
                    expect(sequence->theme.actor_fill == std::get<SequenceRenderInput>(reference->content).theme.actor_fill);
                if (const auto* er = std::get_if<ErRenderInput>(&actual->content))
                    expect(er->theme.background == std::get<ErRenderInput>(reference->content).theme.background);
            }
        }
    };

    "mermaid_theme_options_omitted_selection_preserves_family_appearance"_test = [] {
        const auto defaults = make_theme_session();
        const auto explicit_default = make_theme_session({.theme = MermaidTheme::default_theme});
        expect(fatal(defaults.has_value() && explicit_default.has_value()));
        expect(defaults->user_config()->find(u"theme") == nullptr);
        const auto flow = prepare_diagram(u"flowchart LR\nA-->B", *defaults);
        const auto packet = prepare_diagram(u"packet\n0-7: \"Data\"", *defaults);
        const auto selected = prepare_diagram(u"flowchart LR\nA-->B", *explicit_default);
        expect(fatal(flow.has_value() && packet.has_value() && selected.has_value()));
        expect(bool(field(flow->effective_config, u"theme").string == u"redux-color"));
        expect(bool(field(flow->effective_config, u"look").string == u"neo"));
        expect(bool(field(packet->effective_config, u"theme").string == u"default"));
        expect(bool(field(packet->effective_config, u"look").string == u"classic"));
        expect(bool(field(selected->effective_config, u"theme").string == u"default"));
        expect(bool(field(selected->effective_config, u"look").string == u"neo"));
    };

    "mermaid_theme_options_custom_variables_reach_typed_data_and_svg"_test = [] {
        ThemeOptions options{.theme = MermaidTheme::base, .variables = json(uR"({
            "primaryColor":"#123456", "primaryTextColor":"#abcdef",
            "lineColor":"#986754", "textColor":"#abcdef",
            "packet":{"blockFillColor":"#123456","labelColor":"#abcdef","blockStrokeColor":"#f0e1d2"}
        })")};
        const auto before = snapshot(options.variables);
        const auto custom = make_theme_session(options);
        const auto plain = make_theme_session({.theme = MermaidTheme::base});
        expect(fatal(custom.has_value() && plain.has_value()));
        expect(snapshot(options.variables) == before);
        const auto config = custom->current_config();
        const auto baseline = plain->current_config();
        const auto& variables = field(*config, u"themeVariables");
        expect(bool(field(variables, u"primaryColor").string == u"#123456"));
        expect(bool(field(variables, u"lineColor").string == u"#986754"));
        expect(field(variables, u"primaryBorderColor").string !=
            field(field(*baseline, u"themeVariables"), u"primaryBorderColor").string);
        const auto packet = prepare_native_diagram(u"packet\n0-7: \"Data\"", *custom);
        expect(fatal(packet.has_value()));
        const auto& theme = std::get<PacketRenderInput>(packet->content).theme;
        expect(theme.fill_color == "#123456");
        expect(theme.label_color == "#abcdef");
        expect(theme.stroke_color == "#f0e1d2");
        const auto svg = render_unmeasured(u"packet\n0-7: \"Data\"", *custom);
        for (const auto color : {"#123456", "#abcdef", "#f0e1d2"})
            expect(svg.svg.find(color) != std::string::npos);
        const auto info = render_unmeasured(u"info showInfo", *custom);
        expect(info.svg.find("fill=\"#abcdef\"") != std::string::npos);
        options.variables.mapping.clear();
        expect(render_unmeasured(u"info showInfo", *custom).svg == info.svg);
    };

    "mermaid_theme_options_source_precedence_and_request_isolation"_test = [] {
        const auto forest = make_theme_session({.theme = MermaidTheme::forest});
        const auto dark = make_theme_session({.theme = MermaidTheme::dark});
        expect(fatal(forest.has_value() && dark.has_value()));
        const auto before = snapshot(*forest->current_config());
        const auto source = u"packet\n0-7: \"Data\"";
        const auto original = render_unmeasured(source, *forest);
        const auto expected_dark = render_unmeasured(source, *dark);
        const auto frontmatter = render_unmeasured(
            u"---\nconfig:\n  theme: dark\n---\npacket\n0-7: \"Data\"", *forest);
        const auto directive = render_unmeasured(
            u"%%{init: {\"theme\": \"dark\"}}%%\npacket\n0-7: \"Data\"", *forest);
        expect(frontmatter.svg == expected_dark.svg);
        expect(directive.svg == expected_dark.svg);
        const auto scoped = prepare_diagram(
            u"---\nconfig:\n  theme: neutral\n  flowchart:\n    theme: dark\n---\nflowchart LR\nA-->B", *forest);
        expect(fatal(scoped.has_value()));
        expect(bool(field(scoped->effective_config, u"theme").string == u"dark"));
        expect(!prepare_native_diagram(u"---\nconfig: [\n---\npacket", *forest));
        expect(render_unmeasured(source, *forest).svg == original.svg);
        expect(snapshot(*forest->current_config()) == before);
    };

    "mermaid_theme_options_theme_switch_rederives_from_raw_overrides"_test = [] {
        const auto variables = json(uR"({"primaryColor":"#123456"})");
        const auto base = make_theme_session({.theme = MermaidTheme::base, .variables = variables});
        const auto forest = make_theme_session({.theme = MermaidTheme::forest, .variables = variables});
        expect(fatal(base.has_value() && forest.has_value()));
        const auto switched = prepare_diagram(
            u"---\nconfig: {theme: forest}\n---\npacket\n0-7: \"Data\"", *base);
        const auto fresh = prepare_diagram(u"packet\n0-7: \"Data\"", *forest);
        expect(fatal(switched.has_value() && fresh.has_value()));
        expect(snapshot(field(switched->effective_config, u"themeVariables")) ==
            snapshot(field(fresh->effective_config, u"themeVariables")));
        expect(base->user_config()->find(u"themeVariables")->mapping.size() == 1_u);
    };

    "mermaid_theme_options_reject_invalid_inputs_without_mutation"_test = [] {
        expect(!make_theme_session({.theme = static_cast<MermaidTheme>(999)}));
        expect(!make_theme_session({.variables = ConfigValue{}}));
        expect(!make_theme_session({.variables = json(u"[]")}));
        expect(!make_theme_session({}, {.config = {.values = 1}}));
        for (const auto input : {uR"({"primaryColor":"bad-color"})",
            uR"({"THEME_COLOR_LIMIT":13})", uR"({"constructor":"value"})"}) {
            ThemeOptions options{.theme = MermaidTheme::base, .variables = json(input)};
            const auto before = snapshot(options.variables);
            expect(!make_theme_session(options));
            expect(snapshot(options.variables) == before);
        }
        ThemeOptions inactive;
        inactive.variables.string = u"hidden";
        expect(!make_theme_session(inactive));
        expect(bool(inactive.variables.string == u"hidden"));
        // A rejected request has no shared session or palette to poison.
        expect(make_theme_session({.theme = MermaidTheme::forest}).has_value());
    };
};
}
