#include "support/folia_test.hpp"
import folia.mermaid.state_svg;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite state=[] {
 "mermaid_state_parser_and_config_match_original"_test=[] {
  const auto dir=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(dir?dir:FOLIA_MERMAID_FIXTURE_DIR)/"state.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
  while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;std::vector<std::string_view> columns;std::string_view rest=line;for(;;){auto tab=rest.find('\t');columns.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}expect(fatal(columns.size()==4_u));++count;
   auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(columns[2]));expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));auto actual=prepare_state(decode(columns[1]),*session);auto expected=json(decode(columns[3]));if(expected.kind==ConfigKind::null){expect(!actual)<<columns[0];continue;}expect(fatal(actual.has_value()))<<columns[0]<<(actual?"":actual.error());const auto& d=actual->document;expect(bool(d.direction==field(expected,u"direction").string));const auto& nodes=field(expected,u"nodes").sequence;expect(fatal(d.nodes.size()==nodes.size()));
   for(std::size_t i=0;i<nodes.size();++i){const auto& a=d.nodes[i];const auto& e=nodes[i];expect(bool(a.id==field(e,u"id").string));expect(bool(a.label==field(e,u"label").string));expect(bool(a.shape==field(e,u"shape").string));expect(bool(a.parent==field(e,u"parent").string));expect(a.group==field(e,u"group").boolean);expect(bool(a.direction==field(e,u"direction").string));const auto& styles=field(e,u"styles").sequence;expect(fatal(a.styles.size()==styles.size()));for(std::size_t j=0;j<styles.size();++j)expect(bool(a.styles[j]==styles[j].string));}
   const auto& edges=field(expected,u"edges").sequence;expect(fatal(d.edges.size()==edges.size()));for(std::size_t i=0;i<edges.size();++i){const auto& a=d.edges[i];const auto& e=edges[i];expect(bool(a.start==field(e,u"start").string));expect(bool(a.end==field(e,u"end").string));expect(bool(a.label==field(e,u"label").string));}expect(!render_state_svg(*actual,{}));
  }expect(count==10_u);
 };
};
}
