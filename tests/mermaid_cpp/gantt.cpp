#include "support/folia_test.hpp"
import folia.mermaid.gantt_svg;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
std::vector<std::string_view> columns(std::string_view rest){std::vector<std::string_view> out;for(;;){auto tab=rest.find('\t');out.push_back(rest.substr(0,tab));if(tab==rest.npos)return out;rest.remove_prefix(tab+1);}}
void strings(const std::vector<std::u16string>& actual,const ConfigValue& expected){expect(fatal(actual.size()==expected.sequence.size()));for(std::size_t i=0;i<actual.size();++i)expect(bool(actual[i]==expected.sequence[i].string));}
suite gantt=[] {
    "mermaid_gantt_original_dates_dependencies_and_exclusions"_test=[] {
        const auto dir=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(dir?dir:FOLIA_MERMAID_FIXTURE_DIR)/"gantt.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;auto f=columns(line);expect(fatal(f.size()==4_u));++count;auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(f[2]));expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));auto actual=prepare_gantt(decode(f[1]),*session);auto expected=json(decode(f[3]));if(expected.kind==ConfigKind::null){expect(!actual)<<f[0];continue;}expect(fatal(actual.has_value()))<<f[0]<<(actual?"":actual.error());const auto& d=actual->document;for(auto [key,value]:{std::pair{u"title",&d.title},{u"accTitle",&d.acc_title},{u"accDescription",&d.acc_description},{u"dateFormat",&d.date_format},{u"axisFormat",&d.axis_format},{u"tickInterval",&d.tick_interval}})expect(bool(*value==field(expected,key).string))<<f[0];strings(d.sections,field(expected,u"sections"));strings(d.excludes,field(expected,u"excludes"));strings(d.includes,field(expected,u"includes"));expect(d.inclusive==field(expected,u"inclusive").boolean);const auto& tasks=field(expected,u"tasks").sequence;expect(fatal(d.tasks.size()==tasks.size()));for(std::size_t i=0;i<tasks.size();++i){const auto& a=d.tasks[i];const auto& e=tasks[i];expect(bool(a.id==field(e,u"id").string));expect(bool(a.text==field(e,u"text").string));expect(bool(a.section==field(e,u"section").string));expect(double(a.start)==field(e,u"start").number)<<f[0]<<i;expect(double(a.end)==field(e,u"end").number)<<f[0]<<i;const auto& render=field(e,u"renderEnd");expect(a.render_end.has_value()==(render.kind!=ConfigKind::null));if(a.render_end)expect(double(*a.render_end)==render.number);expect(double(a.order)==field(e,u"order").number);for(auto [key,value]:{std::pair{u"active",a.active},{u"done",a.done},{u"crit",a.critical},{u"milestone",a.milestone},{u"vert",a.vertical},{u"manual",a.manual_end}})expect(value==field(e,key).boolean);}expect(!render_gantt_svg(*actual,{}));
        }expect(count==10_u);
    };
};
}
