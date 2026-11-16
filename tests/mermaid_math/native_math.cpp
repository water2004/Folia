#include "support/folia_test.hpp"
import folia.platform.mermaid_math;
import folia.mermaid.math_label;
import folia.mermaid.render_entry;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

ConfigSession session_for_math() {
    auto session=ConfigSession::create();
    expect(fatal(session.has_value()));
    ConfigValue site{.kind=ConfigKind::mapping};
    site.mapping.emplace(u"look",ConfigValue{.kind=ConfigKind::string,.string=u"classic"});
    expect(fatal(session->initialize(site).has_value()));
    return std::move(*session);
}

// These integration fixtures contain only math atoms. Refuse any request for
// ordinary text metrics, so a portable test cannot conceal a font estimate.
MathLabel math_only(folia::platform::MermaidMathRenderer& renderer,
    std::u16string_view text, double size = 16,
    MathLabelParsePolicy policy = MathLabelParsePolicy::create_text) {
    auto label = measure_math_label(text,
        {.font_size=size,.line_height=size*1.5,.parse_policy=policy},
        [](std::u16string_view,unsigned,bool)->std::expected<MathLabelTextMetrics,std::string> {
            return std::unexpected("This fixture requires real math atoms only");
        },[&](std::u16string_view tex,double font_size) { return renderer.render(tex,font_size); });
    expect(label.has_value()) << (label ? "" : label.error());
    expect(fatal(label.has_value()));
    return std::move(*label);
}

void save_svg(std::string_view name, std::string_view svg) {
    if (const auto* directory = std::getenv("FOLIA_MERMAID_MATH_SVG_OUTPUT")) {
        const auto output = std::filesystem::path{directory};
        std::filesystem::create_directories(output);
        std::ofstream file{output / (std::string{name} + ".svg"),std::ios::binary};
        expect(fatal(file.is_open()));
        file << svg;
        expect(file.good());
    }
}

suite native_math = [] {
    "mermaid_native_math_bridge_preserves_resolved_size_and_svg"_test = [] {
        folia::platform::MermaidMathRenderer renderer;
        auto small = renderer.render(uR"(\frac{\nabla f}{\sqrt{x_1^2+1}})",16);
        auto large = renderer.render(uR"(\frac{\nabla f}{\sqrt{x_1^2+1}})",32);
        expect(fatal(small.has_value())) << (small ? "" : small.error());
        expect(fatal(large.has_value())) << (large ? "" : large.error());
        expect(small->width > 0 && small->height > 0);
        expect(std::abs(large->width-small->width*2) < 1e-8);
        expect(std::abs(large->height-small->height*2) < 1e-8);
        expect(std::abs(large->baseline-small->baseline*2) < 1e-8);
        expect(small->svg.starts_with("<svg"));
        expect(small->svg.find("<mjx-container") == std::string::npos);
        expect(small->svg.find("<foreignObject") == std::string::npos);
        expect(small->svg.find("<path") != std::string::npos);
        expect(!renderer.render(u"x",0));
        save_svg("native-fragment",small->svg);
    };

    "mermaid_native_math_composes_multiple_formulas_without_reparsing_svg"_test = [] {
        folia::platform::MermaidMathRenderer renderer;
        auto label = math_only(renderer,uR"($$\nabla f$$$$\frac{a}{b}$$<br/>$$\sqrt{x_1^2+1}$$)");
        expect(fatal(label.lines.size() == 2_u));
        expect(label.lines[0].atoms.size() == 2_u);
        expect(label.lines[1].atoms.size() == 1_u);
        expect(std::abs(label.lines[0].width-
            label.lines[0].atoms[0].width-label.lines[0].atoms[1].width) < 1e-8);
        expect(std::abs(label.height-label.lines[0].height-label.lines[1].height) < 1e-8);
        auto svg = render_math_label_svg(label,0,0,{u"Arial","#ffffff","label"});
        expect(fatal(svg.has_value()));
        expect(svg->find("color=\"#ffffff\"") != std::string::npos);
        expect(svg->find("<foreignObject") == std::string::npos);
        expect(svg->find("$$") == std::string::npos);
    };

    "mermaid_native_math_dimensions_feed_both_flowchart_layout_engines"_test = [] {
        for (std::u16string_view engine : {u"dagre",u"elk"}) {
            auto session = session_for_math();
            const auto source = u"---\nconfig:\n  layout: " + std::u16string{engine} +
                u"\n---\nflowchart TD\nA[\"$$\\frac{1}{1+\\frac{1}{x}}$$\"] -->|$$x_1$$| B[\"$$\\nabla f$$\"]";
            auto input = prepare_native_diagram(source,session);
            expect(fatal(input.has_value())) << (input ? "" : input.error());
            const auto& flow = std::get<FlowchartRenderInput>(input->content);
            folia::platform::MermaidMathRenderer renderer;
            const auto measure = [&](std::u16string_view source) {
                auto label = math_only(renderer,source,flow.theme.font_size);
                FlowchartTextBox box{.width=label.width,.height=label.height};
                box.math_label=std::make_shared<const MathLabel>(std::move(label));
                return box;
            };
            FlowchartMeasurements measurements;
            for (const auto& node : flow.document.nodes) measurements.nodes.push_back(measure(node.label));
            for (const auto& edge : flow.document.edges) measurements.edges.push_back(measure(edge.label));
            expect(measurements.nodes[0].height > measurements.nodes[1].height);
            auto svg = render_native_diagram(*input,{},measurements);
            expect(fatal(svg.has_value())) << (svg ? "" : svg.error());
            expect(svg->width > measurements.nodes[0].width);
            expect(svg->height > measurements.nodes[0].height+measurements.nodes[1].height);
            expect(svg->svg.find("<foreignObject") == std::string::npos);
            expect(svg->svg.find("$$") == std::string::npos);
            save_svg(engine == u"dagre" ? "native-flowchart-dagre" : "native-flowchart-elk",svg->svg);
        }
    };

    "mermaid_native_math_sequence_uses_formula_actor_and_message_boxes"_test = [] {
        auto session = session_for_math();
        auto input = prepare_native_diagram(uR"(sequenceDiagram
participant A as $$x_1$$
participant B as $$\nabla f$$
A->>B: $$\frac{a}{b}$$
Note over A,B: $$\sum_{k=1}^{n} k$$
)",session);
        expect(fatal(input.has_value())) << (input ? "" : input.error());
        const auto& sequence = std::get<SequenceRenderInput>(input->content);
        folia::platform::MermaidMathRenderer renderer;
        const auto measure = [&](std::u16string_view source) {
            auto label = math_only(renderer,source,sequence.theme.font_size,MathLabelParsePolicy::sequence);
            SequenceTextBox box{.width=label.width,.height=label.height,.display=std::u16string{source}};
            box.math_label=std::make_shared<const MathLabel>(std::move(label));
            return box;
        };
        SequenceMeasurements measurements;
        for (const auto& actor : sequence.document.actors)
            measurements.actors.push_back(measure(actor.description));
        for (const auto& message : sequence.document.messages)
            measurements.messages.push_back(measure(message.message));
        measurements.wrap=[&](std::u16string_view source,double,unsigned)
            ->std::expected<SequenceTextBox,std::string> { return measure(source); };
        auto svg = render_native_diagram(*input,{}, {},measurements);
        expect(fatal(svg.has_value())) << (svg ? "" : svg.error());
        expect(svg->width > 0 && svg->height > 0);
        expect(svg->svg.find("<foreignObject") == std::string::npos);
        expect(svg->svg.find("$$") == std::string::npos);
        save_svg("native-sequence",svg->svg);
    };

    "mermaid_native_math_shared_callbacks_size_shapes_and_tables"_test = [] {
        struct Fixture { std::string_view name; std::u16string_view source; };
        constexpr Fixture fixtures[] = {
            {"native-block",uR"(block-beta
columns 2
A["$$\frac{1}{1+\frac{1}{x}}$$"] B{"$$\nabla f$$"}
A -- "$$x_1$$" --> B)"},
            {"native-state",uR"(stateDiagram-v2
state "$$\frac{a}{b}$$" as A
state "$$\nabla f$$" as B
A --> B: $$x_1$$)"},
            {"native-agentflow",uR"(agentflow-beta TB
flow G["$$\mathbb{R}$$"]
A["$$\frac{a}{b}$$"] -->|$$x_1$$| B["$$\nabla f$$"]
end)"},
            {"native-er",uR"(erDiagram
A["$$\frac{a}{b}$$"]
B["$$\nabla f$$"]
A ||--o{ B : $$x_1$$)"},
            {"native-usecase",uR"(usecase-beta
actor A("$$x^2$$")
U("$$\frac{a}{b}$$")
A -- "$$n$$" --> U)"},
            {"native-mindmap",uR"(mindmap
  root(("$$\frac{a}{b}$$"))
    $$\nabla f$$)"},
            {"native-class",uR"(classDiagram
class A["$$\mathbb{R}$$"]
class B["$$\frac{a}{b}$$"]
A --> B : $$x_1$$)"},
            {"native-swimlane",uR"(---
config:
  flowchart:
    htmlLabels: true
---
swimlane-beta TB
subgraph L["$$\frac{a}{b}$$"]
A["$$x^2$$"]
B["$$y$$"]
end
A -->|"$$\sqrt{x}$$"| B)"},
        };
        for (const auto& fixture : fixtures) {
            auto session = session_for_math();
            auto input = prepare_native_diagram(fixture.source,session);
            expect(input.has_value()) << fixture.name << (input ? "" : input.error());
            expect(fatal(input.has_value()));
            folia::platform::MermaidMathRenderer renderer;
            const MathLabelMeasure rich = [&](std::u16string_view source,std::u16string_view,
                double size,unsigned,bool)->std::expected<std::optional<MathLabel>,std::string> {
                if (!has_math_label(source)) return std::nullopt;
                return math_only(renderer,source,size);
            };
            const auto empty_only = []<class Metrics>(std::u16string_view text)
                ->std::expected<Metrics,std::string> {
                if (!text.empty()) return std::unexpected("Unexpected ordinary text in math fixture");
                return Metrics{};
            };
            const auto result = std::visit([&](const auto& content)
                ->std::expected<DiagramSvg,std::string> {
                const auto convert = [](auto result)->std::expected<DiagramSvg,std::string> {
                    if (!result) return std::unexpected(result.error());
                    return DiagramSvg{std::move(result->svg),result->width,result->height};
                };
                using Input = std::decay_t<decltype(content)>;
                if constexpr (std::same_as<Input,BlockRenderInput>) {
                    return convert(render_block_svg(content,[&](std::u16string_view text,double) {
                        return empty_only.template operator()<BlockTextBox>(text);
                    },rich));
                } else if constexpr (std::same_as<Input,StateRenderInput>) {
                    return convert(render_state_svg(content,[&](std::u16string_view text,double) {
                        return empty_only.template operator()<StateTextBox>(text);
                    },rich));
                } else if constexpr (std::same_as<Input,AgentflowRenderInput>) {
                    return convert(render_agentflow_svg(content,[&](std::u16string_view text,double,unsigned) {
                        return empty_only.template operator()<SvgTextMetrics>(text);
                    },rich));
                } else if constexpr (std::same_as<Input,ErRenderInput>) {
                    return convert(render_er_svg(content,[&](std::u16string_view text,std::u16string_view,double,unsigned,bool) {
                        return empty_only.template operator()<ErTextMetrics>(text);
                    },rich));
                } else if constexpr (std::same_as<Input,UsecaseRenderInput>) {
                    return convert(render_usecase_svg(content,[&](std::u16string_view text,
                        std::u16string_view,double,unsigned,const SvgFamilyTextOptions&) {
                        return empty_only.template operator()<SvgTextMetrics>(text);
                    },rich));
                } else if constexpr (std::same_as<Input,SwimlaneRenderInput>) {
                    return convert(render_swimlane_svg(content,[&](std::u16string_view text,
                        std::u16string_view,double,unsigned,const SvgFamilyTextOptions&) {
                        return empty_only.template operator()<SvgTextMetrics>(text);
                    },rich));
                } else if constexpr (std::same_as<Input,MindmapRenderInput>) {
                    MindmapMeasurements measurements;
                    for (const auto& node : content.document.nodes) {
                        auto label = math_only(renderer,node.label,content.theme.font_size);
                        FlowchartTextBox box{.width=label.width,.height=label.height};
                        box.math_label = std::make_shared<const MathLabel>(std::move(label));
                        measurements.nodes.push_back(std::move(box));
                    }
                    return convert(render_mindmap_svg(content,measurements));
                } else if constexpr (std::same_as<Input,ClassRenderInput>) {
                    const auto measure = [&](std::u16string_view text) {
                        auto label = math_only(renderer,text,content.theme.font_size);
                        ClassTextBox box{.text=std::u16string{text},.width=label.width,.height=label.height};
                        box.line_step = label.lines.front().height;
                        box.math_label = std::make_shared<const MathLabel>(std::move(label));
                        return box;
                    };
                    ClassMeasurements measurements;
                    for (const auto& node : content.document.classes)
                        measurements.nodes.push_back({.label=measure(node.label)});
                    for (const auto& edge : content.document.relations)
                        measurements.edges.push_back({.label=measure(edge.title)});
                    return convert(render_class_svg(content,measurements));
                } else {
                    return std::unexpected("Unexpected diagram family in math fixture");
                }
            },input->content);
            expect(result.has_value()) << fixture.name << (result ? "" : result.error());
            expect(fatal(result.has_value()));
            expect(result->width > 0 && result->height > 0);
            expect(result->svg.find("$$") == std::string::npos);
            expect(result->svg.find("<foreignObject") == std::string::npos);
            expect(result->svg.find("<mjx-container") == std::string::npos);
            expect(result->svg.find("data-mml-node") != std::string::npos);
            save_svg(fixture.name,result->svg);
        }
    };
};
}
