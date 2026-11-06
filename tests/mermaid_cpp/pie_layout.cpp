#include "support/folia_test.hpp"
import folia.mermaid.pie_layout;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> parts(std::string_view text, char separator) {
    std::vector<std::string_view> result;
    for (;;) {
        auto end = text.find(separator);
        result.push_back(text.substr(0,end));
        if (end == std::string_view::npos) return result;
        text.remove_prefix(end+1);
    }
}
std::u16string unhex(std::string_view text) {
    if (text == "-") return {};
    if (text.size()%4) throw std::runtime_error("Invalid layout UTF-16 fixture");
    std::u16string result;
    for (std::size_t i=0;i<text.size();i+=4) {
        unsigned value{};
        auto parsed=std::from_chars(text.data()+i,text.data()+i+4,value,16);
        if (parsed.ec!=std::errc{} || parsed.ptr!=text.data()+i+4)
            throw std::runtime_error("Invalid layout UTF-16 digit");
        result.push_back(static_cast<char16_t>(value));
    }
    return result;
}
double number(std::string_view text) {
    double value{};
    auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size())
        throw std::runtime_error("Invalid layout numeric fixture");
    return value;
}
PieLegendPosition position(std::string_view text) {
    if(text=="right") return PieLegendPosition::right;
    if(text=="left") return PieLegendPosition::left;
    if(text=="top") return PieLegendPosition::top;
    if(text=="bottom") return PieLegendPosition::bottom;
    if(text=="center") return PieLegendPosition::center;
    throw std::runtime_error("Invalid layout position fixture");
}
suite pie_layout = [] {
    "mermaid_pie_layout_matches_official_renderer_with_measured_inputs"_test=[] {
        auto override_path=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override_path?override_path:FOLIA_MERMAID_FIXTURE_DIR}/"pie-layout.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count{};
        while(std::getline(input,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            if(line.empty() || line.front()=='#') continue;
            auto f=parts(line,'\t');
            expect(fatal(f.size()==13_u));
            auto document=parse_pie(unhex(f[1]));
            expect(fatal(document.has_value())) << f[0];
            PieMeasurements measured;
            if(f[3]!="-") for(auto width:parts(f[3],',')) measured.legend_widths.push_back(number(width));
            measured.title_width=number(f[4]);
            PieLayoutConfig config;
            config.legend_position=position(f[2]);
            auto layout=layout_pie(*document,measured,config);
            expect(layout.has_value()==(f[5]=="1")) << f[0];
            if(layout) {
                expect(layout->viewbox_x==number(f[6])) << f[0];
                expect(layout->width==number(f[7])) << f[0];
                expect(layout->height==number(f[8])) << f[0];
                expect(layout->pie_x==number(f[9])) << f[0];
                expect(layout->pie_y==number(f[10])) << f[0];
                auto legends=f[11]=="-"?std::vector<std::string_view>{}:parts(f[11],';');
                expect(fatal(layout->legends.size()==legends.size())) << f[0];
                auto text=pie_legend_text(*document);
                expect(fatal(text.has_value()));
                expect(fatal(text->size()==legends.size()));
                for(std::size_t i=0;i<legends.size();++i) {
                    auto fields=parts(legends[i],',');
                    expect(fatal(fields.size()==3_u));
                    expect(layout->legends[i].x==number(fields[0])) << f[0];
                    expect(layout->legends[i].y==number(fields[1])) << f[0];
                    expect(bool(layout->legends[i].text==unhex(fields[2]))) << f[0];
                    expect(bool((*text)[i]==unhex(fields[2]))) << f[0];
                }
                auto slices=f[12]=="-"?std::vector<std::string_view>{}:parts(f[12],';');
                expect(fatal(layout->slices.size()==slices.size())) << f[0];
                for(std::size_t i=0;i<slices.size();++i) {
                    auto fields=parts(slices[i],',');
                    expect(fatal(fields.size()==2_u));
                    expect(layout->slices[i].section_index==static_cast<std::size_t>(number(fields[0]))) << f[0];
                    expect(layout->slices[i].percentage==fields[1]) << f[0];
                }
            }
            ++count;
        }
        expect(count==40_u);
    };
    "mermaid_pie_layout_requires_valid_measurements_without_estimates"_test=[] {
        auto document=parse_pie(u"pie\n\"A\":1\n\"A\":9");
        expect(fatal(document.has_value()));
        expect(!layout_pie(*document,{}));
        expect(!layout_pie(*document,{{10,20},0}));
        expect(layout_pie(*document,{{10},0}).has_value());
        for(auto width:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            expect(!layout_pie(*document,{{width},0}));
            expect(!layout_pie(*document,{{10},width}));
        }
        document->sections={{u"A",std::numeric_limits<double>::max()},{u"B",std::numeric_limits<double>::max()}};
        expect(!layout_pie(*document,{{10,10},0}));
    };
};
}
