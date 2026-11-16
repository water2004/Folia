#include "support/folia_test.hpp"
import folia.mermaid.math_label;
import folia.mermaid.render_entry;
import folia.mermaid.mindmap_svg;
import folia.mermaid.class_svg;
import folia.mermaid.state_svg;
import folia.mermaid.agentflow_svg;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

// These are supplied measurement fixtures, not estimated text widths. The
// compositor must use them verbatim; native MathJax has separate provider tests.
MathLabel formula_box(double width, double height) {
    MathLabelOptions options;
    options.line_height = 0;
    auto result = measure_math_label(u"$$x$$", options,
        [](std::u16string_view, unsigned, bool) -> std::expected<MathLabelTextMetrics, std::string> {
            return std::unexpected("A formula-only fixture must not measure text");
        },
        [=](std::u16string_view, double) -> std::expected<MathFragment, std::string> {
            return MathFragment{"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + std::to_string(width) +
                "\" height=\"" + std::to_string(height) + "\"><path data-math=\"fixture\" d=\"M0 0H1\"/></svg>",
                width, height, height + 2};
        });
    expect(fatal(result.has_value()));
    return std::move(*result);
}
FlowchartTextBox flow_box(double width, double height) {
    FlowchartTextBox box;
    box.width = width;
    box.height = height;
    box.math_label = std::make_shared<const MathLabel>(formula_box(width, height));
    return box;
}
SequenceTextBox sequence_box(double width, double height) {
    SequenceTextBox box;
    box.width = width;
    box.height = height;
    box.display = u"$$x$$";
    box.line_height = height;
    box.math_label = std::make_shared<const MathLabel>(formula_box(width, height));
    return box;
}
std::size_t occurrences(std::string_view text, std::string_view value) {
    std::size_t count = 0;
    for (auto at = text.find(value); at != text.npos; at = text.find(value, at + value.size())) ++count;
    return count;
}
double svg_attribute(std::string_view svg, std::string_view element, std::string_view attribute) {
    const auto start=svg.find(element);
    expect(fatal(start!=svg.npos));
    const auto end=svg.find('>',start);
    const auto key=std::string{attribute}+"=\"";
    const auto at=svg.find(key,start);
    expect(fatal(at!=svg.npos&&at<end));
    const auto first=at+key.size(),last=svg.find('"',first);
    double value=0;
    const auto parsed=std::from_chars(svg.data()+first,svg.data()+last,value);
    expect(fatal(parsed.ec==std::errc{}&&parsed.ptr==svg.data()+last));
    return value;
}

suite mathematical_labels = [] {
    "mermaid_math_measurement_copies_share_immutable_formula_data"_test = [] {
        FlowchartMeasurements flow;
        flow.nodes.push_back(flow_box(40, 24));
        const auto flow_copy = flow;
        expect(flow_copy.nodes.front().math_label.get() == flow.nodes.front().math_label.get());

        SequenceMeasurements sequence;
        sequence.messages.push_back(sequence_box(40, 24));
        const auto sequence_copy = sequence;
        expect(sequence_copy.messages.front().math_label.get() == sequence.messages.front().math_label.get());

        ClassMeasurements classes;
        ClassNodeMeasurements node;
        node.label.math_label = std::make_shared<const MathLabel>(formula_box(40, 24));
        classes.nodes.push_back(std::move(node));
        const auto classes_copy = classes;
        expect(classes_copy.nodes.front().label.math_label.get() == classes.nodes.front().label.math_label.get());
    };
    "mermaid_math_delimiters_follow_the_original_detector"_test = [] {
        expect(has_math_label(u"before $$x_i$$ after"));
        expect(has_math_label(u"$$$$"));
        expect(has_math_label(u"\\$$x$$"));
        expect(has_math_label(u"$$broken\n$$valid$$"));
        expect(!has_math_label(u"$x$"));
        expect(!has_math_label(u"$$unfinished"));
        expect(!has_math_label(u"$$x\ny$$"));
        expect(!has_math_label(u"$$x\ry$$"));
        expect(!has_math_label(u"$$x\u2028y$$"));
        expect(!has_math_label(u"$$x\u2029y$$"));
    };
    "mermaid_math_keeps_tex_out_of_markdown_and_newline_conversion"_test = [] {
        std::vector<std::u16string> formulas, words;
        MathLabelOptions options;
        const auto text = [&](std::u16string_view value, unsigned, bool)
            -> std::expected<MathLabelTextMetrics, std::string> {
            words.emplace_back(value);
            return MathLabelTextMetrics{31, 15, 11};
        };
        const auto math = [&](std::u16string_view value, double size)
            -> std::expected<MathFragment, std::string> {
            expect(size == 16.0);
            formulas.emplace_back(value);
            return MathFragment{"<svg/>", 20, 30, 25};
        };
        auto result = measure_math_label(u"**literal** $$\\nabla_i^2 + \\nu$$ $$a*b_c$$", options, text, math);
        expect(fatal(result.has_value()));
        expect(fatal(formulas.size() == 2_u));
        expect(bool(formulas[0] == u"\\nabla_i^2 + \\nu"));
        expect(bool(formulas[1] == u"a*b_c"));
        expect(fatal(words.size() == 1_u));
        expect(bool(words[0] == u"**literal**"));
        formulas.clear();
        const auto matrix = uR"($$\begin{matrix}a\\b\end{matrix}$$)";
        expect(measure_math_label(matrix, options, text, math).has_value());
        expect(bool(formulas.back() == uR"(\begin{matrix}a\b\end{matrix})"));
        options.parse_policy = MathLabelParsePolicy::sequence;
        expect(measure_math_label(matrix, options, text, math).has_value());
        expect(bool(formulas.back() == uR"(\begin{matrix}a\\b\end{matrix})"));
    };
    "mermaid_math_composes_multiple_formulas_and_styled_lines_from_actual_metrics"_test = [] {
        std::vector<std::tuple<std::u16string, unsigned, bool>> requests;
        const MathLabelTextMeasure text = [&](std::u16string_view value, unsigned weight, bool italic)
            -> std::expected<MathLabelTextMetrics, std::string> {
            requests.emplace_back(value, weight, italic);
            if (value == u"left") return MathLabelTextMetrics{30, 12, 9};
            if (value == u"bold") return MathLabelTextMetrics{40, 14, 10};
            if (value == u"tail") return MathLabelTextMetrics{10, 10, 7};
            return std::unexpected("Unexpected text in measurement fixture");
        };
        const MathLabelRender math = [](std::u16string_view value, double)
            -> std::expected<MathFragment, std::string> {
            if (value == u"\\frac{1}{2}") return MathFragment{"<svg data-math=\"fraction\"/>", 20, 50, 38};
            if (value == u"x") return MathFragment{"<svg data-math=\"x\"/>", 80, 20, 17};
            return std::unexpected("Unexpected TeX in measurement fixture");
        };
        auto result = measure_math_label(u"left $$\\frac{1}{2}$$ <b>bold</b><BR />$$x$$ <i>tail</i>", {}, text, math);
        expect(fatal(result.has_value())) << (result ? "" : result.error());
        expect(result->width == 90.0);
        expect(result->height == 74.0);
        expect(fatal(result->lines.size() == 2_u));
        const auto& first = result->lines[0];
        expect(fatal(first.atoms.size() == 3_u));
        expect(first.atoms[0].y == 19.0);
        expect(first.atoms[1].x == 30.0);
        expect(first.atoms[1].y == 0.0);
        expect(first.atoms[2].x == 50.0);
        expect(first.atoms[2].weight == 700_u);
        expect(result->lines[1].y == 50.0);
        expect(result->lines[1].atoms[1].italic);
        expect(requests.size() == 3_u);
        auto svg = render_math_label_svg(*result, 5, 7, {u"A&B", "#abcdef", "node-label"});
        expect(fatal(svg.has_value()));
        expect(svg->find("font-family=\"A&amp;B\"") != svg->npos);
        expect(svg->find("color=\"#abcdef\"") != svg->npos);
        expect(svg->find("transform=\"translate(35,7)\"") != svg->npos);
        expect(occurrences(*svg, "<svg ") == 2_u);
        expect(svg->find("$$") == svg->npos);
        expect(svg->find("font-weight=\"700\"") != svg->npos);
        expect(svg->find("font-style=\"italic\"") != svg->npos);
    };
    "mermaid_math_uses_real_style_metrics_and_decodes_text_entities_once"_test = [] {
        MathLabelOptions options;
        options.font_weight = 600;
        bool measured = false;
        auto result = measure_math_label(u"<b><i>A&amp;B&lt;</i></b>$$x$$", options,
            [&](std::u16string_view value, unsigned weight, bool italic)
                -> std::expected<MathLabelTextMetrics, std::string> {
                expect(bool(value == u"A&B<"));
                expect(weight == 900_u);
                expect(italic);
                measured = true;
                return MathLabelTextMetrics{44, 18, 13};
            },
            [](std::u16string_view, double) -> std::expected<MathFragment, std::string> {
                return MathFragment{"<svg/>", 10, 12, -2};
            });
        expect(fatal(result.has_value()));
        expect(measured);
        auto svg = render_math_label_svg(*result, 0, 0, {u"Arial", "black", "label"});
        expect(fatal(svg.has_value()));
        expect(svg->find("A&amp;B&lt;") != svg->npos);
        expect(svg->find("&amp;amp;") == svg->npos);
    };
    "mermaid_math_preserves_inline_spaces_inside_nested_emphasis"_test = [] {
        std::vector<std::u16string> requests;
        auto label = measure_math_label(u"<b>bold <i>both</i> again</b> $$x$$", {},
            [&](std::u16string_view text, unsigned, bool)
                -> std::expected<MathLabelTextMetrics, std::string> {
                requests.emplace_back(text);
                return MathLabelTextMetrics{24, 16, 12};
            },
            [](std::u16string_view, double) -> std::expected<MathFragment, std::string> {
                return MathFragment{"<svg/>", 12, 20, 16};
            });
        expect(fatal(label.has_value()));
        expect(fatal(requests.size() == 3_u));
        expect(bool(requests[0] == u"bold "));
        expect(bool(requests[1] == u"both"));
        expect(bool(requests[2] == u" again"));
    };
    "mermaid_math_propagates_provider_errors_and_rejects_invalid_geometry"_test = [] {
        const MathLabelTextMeasure text = [](std::u16string_view, unsigned, bool)
            -> std::expected<MathLabelTextMetrics, std::string> { return MathLabelTextMetrics{10, 14, 11}; };
        const MathLabelRender failure = [](std::u16string_view, double)
            -> std::expected<MathFragment, std::string> { return std::unexpected("invalid TeX"); };
        auto error = measure_math_label(u"$$invalid$$", {}, text, failure);
        expect(!error);
        expect(bool(error.error() == "invalid TeX"));
        const MathLabelRender invalid = [](std::u16string_view, double)
            -> std::expected<MathFragment, std::string> {
            return MathFragment{"<svg/>", std::numeric_limits<double>::infinity(), 12, 10};
        };
        expect(!measure_math_label(u"$$x$$", {}, text, invalid));
        expect(!measure_math_label(u"$$x$$", {}, {}, failure));
        auto label = formula_box(20, 12);
        expect(!render_math_label_svg(label, std::numeric_limits<double>::quiet_NaN(), 0, {}));
        auto compact = render_math_label_svg(label, 10, 5, {u"Arial", "black", "label", 40});
        expect(fatal(compact.has_value()));
        expect(compact->find("transform=\"translate(20,5)\"") != compact->npos);
    };
    "mermaid_math_dimensions_reach_flowchart_layout_before_svg_paint"_test = [] {
        auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        auto prepared = prepare_native_diagram(u"flowchart TD\nA[\"$$x$$\"]", *session);
        expect(fatal(prepared.has_value())) << (prepared ? "" : prepared.error());
        const auto& input = std::get<FlowchartRenderInput>(prepared->content);
        FlowchartMeasurements small, large;
        small.nodes.push_back(flow_box(40, 20));
        large.nodes.push_back(flow_box(240, 96));
        auto a = render_flowchart_svg(input, small), b = render_flowchart_svg(input, large);
        expect(fatal(a.has_value())) << (a ? "" : a.error());
        expect(fatal(b.has_value())) << (b ? "" : b.error());
        expect(b->width > a->width + 100);
        expect(b->height > a->height + 50);
        expect(occurrences(b->svg, "data-math=\"fixture\"") == 1_u);
        expect(b->svg.find("$$x$$") == b->svg.npos);
    };
    "mermaid_math_dimensions_reach_sequence_spacing_and_all_label_painters"_test = [] {
        auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        auto prepared = prepare_native_diagram(u"---\nconfig: {look: classic}\n---\nsequenceDiagram\nbox $$x$$\nparticipant A as $$x$$\nparticipant B\nend\nalt $$x$$\nA->>B: $$x$$\nelse $$x$$\nNote over A,B: $$x$$\nend", *session);
        expect(prepared.has_value()) << (prepared ? "" : prepared.error());
        expect(fatal(prepared.has_value()));
        const auto& input = std::get<SequenceRenderInput>(prepared->content);
        const auto measured = [&](double width, double height) {
            SequenceMeasurements boxes;
            for (const auto& actor : input.document.actors) {
                if (has_math_label(actor.description)) boxes.actors.push_back(sequence_box(width, height));
                else boxes.actors.push_back({12, 16, actor.description, {actor.description}, 16});
            }
            for (const auto& message : input.document.messages) {
                if (has_math_label(message.message)) boxes.messages.push_back(sequence_box(width, height));
                else boxes.messages.emplace_back();
            }
            boxes.boxes.resize(input.document.boxes.size(), sequence_box(width, height));
            boxes.wrap = [=](std::u16string_view, double, unsigned)
                -> std::expected<SequenceTextBox, std::string> { return sequence_box(width, height); };
            return boxes;
        };
        auto a = render_sequence_svg(input, measured(40, 24));
        auto b = render_sequence_svg(input, measured(320, 96));
        expect(fatal(a.has_value())) << (a ? "" : a.error());
        expect(fatal(b.has_value())) << (b ? "" : b.error());
        expect(b->width > a->width);
        expect(b->height > a->height + 100);
        // Header/footer participant, box, region/section titles, signal, note.
        expect(occurrences(b->svg, "data-math=\"fixture\"") == 7_u);
        expect(b->svg.find("$$x$$") == b->svg.npos);
    };
    "mermaid_math_payload_is_shared_by_flowchart_nodes_edges_and_clusters"_test = [] {
        auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        auto prepared = prepare_native_diagram(u"flowchart TD\nsubgraph G[\"$$g$$\"]\nA[\"$$a$$\"] -->|\"$$e$$\"| B[\"$$b$$\"]\nend\nstyle G color:#d81b60", *session);
        expect(fatal(prepared.has_value())) << (prepared ? "" : prepared.error());
        const auto& input = std::get<FlowchartRenderInput>(prepared->content);
        FlowchartMeasurements boxes;
        boxes.nodes.resize(input.document.nodes.size(), flow_box(40, 24));
        boxes.edges.resize(input.document.edges.size(), flow_box(40, 24));
        auto svg = render_flowchart_svg(input, boxes);
        expect(fatal(svg.has_value())) << (svg ? "" : svg.error());
        expect(occurrences(svg->svg, "data-math=\"fixture\"") == 4_u);
        expect(svg->svg.find("class=\"cluster-label\" color=\"#d81b60\"") != svg->svg.npos);
        expect(svg->svg.find("$$") == svg->svg.npos);
    };
    "mermaid_math_dimensions_reach_mindmap_shapes"_test = [] {
        auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        auto prepared = prepare_mindmap_render(u"---\nconfig: {look: classic}\n---\nmindmap\n  root(($$x$$))", *session);
        expect(prepared.has_value()) << (prepared ? "" : prepared.error());
        expect(fatal(prepared.has_value()));
        MindmapMeasurements small, large;
        small.nodes.push_back(flow_box(40, 24));
        large.nodes.push_back(flow_box(240, 96));
        auto a = render_mindmap_svg(*prepared, small), b = render_mindmap_svg(*prepared, large);
        expect(fatal(a.has_value())) << (a ? "" : a.error());
        expect(fatal(b.has_value())) << (b ? "" : b.error());
        expect(b->width > a->width + 100);
        expect(b->height > a->height + 100);
        expect(occurrences(b->svg, "data-math=\"fixture\"") == 1_u);
    };
    "mermaid_math_class_payload_preserves_node_geometry_with_svg_labels_disabled"_test = [] {
        auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        auto prepared = prepare_native_diagram(u"---\nconfig: {look: classic, layout: dagre, htmlLabels: false}\n---\nclassDiagram\nclass A[\"$$x$$\"]", *session);
        expect(prepared.has_value()) << (prepared ? "" : prepared.error());
        expect(fatal(prepared.has_value()));
        const auto& input = std::get<ClassRenderInput>(prepared->content);
        expect(!input.config.html_labels);
        const auto measured = [](double width, double height) {
            ClassMeasurements boxes;
            ClassNodeMeasurements node;
            node.label.text = u"$$x$$";
            node.label.width = width;
            node.label.height = height;
            node.label.line_step = height;
            node.label.math_label = std::make_shared<const MathLabel>(formula_box(width, height));
            boxes.nodes.push_back(std::move(node));
            return boxes;
        };
        auto a = render_class_svg(input, measured(40, 24));
        auto b = render_class_svg(input, measured(240, 96));
        expect(a.has_value()) << (a ? "" : a.error());
        expect(b.has_value()) << (b ? "" : b.error());
        expect(fatal(a.has_value() && b.has_value()));
        expect(b->width > a->width + 100);
        expect(b->height > a->height + 50);
        expect(occurrences(b->svg, "data-math=\"fixture\"") == 1_u);
        expect(b->svg.find("$$") == b->svg.npos);
    };
    "mermaid_math_state_callback_measures_nodes_edges_and_notes_before_layout"_test = [] {
        auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        auto input = prepare_state(uR"(---
config: {look: classic, layout: dagre}
---
stateDiagram-v2
A: $$\frac{x}{y}$$
A --> A : $$y_{n+1}$$
note right of A : $$\sqrt{z}$$)", *session);
        expect(input.has_value()) << (input ? "" : input.error());
        expect(fatal(input.has_value()));
        const StateMeasure no_plain = [](std::u16string_view, double)
            -> std::expected<StateTextBox, std::string> { return std::unexpected("Math must bypass ordinary text measurement"); };
        const auto callback = [](double width, double height) -> MathLabelMeasure {
            return [=](std::u16string_view text, std::u16string_view family, double size, unsigned weight, bool italic)
                -> std::expected<std::optional<MathLabel>, std::string> {
                expect(has_math_label(text));
                expect(!family.empty() && size > 0);
                expect(weight == 400_u && !italic);
                return formula_box(width, height);
            };
        };
        auto a = render_state_svg(*input, no_plain, callback(40, 24));
        auto b = render_state_svg(*input, no_plain, callback(240, 96));
        expect(a.has_value()) << (a ? "" : a.error());
        expect(b.has_value()) << (b ? "" : b.error());
        expect(fatal(a.has_value() && b.has_value()));
        expect(b->width > a->width);
        expect(b->height > a->height);
        expect(occurrences(b->svg, "data-math=\"fixture\"") == 3_u);
        expect(b->svg.find("$$") == b->svg.npos);
        auto disabled = *input;
        disabled.html_labels = false;
        const StateMeasure plain = [](std::u16string_view, double)
            -> std::expected<StateTextBox, std::string> { return StateTextBox{40, 40, 16, 12, 16}; };
        const MathLabelMeasure rejected = [](std::u16string_view, std::u16string_view, double, unsigned, bool)
            -> std::expected<std::optional<MathLabel>, std::string> { return std::unexpected("math disabled"); };
        auto fallback = render_state_svg(disabled, plain, rejected);
        auto original = render_state_svg(disabled, plain);
        expect(fatal(fallback.has_value() && original.has_value()));
        expect(bool(fallback->svg == original->svg));
        expect(!render_state_svg(*input, plain, rejected));
    };
    "mermaid_math_agentflow_callback_reuses_the_flowchart_label_path"_test = [] {
        auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        auto prepared = prepare_native_diagram(u"---\nconfig: {look: classic}\n---\nagentflow-beta TB\nflow G[\"$$g$$\"]\nA[\"$$a$$\"] -->|$$e$$| B[\"$$b$$\"]\nend", *session);
        expect(prepared.has_value()) << (prepared ? "" : prepared.error());
        expect(fatal(prepared.has_value()));
        const auto& input = std::get<AgentflowRenderInput>(prepared->content);
        const SvgTextMeasure no_plain = [](std::u16string_view, double, unsigned)
            -> std::expected<SvgTextMetrics, std::string> { return std::unexpected("Math must bypass ordinary text measurement"); };
        const auto callback = [](double width, double height) -> MathLabelMeasure {
            return [=](std::u16string_view text, std::u16string_view family, double size, unsigned weight, bool italic)
                -> std::expected<std::optional<MathLabel>, std::string> {
                expect(has_math_label(text));
                expect(!family.empty() && size > 0);
                expect(weight == 400_u && !italic);
                return formula_box(width, height);
            };
        };
        auto a = render_agentflow_svg(input, no_plain, callback(40, 24));
        auto b = render_agentflow_svg(input, no_plain, callback(240, 96));
        expect(a.has_value()) << (a ? "" : a.error());
        expect(b.has_value()) << (b ? "" : b.error());
        expect(fatal(a.has_value() && b.has_value()));
        expect(b->width > a->width);
        expect(b->height > a->height);
        expect(occurrences(b->svg, "data-math=\"fixture\"") == 4_u);
        expect(b->svg.find("$$") == b->svg.npos);
    };
    "mermaid_math_sequence_wrap_keeps_formula_boxes_inside_actors_and_notes"_test = [] {
        auto session=ConfigSession::create();expect(fatal(session.has_value()));
        auto prepared=prepare_native_diagram(u"---\nconfig: {look: classic}\n---\nsequenceDiagram\nparticipant A as wrap:$$x$$\nparticipant B as $$x$$\nA->>A: wrap:$$x$$\nNote over A: wrap:$$x$$",*session);
        expect(prepared.has_value())<<(prepared?"":prepared.error());expect(fatal(prepared.has_value()));
        const auto& input=std::get<SequenceRenderInput>(prepared->content);
        expect(input.document.actors[0].wrap&&!input.document.actors[1].wrap);
        expect(input.document.messages[0].wrap&&input.document.messages[1].wrap);
        SequenceMeasurements boxes;
        boxes.actors.resize(input.document.actors.size(),sequence_box(360,120));
        boxes.messages.resize(input.document.messages.size(),sequence_box(360,120));
        boxes.wrap=[](std::u16string_view,double,unsigned)->std::expected<SequenceTextBox,std::string>{return sequence_box(360,120);};
        auto svg=render_sequence_svg(input,boxes);
        expect(svg.has_value())<<(svg?"":svg.error());expect(fatal(svg.has_value()));
        const auto actor_a="<rect class=\"actor actor-top\" name=\"A\"";
        const auto actor_b="<rect class=\"actor actor-top\" name=\"B\"";
        expect(svg_attribute(svg->svg,actor_a,"width")>=380);
        expect(svg_attribute(svg->svg,actor_a,"height")>=140);
        expect(svg_attribute(svg->svg,actor_b,"height")>=140);
        expect(svg_attribute(svg->svg,"<rect class=\"note\"","width")>=380);
        expect(svg_attribute(svg->svg,"<rect class=\"note\"","height")>=140);
        expect(occurrences(svg->svg,"data-math=\"fixture\"")==6_u);
    };
    "mermaid_math_sequence_loop_expands_for_an_unbreakable_formula_title"_test = [] {
        auto session=ConfigSession::create();expect(fatal(session.has_value()));
        auto prepared=prepare_native_diagram(u"---\nconfig: {look: classic}\n---\nsequenceDiagram\nparticipant A\nparticipant B\nloop wrap:$$x$$\nA->>B: plain\nend",*session);
        expect(prepared.has_value())<<(prepared?"":prepared.error());expect(fatal(prepared.has_value()));
        const auto& input=std::get<SequenceRenderInput>(prepared->content);
        SequenceMeasurements boxes;
        for(const auto& actor:input.document.actors) boxes.actors.push_back({12,16,actor.description,{actor.description},16});
        for(const auto& message:input.document.messages) {
            if(has_math_label(message.message)) boxes.messages.push_back(sequence_box(600,120));
            else if(!message.message.empty()) boxes.messages.push_back({30,16,message.message,{message.message},16});
            else boxes.messages.emplace_back();
        }
        boxes.wrap=[](std::u16string_view,double,unsigned)->std::expected<SequenceTextBox,std::string>{return sequence_box(600,120);};
        auto svg=render_sequence_svg(input,boxes);
        expect(svg.has_value())<<(svg?"":svg.error());expect(fatal(svg.has_value()));
        const auto left=svg_attribute(svg->svg,"<line class=\"loopLine\"","x1");
        const auto right=svg_attribute(svg->svg,"<line class=\"loopLine\"","x2");
        expect(right-left>=620);
        expect(svg->width>=right-left);
    };
    "mermaid_math_sequence_symbol_actor_places_tall_labels_below_the_body"_test = [] {
        auto session=ConfigSession::create();expect(fatal(session.has_value()));
        auto prepared=prepare_native_diagram(u"---\nconfig: {look: classic}\n---\nsequenceDiagram\nactor A as $$x$$\nA->>A: plain",*session);
        expect(prepared.has_value())<<(prepared?"":prepared.error());expect(fatal(prepared.has_value()));
        const auto& input=std::get<SequenceRenderInput>(prepared->content);
        SequenceMeasurements boxes;
        boxes.actors.push_back(sequence_box(80,120));
        boxes.messages.push_back({30,16,u"plain",{u"plain"},16});
        auto svg=render_sequence_svg(input,boxes);
        expect(svg.has_value())<<(svg?"":svg.error());expect(fatal(svg.has_value()));
        expect(svg_attribute(svg->svg,"<line class=\"actor-line\"","y1")>=180);
        expect(svg->svg.find("transform=\"translate(35,65)\"")!=svg->svg.npos);
        expect(occurrences(svg->svg,"data-math=\"fixture\"")==2_u);
    };
};
}
