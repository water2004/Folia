#include "support/folia_test.hpp"
import folia.mermaid.swimlane_svg;
import folia.mermaid.swimlane_layout;
import folia.mermaid.swimlane_router;
import folia.mermaid.svg_text_measure;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

// Explicit provider boxes exercise geometry without estimating font or TeX
// dimensions. Real MathJax fragments have a separate native integration test.
MathLabel math_box(double width, double height, double font_size = 16) {
    MathLabelAtom atom;
    atom.content = MathFragment{
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + std::to_string(width) +
        "\" height=\"" + std::to_string(height) +
        "\"><path data-math=\"swimlane-fixture\" d=\"M0 0H1\"/></svg>",
        width, height, height + 2};
    atom.width = width;
    atom.height = height;
    atom.baseline = height + 2;
    MathLabelLine line;
    line.atoms.push_back(std::move(atom));
    line.width = width;
    line.height = height;
    return {{std::move(line)}, width, height, font_size};
}

SwimlaneRenderInput prepare(std::u16string_view source) {
    auto session = ConfigSession::create();
    expect(fatal(session.has_value()));
    auto diagram = prepare_diagram(source, *session);
    expect(fatal(diagram.has_value())) << (diagram ? "" : diagram.error());
    auto result = prepare_swimlane_render(*diagram);
    expect(fatal(result.has_value())) << (result ? "" : result.error());
    return std::move(*result);
}

constexpr std::u16string_view source =
    u"swimlane-beta TB\nsubgraph Lane[\"$$title$$\"]\n"
    u"A[\"$$x$$\"]\nB[\"$$y$$\"]\nend\nA -->|$$edge$$| B\n";

SvgFamilyTextMeasure reject_text() {
    return [](std::u16string_view, std::u16string_view, double, unsigned,
              const SvgFamilyTextOptions&) -> std::expected<SvgTextMetrics, std::string> {
        return std::unexpected("Unexpected ordinary text measurement");
    };
}

SvgFamilyTextMeasure plain_text() {
    return [](std::u16string_view, std::u16string_view, double, unsigned,
              const SvgFamilyTextOptions&) -> std::expected<SvgTextMetrics, std::string> {
        return SvgTextMetrics{42, 16, 12, 4};
    };
}

std::size_t count(std::string_view text, std::string_view needle) {
    std::size_t result = 0;
    for (auto at = text.find(needle); at != text.npos; at = text.find(needle, at + needle.size()))
        ++result;
    return result;
}

bool contains(const FlowchartPlacedNode& outer, const FlowchartPlacedNode& inner) {
    constexpr double epsilon = 1e-6;
    return outer.x - outer.width / 2 <= inner.x - inner.width / 2 + epsilon &&
        outer.y - outer.height / 2 <= inner.y - inner.height / 2 + epsilon &&
        outer.x + outer.width / 2 + epsilon >= inner.x + inner.width / 2 &&
        outer.y + outer.height / 2 + epsilon >= inner.y + inner.height / 2;
}

bool disjoint(const FlowchartPlacedNode& a, const FlowchartPlacedNode& b) {
    constexpr double epsilon = 1e-6;
    return a.x + a.width / 2 <= b.x - b.width / 2 + epsilon ||
        b.x + b.width / 2 <= a.x - a.width / 2 + epsilon ||
        a.y + a.height / 2 <= b.y - b.height / 2 + epsilon ||
        b.y + b.height / 2 <= a.y - a.height / 2 + epsilon;
}

suite swimlane_math = [] {
    "mermaid_swimlane_math_covers_lane_node_and_edge_painters"_test = [] {
        auto input = prepare(u"---\nconfig:\n  look: classic\n  flowchart:\n    htmlLabels: true\n---\n" +
            std::u16string{source});
        for (auto& node : input.document.nodes) if (node.id == u"A")
            node.styles = {u"font-family:Fixture Serif", u"font-size:28px", u"font-weight:700",
                u"font-style:italic", u"color:#123456"};
        std::size_t calls = 0;
        const MathLabelMeasure math = [&](std::u16string_view text, std::u16string_view family,
            double size, unsigned weight, bool italic)
            -> std::expected<std::optional<MathLabel>, std::string> {
            ++calls;
            expect(has_math_label(text));
            if (text == u"$$x$$") {
                expect(family == u"Fixture Serif");
                expect(size == 28.0 && weight == 700_u && italic);
            } else {
                expect(family == input.theme.font_family);
                expect(size == input.theme.font_size && weight == 400_u && !italic);
            }
            return text == u"$$title$$" ? math_box(440, 160, size) : math_box(180, 96, size);
        };
        auto rendered = render_swimlane_svg(input, reject_text(), math);
        expect(fatal(rendered.has_value())) << (rendered ? "" : rendered.error());
        expect(calls == 4_u);
        expect(count(rendered->svg, "data-math=\"swimlane-fixture\"") == 4_u);
        expect(rendered->svg.find("$$") == std::string::npos);
        expect(rendered->svg.find("<foreignObject") == std::string::npos);
        expect(rendered->svg.find("color:#123456") != std::string::npos);
        expect(rendered->width >= 456.0);
        expect(rendered->height > 400.0);
    };

    "mermaid_swimlane_math_respects_independent_lane_html_setting"_test = [] {
        struct Case { std::u16string_view config; bool nodes, lanes; std::size_t math_count; };
        for (const auto& item : std::array{
            Case{u"", true, false, 3},
            Case{u"  htmlLabels: true\n", true, false, 3},
            Case{u"  htmlLabels: false\n  flowchart:\n    htmlLabels: true\n", false, true, 1},
            Case{u"  htmlLabels: true\n  flowchart:\n    htmlLabels: false\n", true, false, 3},
            Case{u"  htmlLabels: false\n", false, false, 0}}) {
            auto input = prepare(u"---\nconfig:\n  look: classic\n" + std::u16string{item.config} +
                u"---\n" + std::u16string{source});
            expect(input.nodes_html_labels == item.nodes);
            expect(input.lane_html_labels == item.lanes);
            std::size_t calls = 0;
            const MathLabelMeasure math = [&](std::u16string_view, std::u16string_view,
                double, unsigned, bool) -> std::expected<std::optional<MathLabel>, std::string> {
                ++calls;
                return math_box(90, 80);
            };
            auto rendered = render_swimlane_svg(input, plain_text(), math);
            expect(fatal(rendered.has_value())) << (rendered ? "" : rendered.error());
            expect(calls == item.math_count);
            expect(count(rendered->svg, "data-math=\"swimlane-fixture\"") == item.math_count);
            expect((rendered->svg.find("$$title$$") == std::string::npos) == item.lanes);
            expect((rendered->svg.find("$$x$$") == std::string::npos) == item.nodes);
        }
    };

    "mermaid_swimlane_math_headers_reserve_space_in_every_direction"_test = [] {
        FlowchartInput graph;
        graph.nodes = {{.id=u"L", .group=true}, {.id=u"R", .group=true},
            {.id=u"A", .parent=u"L"}, {.id=u"B", .parent=u"R"}};
        const std::array<FlowchartNodeBox, 4> sizes{{{}, {}, {180, 54}, {180, 54}}};
        const std::array<std::optional<FlowchartNodeBox>, 4> titles{
            FlowchartNodeBox{500, 150}, FlowchartNodeBox{90, 300}, std::nullopt, std::nullopt};
        for (const auto direction : {u"TB", u"BT", u"LR", u"RL"}) {
            graph.direction = direction;
            auto layout = layout_swimlanes(graph, sizes, {}, titles);
            expect(fatal(layout.has_value())) << (layout ? "" : layout.error());
            expect(fatal(layout->nodes.size() == 4_u));
            expect(disjoint(layout->nodes[0], layout->nodes[1]));
            for (std::size_t i = 0; i < 2; ++i) {
                const auto& lane = layout->nodes[i];
                const auto& node = layout->nodes[i + 2];
                expect(fatal(layout->title_boxes[i].has_value()));
                const auto& band = *layout->title_boxes[i];
                const auto& measured = *titles[i];
                FlowchartPlacedNode visible;
                if (graph.direction == u"LR") {
                    visible = {{}, band.x, lane.y, measured.height, measured.width};
                } else {
                    visible = {{}, lane.x, lane.y - lane.height / 2 + measured.height / 2,
                        measured.width, measured.height};
                }
                expect(contains(lane, node));
                expect(contains(lane, visible));
                expect(contains(band, visible));
                expect(disjoint(node, visible));
            }
        }
    };

    "mermaid_swimlane_math_title_measurements_are_checked"_test = [] {
        FlowchartInput graph;
        graph.nodes = {{.id=u"L", .group=true}, {.id=u"A", .parent=u"L"}};
        const std::array<FlowchartNodeBox, 2> sizes{{{}, {180, 54}}};
        std::array<std::optional<FlowchartNodeBox>, 2> titles{FlowchartNodeBox{80, 120}, std::nullopt};
        expect(layout_swimlanes(graph, sizes, {}, titles).has_value());
        expect(!layout_swimlanes(graph, sizes, {}, std::span{titles}.first(1)));
        titles[1] = FlowchartNodeBox{20, 20};
        expect(!layout_swimlanes(graph, sizes, {}, titles));
        titles[1].reset();
        titles[0]->height = std::numeric_limits<double>::infinity();
        expect(!layout_swimlanes(graph, sizes, {}, titles));
        titles[0]->height = -1;
        expect(!layout_swimlanes(graph, sizes, {}, titles));
    };

    "mermaid_swimlane_horizontal_geometry_uses_physical_box_spans"_test = [] {
        FlowchartInput graph;
        graph.nodes = {{.id=u"L", .group=true}, {.id=u"R", .group=true},
            {.id=u"A", .parent=u"L"}, {.id=u"B", .parent=u"L"},
            {.id=u"C", .parent=u"R"}};
        const std::array<FlowchartNodeBox, 5> sizes{{{}, {}, {180, 430}, {120, 360}, {180, 300}}};
        for (const auto direction : {u"LR", u"RL"}) {
            graph.direction = direction;
            auto layout = layout_swimlanes(graph, sizes, {});
            expect(fatal(layout.has_value())) << (layout ? "" : layout.error());
            expect(fatal(layout->nodes.size() == 5_u));
            expect(disjoint(layout->nodes[0], layout->nodes[1]));
            expect(contains(layout->nodes[0], layout->nodes[2]));
            expect(contains(layout->nodes[0], layout->nodes[3]));
            expect(contains(layout->nodes[1], layout->nodes[4]));
            for (std::size_t i = 2; i < 5; ++i)
                for (std::size_t j = i + 1; j < 5; ++j)
                    expect(disjoint(layout->nodes[i], layout->nodes[j]));

            const std::array<std::optional<FlowchartNodeBox>, 5> titles{
                FlowchartNodeBox{90, 300}, FlowchartNodeBox{200, 160},
                std::nullopt, std::nullopt, std::nullopt};
            auto headed = layout_swimlanes(graph, sizes, {}, titles);
            expect(fatal(headed.has_value())) << (headed ? "" : headed.error());
            expect(disjoint(headed->nodes[0], headed->nodes[1]));
            for (std::size_t i = 0; i < 2; ++i) {
                const auto& lane = headed->nodes[i];
                expect(fatal(headed->title_boxes[i].has_value()));
                const auto& band = *headed->title_boxes[i];
                const auto& measured = *titles[i];
                const FlowchartPlacedNode visible = graph.direction == u"LR"
                    ? FlowchartPlacedNode{{}, band.x, lane.y, measured.height, measured.width}
                    : FlowchartPlacedNode{{}, lane.x, lane.y - lane.height / 2 + measured.height / 2,
                        measured.width, measured.height};
                expect(contains(lane, visible));
                for (std::size_t j = 2; j < 5; ++j)
                    expect(disjoint(visible, headed->nodes[j]));
            }
        }
        // The main axis must likewise retain the physical widths when the
        // center transform leaves two consecutive wide boxes unrotated.
        graph.nodes = {{.id=u"L", .group=true},
            {.id=u"A", .parent=u"L"}, {.id=u"B", .parent=u"L"}};
        graph.edges = {{.id=u"e", .start=u"A", .end=u"B"}};
        const std::array<FlowchartNodeBox, 3> wide{{{}, {700, 50}, {600, 50}}};
        const std::array<FlowchartEdgeLabelSize, 1> labels{};
        for (const auto direction : {u"LR", u"RL"}) {
            graph.direction = direction;
            auto layout = layout_swimlanes(graph, wide, labels);
            expect(fatal(layout.has_value())) << (layout ? "" : layout.error());
            expect(disjoint(layout->nodes[1], layout->nodes[2]));
        }
    };

    "mermaid_swimlane_math_header_side_does_not_depend_on_aspect_ratio"_test = [] {
        SwimlaneRoutingNode lane;
        lane.group = true;
        lane.box = {{}, 200, 0, 500, 100};
        lane.title_box = FlowchartPlacedNode{{}, 50, 0, 200, 100};
        std::array nodes{lane};
        SwimlaneRoutingEdge crossing;
        crossing.placed.points = {{100, -40}, {100, 40}};
        const std::array edges{crossing};
        adjust_swimlane_title_bands(nodes, edges, u"LR");
        expect(fatal(nodes[0].title_box.has_value()));
        const auto& title = *nodes[0].title_box;
        expect(title.x + title.width / 2 < 100.0);
        expect(contains(nodes[0].box, title));
    };

    "mermaid_swimlane_math_provider_preserves_ordinary_svg"_test = [] {
        auto input = prepare(u"swimlane-beta LR\nsubgraph Lane\nA[Start]\nB[Finish]\nend\nA -->|next| B");
        input.lane_html_labels = true;
        const MathLabelMeasure declined = [](std::u16string_view, std::u16string_view,
            double, unsigned, bool) -> std::expected<std::optional<MathLabel>, std::string> {
            return std::nullopt;
        };
        auto with_math = render_swimlane_svg(input, plain_text(), declined);
        auto ordinary = render_swimlane_svg(input, plain_text());
        expect(fatal(with_math.has_value() && ordinary.has_value()));
        expect(with_math->svg == ordinary->svg);
        expect(with_math->width == ordinary->width && with_math->height == ordinary->height);
    };

    "mermaid_swimlane_math_errors_propagate_before_plain_measurement"_test = [] {
        auto input = prepare(source);
        const MathLabelMeasure failed = [](std::u16string_view, std::u16string_view,
            double, unsigned, bool) -> std::expected<std::optional<MathLabel>, std::string> {
            return std::unexpected("Math rendering failed");
        };
        auto result = render_swimlane_svg(input, reject_text(), failed);
        expect(!result && result.error() == "Math rendering failed");
    };
};
}
