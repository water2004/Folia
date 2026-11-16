#include "support/folia_test.hpp"
import folia.mermaid.measured_render;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

// This provider deliberately cannot shape text. Fixed symbols must render
// without invoking it; it supplies no substitute measurements or font widths.
class RejectSymbolText final:public TextMeasurer {
public:
    mutable std::size_t measurements=0;
    std::expected<std::u16string,std::string> resolve_family(
        std::u16string_view family,MermaidFontContext) const override {return std::u16string(family);}
    std::expected<MermaidTextMetrics,std::string> measure(std::u16string_view,
        const MermaidFont&,bool,std::span<const MermaidTextStyle>,bool) const override {
        ++measurements;return std::unexpected("A symbol attempted to measure hidden text");
    }
    std::expected<std::vector<std::u16string>,std::string> wrap(std::u16string_view,
        const MermaidFont&,double,std::span<const MermaidTextStyle>,bool) const override {
        ++measurements;return std::unexpected("A symbol attempted to wrap hidden text");
    }
    std::expected<MermaidSvgFontMetrics,std::string> svg_font_metrics(const MermaidFont&) const override {
        ++measurements;return std::unexpected("A symbol attempted to measure a hidden font");
    }
};

FlowchartRenderInput symbol_input(std::u16string_view shape,std::u16string_view direction=u"TB") {
    FlowchartRenderInput input;
    input.document.direction=direction;
    input.document.nodes.push_back({.id=u"Symbol",.label=u"Visible label",.shape=std::u16string(shape)});
    input.theme.hand_drawn_seed=42;
    return input;
}

suite flowchart_symbols=[] {
    "mermaid_flowchart_symbol_aliases_match_the_registered_handlers"_test=[] {
        for(const auto& [alias,name]:{
            std::pair{u"small-circle",u"sm-circ"},{u"start",u"sm-circ"},{u"stateStart",u"sm-circ"},
            {u"stop",u"fr-circ"},{u"framed-circle",u"fr-circ"},{u"stateEnd",u"fr-circ"},
            {u"join",u"fork"},{u"forkJoin",u"fork"},{u"collate",u"hourglass"},
            {u"com-link",u"bolt"},{u"lightning-bolt",u"bolt"},
            {u"junction",u"f-circ"},{u"filled-circle",u"f-circ"},
            {u"summary",u"cross-circ"},{u"crossed-circle",u"cross-circ"}}) {
            expect(bool(flowchart_classic_shape(alias)==name));
            expect(!flowchart_shape_has_label(alias));
        }
        expect(flowchart_shape_has_label(u"bang"));
        expect(flowchart_shape_has_label(u"text"));
        expect(flowchart_shape_has_label(u"circle"));
    };

    "mermaid_flowchart_fixed_symbols_discard_supplied_label_and_math_geometry"_test=[] {
        for(const bool rough:{false,true}) for(const auto shape:
            {u"sm-circ",u"fr-circ",u"fork",u"hourglass",u"bolt",u"f-circ",u"cross-circ"}) {
            auto input=symbol_input(shape);input.theme.hand_drawn=rough;
            input.document.nodes.front().label=u"$$this must not appear$$";
            FlowchartMeasurements supplied;
            supplied.nodes.push_back({.width=900,.height=500,.baseline=350,.line_step=500,
                .lines={u"$$this must not appear$$"},.math_label=std::make_shared<MathLabel>()});
            const auto with_label=render_flowchart_svg(input,supplied);
            expect(fatal(with_label.has_value())) << (with_label?"":with_label.error());
            input.document.nodes.front().label.clear();
            supplied.nodes.front()={};
            const auto without_label=render_flowchart_svg(input,supplied);
            expect(fatal(without_label.has_value()));
            expect(bool(with_label->svg==without_label->svg));
            expect(with_label->width<100&&with_label->height<100);
            expect(with_label->svg.find("must not appear")==std::string::npos);
        }
    };

    "mermaid_flowchart_symbol_geometry_keeps_bar_orientation_and_plain_text_bounds"_test=[] {
        FlowchartMeasurements empty;empty.nodes.resize(1);
        for(const auto direction:{u"TB",u"BT",u"LR",u"RL"}) {
            const auto rendered=render_flowchart_svg(symbol_input(u"fork",direction),empty);
            expect(fatal(rendered.has_value())) << (rendered?"":rendered.error());
            const bool horizontal=std::u16string_view(direction)==u"LR"||std::u16string_view(direction)==u"RL";
            expect(std::abs(rendered->width-(horizontal?26.0:86.0))<1e-6);
            expect(std::abs(rendered->height-(horizontal?86.0:26.0))<1e-6);
        }
        const auto bolt=render_flowchart_svg(symbol_input(u"bolt"),empty);
        expect(fatal(bolt.has_value()));
        expect(std::abs(bolt->width-51)<1e-6);expect(std::abs(bolt->height-86)<1e-6);
        auto text=symbol_input(u"text");text.theme.minimum_label_width=0;
        FlowchartMeasurements measured;
        measured.nodes.push_back({.width=80,.height=24,.baseline=18,.line_step=24,.lines={u"Visible label"}});
        const auto visible=render_flowchart_svg(text,measured);
        expect(fatal(visible.has_value()));
        expect(std::abs(visible->width-111)<1e-6);expect(std::abs(visible->height-55)<1e-6);
        expect(visible->svg.find("Visible label")!=std::string::npos);
        expect(visible->svg.find("fill=\"none\"")!=std::string::npos);
        text.document.nodes.front().shape=u"bang";
        for(const bool rough:{false,true}) {
            text.theme.hand_drawn=rough;
            const auto bang=render_flowchart_svg(text,measured);
            expect(fatal(bang.has_value())) << (bang?"":bang.error());
            expect(bang->width>visible->width&&bang->height>visible->height);
            expect(bang->svg.find("Visible label")!=std::string::npos);
        }
    };

    "mermaid_flowchart_hidden_symbol_labels_never_request_text_or_tex_services"_test=[] {
        FlowchartRenderInput flow;
        for(const auto shape:{u"sm-circ",u"fr-circ",u"fork",u"hourglass",u"bolt",u"f-circ",u"cross-circ"})
            flow.document.nodes.push_back({.id=shape,.label=u"$$\\unavailable{formula}$$",.shape=shape});
        NativeDiagram input;input.content=std::move(flow);
        RejectSymbolText measurer;
        std::size_t formulas=0;
        const auto svg=render_measured_diagram(std::move(input),measurer,
            [&](std::u16string_view,double)->std::expected<MathFragment,std::string> {
                ++formulas;return std::unexpected("A symbol attempted to render hidden TeX");
            });
        expect(fatal(svg.has_value())) << (svg?"":svg.error());
        expect(measurer.measurements==0_u);expect(formulas==0_u);
        expect(svg->svg.find("unavailable")==std::string::npos);
    };
};
}
