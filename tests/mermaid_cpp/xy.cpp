#include "support/folia_test.hpp"
import folia.mermaid.xy_svg;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
void same(std::u16string_view a,const ConfigValue& e,std::u16string_view key){expect(bool(a==field(e,key).string))<<hex(key)<<hex(a)<<hex(field(e,key).string);}
void near(double a,double e){expect(std::abs(a-e)<1e-8)<<a<<e;}
void axis(const XYAxis& a,const ConfigValue& e){same(a.title,e,u"title");expect(a.band==(field(e,u"type").string==u"band"));if(a.band){const auto& cats=field(e,u"categories").sequence;expect(fatal(cats.size()==a.categories.size()));for(std::size_t i=0;i<cats.size();++i)expect(bool(a.categories[i]==cats[i].string));}else{near(a.min,field(e,u"min").number);near(a.max,field(e,u"max").number);}}
suite xy=[] {
    "mermaid_xy_original_measured_layout"_test=[] {
        const auto fixture_dir=std::getenv("FOLIA_MERMAID_FIXTURES");
        const auto live=std::getenv("FOLIA_MERMAID_XY_MEASUREMENTS");
        const auto path=live?std::filesystem::path(live):std::filesystem::path(fixture_dir?fixture_dir:FOLIA_MERMAID_FIXTURE_DIR)/"xy-measurements.tsv";
        std::ifstream input(path);expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            std::vector<std::string_view> fields;std::string_view rest{line};for(;;){auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}expect(fatal(fields.size()==5_u));++count;
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            site.mapping.emplace(u"fontFamily",ConfigValue{.kind=ConfigKind::string,.string=u"Arial"});site.mapping.emplace(u"theme",ConfigValue{.kind=ConfigKind::string,.string=decode(fields[2])});
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));auto actual=prepare_xy(decode(fields[1]),*session);expect(fatal(actual.has_value()))<<fields[0];
            const auto requests=xy_measurement_requests(*actual);const auto expected=json(decode(fields[3]));const auto boxes=json(decode(fields[4]));expect(fatal(requests.size()==expected.sequence.size()));expect(fatal(requests.size()==boxes.sequence.size()));XYMeasurements measurements;
            for(std::size_t i=0;i<requests.size();++i){same(requests[i].text,expected.sequence[i],u"text");near(requests[i].font_size,field(expected.sequence[i],u"font_size").number);measurements.text.push_back({field(boxes.sequence[i],u"width").number,field(boxes.sequence[i],u"height").number});}
            auto svg=render_xy_svg(*actual,measurements);expect(fatal(svg.has_value()))<<fields[0]<<(svg?"":svg.error());
            if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);std::ofstream output(std::filesystem::path(directory)/("xy-oracle-"+std::string(fields[0])+".svg"),std::ios::binary);output<<svg->svg;expect(output.good());}
        }expect(count==24_u);
    };
    "mermaid_xy_parser_db_and_configuration_match_fixed_original_corpus"_test=[] {
        const auto override_path=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path{override_path?override_path:FOLIA_MERMAID_FIXTURE_DIR}/"xy.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;std::vector<std::string_view> fields;std::string_view rest{line};for(;;){auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}expect(fatal(fields.size()==4_u));++count;
            auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(fields[2]));expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));auto e=json(decode(fields[3]));auto actual=prepare_xy(decode(fields[1]),*session);
            if(e.kind==ConfigKind::null){expect(!actual)<<fields[0];continue;}expect(fatal(actual.has_value()))<<fields[0]<<(actual?"":actual.error());const auto& d=actual->document;same(d.title,e,u"title");same(d.acc_title,e,u"accTitle");same(d.acc_description,e,u"accDescription");const auto& data=field(e,u"data");axis(d.x,field(data,u"xAxis"));axis(d.y,field(data,u"yAxis"));const auto& plots=field(data,u"plots").sequence;expect(fatal(plots.size()==d.plots.size()));
            for(std::size_t i=0;i<plots.size();++i){const auto& p=d.plots[i];const auto& ep=plots[i];same(p.title,ep,u"title");expect(p.bar==(field(ep,u"type").string==u"bar"));const auto& points=field(ep,u"data").sequence;expect(fatal(points.size()==p.points.size()));const auto& color=actual->theme.colors[i%actual->theme.colors.size()];same(color,ep,p.bar?u"fill":u"strokeFill");for(std::size_t j=0;j<points.size();++j){near(p.points[j].y,points[j].sequence[1].number);if(d.x.band)expect(bool(p.points[j].category==points[j].sequence[0].string));else{auto s=points[j].sequence[0].string;std::string n(s.begin(),s.end());near(p.points[j].x,std::stod(n));}if(auto labels=ep.find(u"pointLabels"))expect(bool(p.points[j].label==labels->sequence[j].string));}}
            const auto& cfg=field(e,u"config");const auto& c=actual->config;for(auto [key,value]:{std::pair{u"width",c.width},{u"height",c.height},{u"titleFontSize",c.title_size},{u"titlePadding",c.title_padding},{u"legendFontSize",c.legend_size},{u"legendPadding",c.legend_padding},{u"plotReservedSpacePercent",c.reserved}})near(value,field(cfg,key).number);expect(c.horizontal==(field(cfg,u"chartOrientation").string==u"horizontal"));expect(c.data_labels==field(cfg,u"showDataLabel").boolean);expect(c.outside_labels==field(cfg,u"showDataLabelOutsideBar").boolean);
            expect(!render_xy_svg(*actual,{})); // Measured layout never estimates missing text dimensions.
        }expect(count==15_u);
    };
};
}
