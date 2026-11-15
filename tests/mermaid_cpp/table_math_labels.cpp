#include "support/folia_test.hpp"
import folia.mermaid.er_svg;
import folia.mermaid.requirement_svg;
import folia.mermaid.config_value;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

// Explicit provider results exercise table geometry independently of a font
// engine. Real native MathJax is covered by FoliaMermaidMathTests and the oracle.
std::expected<std::optional<MathLabel>,std::string> measured_math(
    std::u16string_view text,double size,unsigned weight,bool italic) {
    if(!has_math_label(text))return std::optional<MathLabel>{};
    auto label=measure_math_label(text,{.font_size=size,.line_height=size*1.5,
        .font_weight=weight,.font_italic=italic},
        [](std::u16string_view,unsigned,bool)->std::expected<MathLabelTextMetrics,std::string> {
            return MathLabelTextMetrics{20,16,12};
        },[](std::u16string_view,double)->std::expected<MathFragment,std::string> {
            return MathFragment{"<svg width=\"80\" height=\"64\"><path d=\"M0 0H80V64\"/></svg>",80,64,48};
        });
    if(!label)return std::unexpected(label.error());
    return std::optional<MathLabel>{std::move(*label)};
}

ErRenderInput er_input(std::u16string_view layout,bool html=true) {
    ErRenderInput input;
    input.document.effective_config.kind=ConfigKind::mapping;
    input.document.effective_config.mapping.emplace(u"layout",
        ConfigValue{.kind=ConfigKind::string,.string=std::u16string{layout}});
    input.document.effective_config.mapping.emplace(u"htmlLabels",
        ConfigValue{.kind=ConfigKind::boolean,.boolean=html});
    input.document.entities={
        {.name=u"A",.id=u"A",.alias=u"$$x$$",.attributes={{.type=u"int",.name=u"value",.comment=u"$$y$$"}}},
        {.name=u"B",.id=u"B"}};
    input.document.relationships={{.entity_a=u"A",.entity_b=u"B",.rel_type=u"IDENTIFYING",
        .card_a=u"ONLY_ONE",.card_b=u"ZERO_OR_MORE",.label=u"$$z$$"}};
    return input;
}

suite table_math = [] {
    "mermaid_er_math_sizes_headers_attribute_rows_and_relationships"_test=[] {
        for(std::u16string_view layout:{u"dagre",u"elk"}) {
            auto input=er_input(layout);
            std::size_t formulas=0;
            const ErSvgMeasure ordinary=[](std::u16string_view text,double,bool)
                ->std::expected<ErTextMetrics,std::string> {
                if(has_math_label(text))return std::unexpected("TeX reached ordinary ER measurement");
                return ErTextMetrics{24,24,16,12};
            };
            const MathLabelMeasure rich=[&](std::u16string_view text,std::u16string_view family,
                double size,unsigned weight,bool italic) {
                expect(bool(family==u"Arial"));
                if(has_math_label(text))++formulas;
                return measured_math(text,size,weight,italic);
            };
            auto result=render_er_svg(input,ordinary,rich);
            expect(result.has_value()) << (result ? "" : result.error());
            expect(fatal(result.has_value()));
            expect(formulas==3_u);
            expect(result->height>128);
            expect(result->svg.find("$$")==std::string::npos);
            expect(result->svg.find("<foreignObject")==std::string::npos);
            expect(result->svg.find("er-math-label")!=std::string::npos);
            expect(result->svg.find("attribute-comment")!=std::string::npos);
        }
    };

    "mermaid_table_math_gates_html_and_propagates_renderer_errors"_test=[] {
        const ErSvgMeasure ordinary=[](std::u16string_view,double,bool)
            ->std::expected<ErTextMetrics,std::string> {return ErTextMetrics{24,24,16,12};};
        const MathLabelMeasure failure=[](std::u16string_view text,std::u16string_view,double,unsigned,bool)
            ->std::expected<std::optional<MathLabel>,std::string> {
            if(has_math_label(text))return std::unexpected("math provider failed");
            return std::optional<MathLabel>{};
        };
        const auto failed=render_er_svg(er_input(u"dagre"),ordinary,failure);
        expect(!failed);
        expect(bool(failed.error()=="math provider failed"));
        const auto literal=render_er_svg(er_input(u"dagre",false),ordinary,failure);
        expect(fatal(literal.has_value()));
        expect(literal->svg.find("$$x$$")!=std::string::npos);
    };

    "mermaid_requirement_math_honors_field_typography_and_height"_test=[] {
        RequirementRenderInput input;
        input.document.requirements={{.name=u"R",.type=u"Requirement",.text=u"$$x_1$$",
            .css_styles={u"font-family:Arial",u"font-size:24px",u"font-weight:700",u"font-style:italic"}}};
        const RequirementSvgMeasure ordinary=[](std::u16string_view text,std::u16string_view,
            double,unsigned,bool,std::span<const RequirementTextRun>,bool)
            ->std::expected<RequirementTextMetrics,std::string> {
            if(has_math_label(text))return std::unexpected("TeX reached ordinary Requirement measurement");
            return RequirementTextMetrics{24,24,24,16,12};
        };
        std::size_t formulas=0;
        const MathLabelMeasure rich=[&](std::u16string_view text,std::u16string_view family,
            double size,unsigned weight,bool italic) {
            if(has_math_label(text)) {
                ++formulas;
                expect(bool(family==u"Arial"));
                expect(size==24.0_d);
                expect(weight==700_u);
                expect(italic);
            }
            return measured_math(text,size,weight,italic);
        };
        for(std::u16string_view layout:{u"dagre",u"elk"}) {
            input.config.layout=layout;
            auto result=render_requirement_svg(input,ordinary,rich);
            expect(result.has_value()) << (result ? "" : result.error());
            expect(fatal(result.has_value()));
            expect(result->height>100);
            expect(result->svg.find("$$")==std::string::npos);
            expect(result->svg.find("Text:")!=std::string::npos);
            expect(result->svg.find("requirement-math-label")!=std::string::npos);
        }
        expect(formulas==2_u);
    };
};
}
