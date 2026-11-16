#include "support/folia_test.hpp"
import folia.mermaid.usecase_svg;
import folia.mermaid.svg_text_measure;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

// Opaque, explicitly sized label payloads test the layout/paint contract.
// Production MathJax and actual glyph paths are covered by native_math.cpp.
MathLabel box_label(double width = 140, double height = 100, double font_size = 16) {
    MathFragment fragment{
        "<svg xmlns=\"http://www.w3.org/2000/svg\" data-math=\"usecase-fixture\" width=\"" +
        std::to_string(width) + "\" height=\"" + std::to_string(height) +
        "\" viewBox=\"0 0 140 100\" preserveAspectRatio=\"none\">"
        "<path d=\"M0 0H140V100H0Z\"/></svg>", width, height, height - 8};
    MathLabelAtom atom;
    atom.content = std::move(fragment);
    atom.width = width;
    atom.height = height;
    atom.baseline = height - 8;
    MathLabelLine line;
    line.atoms.push_back(std::move(atom));
    line.width = width;
    line.height = height;
    return {{std::move(line)}, width, height, font_size};
}

UsecaseRenderInput single_node(UsecaseNodeKind kind = UsecaseNodeKind::usecase) {
    UsecaseRenderInput input;
    input.min_node_width = 0;
    input.document.nodes.push_back({.id = u"A", .label = u"$$x$$", .kind = kind});
    return input;
}

UsecaseTextMeasure reject_text() {
    return [](std::u16string_view, std::u16string_view, double, unsigned,
              const SvgFamilyTextOptions&) -> std::expected<SvgTextMetrics, std::string> {
        return std::unexpected("Unexpected ordinary text measurement");
    };
}

UsecaseTextMeasure plain_fixture() {
    return [](std::u16string_view, std::u16string_view, double, unsigned,
              const SvgFamilyTextOptions&) -> std::expected<SvgTextMetrics, std::string> {
        return SvgTextMetrics{40, 16, 12, 0, 16, 12};
    };
}

MathLabelMeasure constant_math(double height = 100) {
    return [height](std::u16string_view, std::u16string_view, double size, unsigned, bool)
        -> std::expected<std::optional<MathLabel>, std::string> {
        return box_label(140, height, size);
    };
}

std::size_t occurrences(std::string_view text, std::string_view needle) {
    std::size_t total = 0, at = 0;
    while ((at = text.find(needle, at)) != text.npos) {
        ++total;
        at += needle.size();
    }
    return total;
}

UsecaseRenderInput prepared(std::u16string_view source) {
    auto session = ConfigSession::create();
    expect(fatal(session.has_value()));
    auto diagram = prepare_diagram(source, *session);
    expect(fatal(diagram.has_value())) << (diagram ? "" : diagram.error());
    auto input = prepare_usecase_render(*diagram);
    expect(fatal(input.has_value())) << (input ? "" : input.error());
    return std::move(*input);
}

suite usecase_math = [] {
    "mermaid_usecase_math_boxes_size_actors_ellipses_and_notes"_test = [] {
        for (const bool elk : {false, true}) {
            for (const auto kind : {UsecaseNodeKind::actor, UsecaseNodeKind::usecase,
                                    UsecaseNodeKind::note}) {
                auto input = single_node(kind);
                input.elk = elk;
                auto first = render_usecase_svg(input, reject_text(), constant_math());
                auto taller = render_usecase_svg(input, reject_text(), constant_math(180));
                expect(fatal(first.has_value())) << (first ? "" : first.error());
                expect(fatal(taller.has_value())) << (taller ? "" : taller.error());
                expect(std::abs(taller->height - first->height - 80) < 1e-8);
                expect(std::abs(taller->width - first->width) < 1e-8);
                expect(first->height >= 160.0);
                expect(occurrences(first->svg, "data-math=\"usecase-fixture\"") == 1_u);
                expect(first->svg.find("<foreignObject") == std::string::npos);
                expect(first->svg.find("$$") == std::string::npos);
                if (kind == UsecaseNodeKind::usecase)
                    expect(first->svg.find("<ellipse rx=\"90\" ry=\"70\"") != std::string::npos);
            }
        }
    };

    "mermaid_usecase_math_covers_stereotypes_groups_tables_and_edges"_test = [] {
        for (const bool elk : {false, true}) {
            UsecaseRenderInput input;
            input.elk = elk;
            input.theme.actor_font_family = u"ActorFixture";
            input.theme.usecase_font_family = u"UsecaseFixture";
            input.theme.font_family = u"BaseFixture";
            input.theme.actor_weight = 600;
            input.theme.usecase_weight = 500;
            input.document.nodes = {
                {.id = u"A", .label = u"$$actor$$", .stereotype = u"$$actorType$$",
                 .kind = UsecaseNodeKind::actor},
                {.id = u"G", .label = u"$$group$$", .kind = UsecaseNodeKind::boundary,
                 .shape = UsecaseShape::package},
                {.id = u"U", .label = u"$$case$$", .parent = u"G", .stereotype = u"$$caseType$$",
                 .business = true},
                {.id = u"J", .label = u"$$table$$", .kind = UsecaseNodeKind::json,
                 .json_rows = {{u"$$key$$", u"$$value$$"}}},
                {.id = u"N", .label = u"$$note$$", .kind = UsecaseNodeKind::note},
            };
            input.document.edges = {
                {.id = u"E", .from = u"A", .to = u"U", .label = u"$$edge$$"},
                {.id = u"E2", .from = u"J", .to = u"U", .label = u"$$edge2$$"},
            };
            std::size_t calls = 0;
            const MathLabelMeasure math = [&](std::u16string_view text, std::u16string_view family,
                double size, unsigned weight, bool italic)
                -> std::expected<std::optional<MathLabel>, std::string> {
                ++calls;
                expect(!italic);
                if (text.find(u"actor") != text.npos) {
                    expect(family == input.theme.actor_font_family);
                    expect(size == input.theme.actor_font_size);
                    expect(weight == 600_u);
                } else if (text.find(u"case") != text.npos) {
                    expect(family == input.theme.usecase_font_family);
                    expect(size == input.theme.font_size);
                    expect(weight == 500_u);
                } else {
                    expect(family == input.theme.font_family);
                    expect(size == (text.find(u"edge") != text.npos ? 10 : input.theme.diagram_font_size));
                    expect(weight == 400_u);
                }
                return box_label(140, 100, size);
            };
            auto svg = render_usecase_svg(input, reject_text(), math);
            expect(fatal(svg.has_value())) << (svg ? "" : svg.error());
            expect(calls == 11_u);
            expect(occurrences(svg->svg, "usecase-math-label") == 11_u);
            expect(svg->svg.find("$$") == std::string::npos);
            expect(svg->svg.find("class=\"background\"") != std::string::npos);
            expect(svg->svg.find("<ellipse rx=\"90\" ry=\"121\"") != std::string::npos);
        }
    };

    "mermaid_usecase_math_json_cells_use_both_dimensions"_test = [] {
        auto input = single_node(UsecaseNodeKind::json);
        input.elk = false;
        input.document.nodes[0].json_rows = {{u"$$key$$", u"$$value$$"}};
        const MathLabelMeasure math = [](std::u16string_view text, std::u16string_view,
            double size, unsigned, bool) -> std::expected<std::optional<MathLabel>, std::string> {
            if (text == u"$$key$$") return box_label(60, 80, size);
            if (text == u"$$value$$") return box_label(180, 140, size);
            return box_label(140, 100, size);
        };
        auto svg = render_usecase_svg(input, reject_text(), math);
        expect(fatal(svg.has_value())) << (svg ? "" : svg.error());
        // Columns include 8 px per side; rows include 4 px above and below.
        // The outer border adds 2 px and the diagram adds 40 px padding.
        expect(std::abs(svg->width - (60 + 180 + 32 + 2 + 40)) < 1e-8);
        expect(std::abs(svg->height - (100 + 140 + 16 + 2 + 40)) < 1e-8);
        expect(occurrences(svg->svg, "data-math=\"usecase-fixture\"") == 3_u);
    };

    "mermaid_usecase_math_preserves_tex_before_conversion_and_escapes_text"_test = [] {
        auto input = single_node();
        input.elk = false;
        const auto source = uR"($$\frac{a_b}{c}<d$$)";
        input.document.nodes[0].label = source;
        input.theme.usecase_font_family = u"Fixture \"<&>";
        const MathLabelMeasure math = [&](std::u16string_view text, std::u16string_view family,
            double size, unsigned, bool) -> std::expected<std::optional<MathLabel>, std::string> {
            expect(text == source);
            expect(family == input.theme.usecase_font_family);
            auto label = box_label(140, 100, size);
            auto& line = label.lines[0];
            line.atoms[0].x = 60;
            MathLabelAtom prefix;
            prefix.content = u"safe <tag> & \"text\"";
            prefix.width = 60;
            prefix.height = 20;
            prefix.baseline = 15;
            prefix.y = 40;
            line.atoms.insert(line.atoms.begin(), std::move(prefix));
            label.width = line.width = 200;
            return label;
        };
        auto svg = render_usecase_svg(input, reject_text(), math);
        expect(fatal(svg.has_value())) << (svg ? "" : svg.error());
        expect(svg->svg.find("safe &lt;tag&gt; &amp; &quot;text&quot;") != std::string::npos);
        expect(svg->svg.find("font-family=\"Fixture &quot;&lt;&amp;&gt;\"") != std::string::npos);
        expect(svg->svg.find("<tag>") == std::string::npos);
    };

    "mermaid_usecase_math_respects_html_gate_and_optional_provider"_test = [] {
        for (const auto config : {u"htmlLabels: false", u"flowchart: {htmlLabels: false}"}) {
            auto input = prepared(u"---\nconfig:\n  " + std::u16string(config) +
                u"\n---\nusecase-beta\nA(\"$$x$$\")\n");
            expect(!input.html_labels);
            std::size_t calls = 0;
            const MathLabelMeasure math = [&](std::u16string_view, std::u16string_view,
                double size, unsigned, bool) -> std::expected<std::optional<MathLabel>, std::string> {
                ++calls;
                return box_label(140, 100, size);
            };
            auto svg = render_usecase_svg(input, plain_fixture(), math);
            expect(fatal(svg.has_value())) << (svg ? "" : svg.error());
            expect(calls == 0_u);
            expect(svg->svg.find("$$x$$") != std::string::npos);
            expect(svg->svg.find("usecase-math-label") == std::string::npos);

            input.html_labels = true;
            const MathLabelMeasure declined = [](std::u16string_view, std::u16string_view,
                double, unsigned, bool) -> std::expected<std::optional<MathLabel>, std::string> {
                return std::nullopt;
            };
            auto with_provider = render_usecase_svg(input, plain_fixture(), declined);
            auto without_provider = render_usecase_svg(input, plain_fixture());
            expect(fatal(with_provider.has_value() && without_provider.has_value()));
            expect(with_provider->svg == without_provider->svg);
        }
    };

    "mermaid_usecase_math_errors_propagate_without_plain_fallback"_test = [] {
        auto input = single_node();
        const MathLabelMeasure failed = [](std::u16string_view, std::u16string_view,
            double, unsigned, bool) -> std::expected<std::optional<MathLabel>, std::string> {
            return std::unexpected("Use Case formula failed");
        };
        auto svg = render_usecase_svg(input, reject_text(), failed);
        expect(!svg && svg.error() == "Use Case formula failed");
    };

    "mermaid_usecase_math_keeps_diagram_title_on_upstream_text_path"_test = [] {
        auto input = single_node();
        input.document.title = u"$$literal title$$";
        std::size_t title_calls = 0;
        const UsecaseTextMeasure text = [&](std::u16string_view value, std::u16string_view,
            double, unsigned, const SvgFamilyTextOptions&) -> std::expected<SvgTextMetrics, std::string> {
            expect(value == input.document.title);
            ++title_calls;
            return SvgTextMetrics{80, 16, 12, 0, 16, 12};
        };
        auto svg = render_usecase_svg(input, text, constant_math());
        expect(fatal(svg.has_value())) << (svg ? "" : svg.error());
        expect(title_calls == 1_u);
        expect(occurrences(svg->svg, "usecase-math-label") == 1_u);
        expect(svg->svg.find(">$$literal title$$</text>") != std::string::npos);
    };
};
}
