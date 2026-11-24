#include "support/folia_test.hpp"
import folia.mermaid.theme_options;
import folia.mermaid.render_entry;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "mermaid_cpp/support/theme_fixture.hpp"

std::filesystem::path fixture_path(std::string_view name) {
    const auto directory = std::getenv("FOLIA_MERMAID_FIXTURES");
    return std::filesystem::path{directory ? directory : FOLIA_MERMAID_FIXTURE_DIR} / name;
}
std::vector<std::string_view> columns(std::string_view line) {
    std::vector<std::string_view> result;
    for (;;) {
        const auto tab = line.find('\t');
        result.push_back(line.substr(0, tab));
        if (tab == line.npos) return result;
        line.remove_prefix(tab + 1);
    }
}
// The expected palettes are committed outputs of the locked JS themes, not
// values recomputed with the implementation being tested.
std::map<std::u16string, ConfigValue> official_palettes() {
    std::ifstream input{fixture_path("themes.tsv")};
    if (!input) throw std::runtime_error("Cannot open official theme fixtures");
    std::map<std::u16string, ConfigValue> result;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto parts = columns(line);
        if (parts.size() == 5 && parts[1] == "baseline")
            result.emplace(std::u16string{parts[0].begin(), parts[0].end()}, json(decode(parts[3])));
    }
    return result;
}

suite theme_renderer_fallbacks = [] {
    "mermaid_radar_theme_fallback_matches_official_styles_for_every_preset"_test = [] {
        const auto palettes = official_palettes();
        expect(fatal(palettes.size() == 11_u));
        const auto& default_style = field(palettes.at(u"default"), u"radar");
        for (const auto& entry : mermaid_themes()) {
            const auto session = make_theme_session({.theme = entry.id});
            expect(fatal(session.has_value()));
            const auto before = snapshot(*session->current_config());
            const auto actual = prepare_radar(
                u"radar-beta\naxis Readability,Fidelity,Speed\ncurve current{3,4,2}\nmax 5", *session);
            expect(fatal(actual.has_value())) << hex(entry.name) << (actual ? "" : actual.error());
            const auto& palette = palettes.at(std::u16string{entry.name});
            const auto* selected_style = palette.find(u"radar");
            const auto& style = selected_style ? *selected_style : default_style;
            const auto& theme = actual->theme;
            expect(theme.axis == field(style, u"axisColor").string);
            expect(theme.graticule == field(style, u"graticuleColor").string);
            expect(theme.text == field(palette, u"textColor").string);
            for (const auto [key, value] : {
                std::pair{u"axisStrokeWidth", theme.axis_width}, {u"axisLabelFontSize", theme.axis_size},
                {u"curveOpacity", theme.curve_opacity}, {u"curveStrokeWidth", theme.curve_width},
                {u"graticuleOpacity", theme.graticule_opacity}, {u"graticuleStrokeWidth", theme.graticule_width},
                {u"legendFontSize", theme.legend_size}})
                expect(value == field(style, key).number);
            expect(theme.colors.size() == static_cast<std::size_t>(field(palette, u"THEME_COLOR_LIMIT").number));
            expect(fatal(!theme.colors.empty()));
            expect(theme.colors.front() == field(palette, u"cScale0").string);
            const auto svg = render_radar_svg(*actual);
            expect(fatal(svg.has_value()));
            expect(svg->svg.find("radarCurve-0") != std::string::npos);
            expect(snapshot(*session->current_config()) == before);
        }
    };

    "mermaid_radar_theme_fallback_merges_partial_overrides_and_keeps_zero"_test = [] {
        const auto palettes = official_palettes();
        for (const auto& entry : mermaid_themes()) {
            const auto& palette = palettes.at(std::u16string{entry.name});
            if (palette.find(u"radar")) continue;
            const auto options = ThemeOptions{.theme = entry.id, .variables = json(uR"({
                "radar":{"axisColor":"#123456","curveOpacity":0,"legendFontSize":21}
            })")};
            const auto session = make_theme_session(options);
            expect(fatal(session.has_value()));
            const auto actual = prepare_radar(u"radar-beta\naxis A,B,C\ncurve sample{1,2,3}", *session);
            expect(fatal(actual.has_value()));
            expect(bool(actual->theme.axis == u"#123456"));
            expect(actual->theme.curve_opacity == 0);
            expect(actual->theme.legend_size == 21);
            expect(actual->theme.axis_width == 2);
            expect(bool(actual->theme.graticule == u"#DEDEDE"));
            expect(actual->theme.colors.front() == field(palette, u"cScale0").string);
            const auto svg = render_radar_svg(*actual);
            expect(fatal(svg.has_value()));
            expect(svg->svg.find("stroke=\"#123456\"") != std::string::npos);
            expect(svg->svg.find("fill-opacity=\"0\"") != std::string::npos);
        }
    };

    "mermaid_xy_theme_fallback_matches_official_database_for_every_preset"_test = [] {
        const auto palettes = official_palettes();
        const auto& default_style = field(palettes.at(u"default"), u"xyChart");
        for (const auto& entry : mermaid_themes()) {
            const auto session = make_theme_session({.theme = entry.id});
            expect(fatal(session.has_value()));
            const auto actual = prepare_xy(
                u"xychart\nx-axis [Start, Middle, End]\ny-axis 0 --> 10\nline [2,8,5]", *session);
            expect(fatal(actual.has_value())) << hex(entry.name) << (actual ? "" : actual.error());
            const auto& selected = field(palettes.at(std::u16string{entry.name}), u"xyChart");
            const auto* label = selected.find(u"dataLabelColor");
            expect(actual->theme.data_label == (label ? label->string : field(default_style, u"dataLabelColor").string));
            expect(actual->theme.background == field(selected, u"backgroundColor").string);
            expect(actual->theme.title == field(selected, u"titleColor").string);
            expect(actual->theme.legend == field(selected, u"legendTextColor").string);
            expect(actual->theme.x.line == field(selected, u"xAxisLineColor").string);
            expect(actual->theme.y.label == field(selected, u"yAxisLabelColor").string);
            const auto& palette = field(selected, u"plotColorPalette").string;
            expect(fatal(!actual->theme.colors.empty()));
            expect(actual->theme.colors.front() == palette.substr(0, palette.find(u',')));
        }
    };

    "mermaid_xy_theme_fallback_renders_data_labels_with_actual_fixture_measurements"_test = [] {
        std::ifstream input{fixture_path("xy-measurements.tsv")};
        expect(fatal(input.is_open()));
        std::string line;
        std::u16string source;
        ConfigValue requests, measured;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const auto parts = columns(line);
            if (parts.size() == 5 && parts[0] == "labels-vertical-default") {
                source = decode(parts[1]);
                requests = json(decode(parts[3]));
                measured = json(decode(parts[4]));
                break;
            }
        }
        expect(fatal(!source.empty()));
        for (const auto& entry : mermaid_themes()) {
            auto session = ConfigSession::create();
            expect(fatal(session.has_value()));
            auto config = json(uR"({"fontFamily":"Arial"})");
            config.mapping.emplace(u"theme",
                ConfigValue{.kind = ConfigKind::string, .string = std::u16string{entry.name}});
            expect(fatal(session->initialize(config).has_value()));
            const auto actual = prepare_xy(source, *session);
            expect(fatal(actual.has_value()));
            expect(bool(actual->theme.font_family == u"Arial"));
            const auto wanted = xy_measurement_requests(*actual);
            expect(fatal(wanted.size() == requests.sequence.size() && wanted.size() == measured.sequence.size()));
            XYMeasurements dimensions;
            for (std::size_t i = 0; i < wanted.size(); ++i) {
                expect(wanted[i].text == field(requests.sequence[i], u"text").string);
                expect(wanted[i].font_size == field(requests.sequence[i], u"font_size").number);
                dimensions.text.push_back({field(measured.sequence[i], u"width").number,
                    field(measured.sequence[i], u"height").number});
            }
            const auto svg = render_xy_svg(*actual, dimensions);
            expect(fatal(svg.has_value())) << hex(entry.name);
            const auto plot = svg->svg.find("<g class=\"bar-plot-0\">");
            expect(fatal(plot != std::string::npos));
            const auto group = svg->svg.substr(plot, svg->svg.find("</g>", plot) - plot);
            const std::string label_color{actual->theme.data_label.begin(), actual->theme.data_label.end()};
            expect(group.find("fill=\"" + label_color + "\"") != std::string::npos);
            expect(group.find(">20</text>") != std::string::npos);
            expect(group.find(">60</text>") != std::string::npos);
            expect(group.find(">40</text>") != std::string::npos);
        }
    };

    "mermaid_xy_theme_fallback_keeps_explicit_family_colors"_test = [] {
        for (const auto& entry : mermaid_themes()) {
            const auto session = make_theme_session({.theme = entry.id, .variables = json(uR"({
                "xyChart":{"dataLabelColor":"#123456","legendTextColor":"#abcdef"}
            })")});
            expect(fatal(session.has_value()));
            const auto actual = prepare_xy(u"xychart\nbar [2,8,5]", *session);
            expect(fatal(actual.has_value()));
            expect(bool(actual->theme.data_label == u"#123456"));
            expect(bool(actual->theme.legend == u"#abcdef"));
            expect(!actual->theme.colors.empty());
        }
    };
};
}
