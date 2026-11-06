#include "support/folia_test.hpp"
import folia.mermaid.kanban_svg;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
std::vector<std::string_view> columns(std::string_view rest){std::vector<std::string_view> out;for(;;){auto tab=rest.find('\t');out.push_back(rest.substr(0,tab));if(tab==rest.npos)return out;rest.remove_prefix(tab+1);}}
suite kanban=[] {
 "mermaid_kanban_parser_and_config_match_original"_test=[] {
  const auto dir=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(dir?dir:FOLIA_MERMAID_FIXTURE_DIR)/"kanban.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
  while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;auto f=columns(line);expect(fatal(f.size()==4_u));++count;auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(f[2]));expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));auto actual=prepare_kanban(decode(f[1]),*session);auto expected=json(decode(f[3]));if(expected.kind==ConfigKind::null){expect(!actual)<<f[0];continue;}expect(fatal(actual.has_value()))<<f[0]<<(actual?"":actual.error());const auto& nodes=field(expected,u"nodes").sequence;expect(fatal(actual->document.nodes.size()==nodes.size()));for(std::size_t i=0;i<nodes.size();++i){const auto& a=actual->document.nodes[i];const auto& e=nodes[i];for(const auto& [key,value]:{std::pair{u"id",a.id},{u"label",a.label},{u"parentId",a.parent},{u"ticket",a.ticket},{u"priority",a.priority},{u"assigned",a.assigned},{u"icon",a.icon}}){const auto& v=field(e,key);expect(bool(value==(v.kind==ConfigKind::null?u"":v.string)));}expect(a.level==field(e,u"level").number);expect(a.is_group==field(e,u"isGroup").boolean);}expect(actual->config.section_width==field(field(expected,u"config"),u"sectionWidth").number);expect(!render_kanban_svg(*actual,{}));
  }expect(count==8_u);
 };
};
}
