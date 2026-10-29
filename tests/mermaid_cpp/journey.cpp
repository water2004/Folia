#include "support/folia_test.hpp"
import folia.mermaid.journey_svg;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
std::vector<std::string_view> columns(std::string_view rest){std::vector<std::string_view> out;for(;;){auto tab=rest.find('\t');out.push_back(rest.substr(0,tab));if(tab==rest.npos)return out;rest.remove_prefix(tab+1);}}
void strings(const std::vector<std::u16string>& actual,const ConfigValue& expected){expect(fatal(actual.size()==expected.sequence.size()));for(std::size_t i=0;i<actual.size();++i)expect(bool(actual[i]==expected.sequence[i].string));}
suite journey=[] {
    "mermaid_journey_parser_and_config_match_original"_test=[] {
        const auto dir=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(dir?dir:FOLIA_MERMAID_FIXTURE_DIR)/"journey.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;auto f=columns(line);expect(fatal(f.size()==4_u));++count;auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(f[2]));expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));auto actual=prepare_journey(decode(f[1]),*session);auto expected=json(decode(f[3]));if(expected.kind==ConfigKind::null){expect(!actual)<<f[0];continue;}expect(fatal(actual.has_value()))<<f[0]<<(actual?"":actual.error());const auto& d=actual->document;expect(bool(d.title==field(expected,u"title").string));expect(bool(d.acc_title==field(expected,u"accTitle").string));expect(bool(d.acc_description==field(expected,u"accDescription").string));strings(d.sections,field(expected,u"sections"));strings(d.actors,field(expected,u"actors"));const auto& tasks=field(expected,u"tasks").sequence;expect(fatal(d.tasks.size()==tasks.size()));for(std::size_t i=0;i<tasks.size();++i){expect(bool(d.tasks[i].text==field(tasks[i],u"task").string));expect(bool(d.tasks[i].section==field(tasks[i],u"section").string));expect(d.tasks[i].score==field(tasks[i],u"score").number);strings(d.tasks[i].people,field(tasks[i],u"people"));}const auto& config=field(expected,u"config");for(auto [key,value]:{std::pair{u"width",actual->config.width},{u"height",actual->config.height},{u"diagramMarginX",actual->config.margin_x},{u"diagramMarginY",actual->config.margin_y},{u"leftMargin",actual->config.left_margin},{u"maxLabelWidth",actual->config.max_label_width},{u"taskMargin",actual->config.task_margin}})expect(value==field(config,key).number)<<f[0];expect(!render_journey_svg(*actual,{}));
        }expect(count==8_u);
    };
};
}
