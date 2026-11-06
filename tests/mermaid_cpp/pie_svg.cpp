#include "support/folia_test.hpp"
import folia.mermaid.pie_svg;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string> fields(std::string const& line, char separator) {
    std::vector<std::string> result;
    std::istringstream row(line);
    std::string field;
    while (std::getline(row, field, separator)) result.push_back(field);
    return result;
}
std::u16string unhex(std::string const& hex) {
    std::u16string result;
    if (hex == "-") return result;
    if (hex.size()%4) throw std::runtime_error("Invalid SVG fixture UTF-16");
    for (std::size_t i=0; i<hex.size(); i+=4)
        result += static_cast<char16_t>(std::stoul(hex.substr(i,4),nullptr,16));
    return result;
}
PieSvgTheme theme(bool dark = false) {
    PieSvgTheme result;
    result.colors={"#112233","#223344","#334455","#445566","#556677","#667788",
        "#778899","#8899aa","#99aabb","#aabbcc","#bbccdd","#ccddee"};
    result.stroke_color=result.outer_stroke_color=result.title_color=
        result.section_color=result.legend_color=dark?"#eeeeee":"#111111";
    result.font_family=u"Arial";
    return result;
}
suite pie_svg = [] {
    "mermaid_pie_svg_official_renderer_inputs"_test=[] {
        auto root=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path(root?root:FOLIA_MERMAID_FIXTURE_DIR)/"pie-svg.tsv"};
        expect(fatal(input.is_open()));
        auto output=std::getenv("FOLIA_MERMAID_SVG_OUTPUT");
        if(output) std::filesystem::create_directories(output);
        std::string line;
        std::size_t count{};
        while(std::getline(input,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            if(line.empty() || line.front()=='#') continue;
            auto f=fields(line,'\t');
            expect(fatal(f.size()==11_u));
            auto doc=parse_pie(unhex(f[1]));
            expect(fatal(doc.has_value()))<<f[0];
            PieSvgConfig config;
            if(f[2]=="left") config.layout.legend_position=PieLegendPosition::left;
            else if(f[2]=="top") config.layout.legend_position=PieLegendPosition::top;
            else if(f[2]=="bottom") config.layout.legend_position=PieLegendPosition::bottom;
            else if(f[2]=="center") config.layout.legend_position=PieLegendPosition::center;
            PieMeasurements widths;
            if(f[3]!="-") for(auto const& width:fields(f[3],',')) widths.legend_widths.push_back(std::stod(width));
            widths.title_width=std::stod(f[4]);
            config.layout.geometry.donut_hole=std::stod(f[5]);
            config.layout.geometry.text_position=std::stod(f[6]);
            config.highlight_slice=unhex(f[7]);
            config.use_max_width=f[8]=="1";
            auto result=render_pie_svg(*doc,widths,theme(f[9]=="1"),config);
            expect(result.has_value()==(f[10]=="1"))<<f[0];
            if(output) {
                std::ofstream file(std::filesystem::path(output)/(f[0]+".svg"),std::ios::binary);
                file<<(result?result->svg:"ERROR");
                expect(file.good());
            }
            ++count;
        }
        expect(count==80_u);
    };
    "mermaid_pie_svg_rejects_invalid_text_and_theme"_test=[] {
        PieDocument doc; doc.sections={{u"A",1}};
        PieMeasurements measured{{40},0};
        auto style=theme();
        expect(render_pie_svg(doc,measured,style).has_value());
        doc.title=std::u16string(1,0xd800);
        expect(!render_pie_svg(doc,measured,style));
        doc.title=u"valid"; doc.sections[0].label=std::u16string(1,1);
        expect(!render_pie_svg(doc,measured,style));
        doc.sections[0].label=u"A";
        style.colors[0]="url(https://example.invalid/)";
        expect(!render_pie_svg(doc,measured,style));
        style=theme(); style.legend_size=std::numeric_limits<double>::infinity();
        expect(!render_pie_svg(doc,measured,style));
        style=theme(); style.opacity=2;
        expect(!render_pie_svg(doc,measured,style));
        style=theme(); style.font_family=u"Arial\" onload=\"bad";
        auto safe=render_pie_svg(doc,measured,style);
        expect(fatal(safe.has_value()));
        expect(safe->svg.find("&quot; onload=&quot;")!=std::string::npos);
    };
};
}
