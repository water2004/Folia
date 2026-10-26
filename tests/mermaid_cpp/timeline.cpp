#include "support/folia_test.hpp"
import folia.mermaid.timeline_svg;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
std::vector<std::string_view> columns(std::string_view rest){std::vector<std::string_view> out;for(;;){auto tab=rest.find('\t');out.push_back(rest.substr(0,tab));if(tab==rest.npos)return out;rest.remove_prefix(tab+1);}}
void strings(const std::vector<std::u16string>& actual,const ConfigValue& expected){expect(fatal(actual.size()==expected.sequence.size()));for(std::size_t i=0;i<actual.size();++i)expect(bool(actual[i]==expected.sequence[i].string));}
suite timeline=[] {
    "mermaid_timeline_parser_and_config_match_original"_test=[] {
        const auto dir=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(dir?dir:FOLIA_MERMAID_FIXTURE_DIR)/"timeline.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;auto f=columns(line);expect(fatal(f.size()==4_u));++count;auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(f[2]));expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));auto actual=prepare_timeline(decode(f[1]),*session);auto expected=json(decode(f[3]));if(expected.kind==ConfigKind::null){expect(!actual)<<f[0];continue;}expect(fatal(actual.has_value()))<<f[0]<<(actual?"":actual.error());const auto& d=actual->document;expect(bool(d.title==field(expected,u"title").string));expect(bool(d.acc_title==field(expected,u"accTitle").string));expect(bool(d.acc_description==field(expected,u"accDescription").string));strings(d.sections,field(expected,u"sections"));expect(bool(d.direction==field(expected,u"direction").string));const auto& tasks=field(expected,u"tasks").sequence;expect(fatal(d.tasks.size()==tasks.size()));for(std::size_t i=0;i<tasks.size();++i){expect(bool(d.tasks[i].text==field(tasks[i],u"task").string));expect(bool(d.tasks[i].section==field(tasks[i],u"section").string));strings(d.tasks[i].events,field(tasks[i],u"events"));}const auto& config=field(expected,u"config");expect(actual->config.left_margin==field(config,u"leftMargin").number)<<f[0];expect(actual->config.disable_multicolor==field(config,u"disableMulticolor").boolean);expect(!render_timeline_svg(*actual,{}));
        }expect(count==8_u);
    };
};
}
