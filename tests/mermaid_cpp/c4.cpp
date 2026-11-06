#include "support/folia_test.hpp"
import folia.mermaid.c4_svg;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
std::vector<std::string_view> columns(std::string_view rest){std::vector<std::string_view> out;for(;;){auto tab=rest.find('\t');out.push_back(rest.substr(0,tab));if(tab==rest.npos)return out;rest.remove_prefix(tab+1);}}
suite c4=[] {
 "mermaid_c4_parser_and_config_match_original"_test=[] {
  const auto dir=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(dir?dir:FOLIA_MERMAID_FIXTURE_DIR)/"c4.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
  while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;auto f=columns(line);expect(fatal(f.size()==4_u));++count;auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(f[2]));expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));auto actual=prepare_c4(decode(f[1]),*session);auto expected=json(decode(f[3]));if(expected.kind==ConfigKind::null){expect(!actual)<<f[0];continue;}expect(fatal(actual.has_value()))<<f[0]<<(actual?"":actual.error());const auto& d=actual->document;expect(bool(d.type==field(expected,u"type").string));expect(bool(d.title==field(expected,u"title").string));expect(d.shapes_in_row==field(expected,u"shapesInRow").number);expect(d.boundaries_in_row==field(expected,u"boundariesInRow").number);
   const auto& shapes=field(expected,u"shapes").sequence;expect(fatal(d.shapes.size()==shapes.size()));for(std::size_t i=0;i<shapes.size();++i){const auto& a=d.shapes[i];const auto& e=shapes[i];expect(bool(a.alias==field(e,u"alias").string));expect(bool(a.label==field(field(e,u"label"),u"text").string));expect(bool(a.description==field(field(e,u"descr"),u"text").string));expect(bool(a.type==field(field(e,u"typeC4Shape"),u"text").string));expect(bool(a.parent==field(e,u"parentBoundary").string));if(auto techn=e.find(u"techn")){expect(fatal(a.technology.has_value()));expect(bool(*a.technology==field(*techn,u"text").string));}else expect(!a.technology);for(const auto& [key,value]:a.styles)expect(bool(value==field(e,key).string));}
   const auto& boundaries=field(expected,u"boundaries").sequence;expect(fatal(d.boundaries.size()==boundaries.size()));for(std::size_t i=0;i<boundaries.size();++i){const auto& a=d.boundaries[i];const auto& e=boundaries[i];expect(bool(a.alias==field(e,u"alias").string));expect(bool(a.parent==field(e,u"parentBoundary").string));expect(bool(a.label==field(field(e,u"label"),u"text").string));expect(bool(a.type==field(field(e,u"type"),u"text").string));if(auto kind=e.find(u"nodeType"))expect(bool(a.node_type==kind->string));}
   const auto& rels=field(expected,u"rels").sequence;expect(fatal(d.relations.size()==rels.size()));for(std::size_t i=0;i<rels.size();++i){const auto& a=d.relations[i];const auto& e=rels[i];expect(bool(a.type==field(e,u"type").string));expect(bool(a.from==field(e,u"from").string));expect(bool(a.to==field(e,u"to").string));expect(bool(a.label==field(field(e,u"label"),u"text").string));expect(bool(a.technology==field(field(e,u"techn"),u"text").string));expect(bool(a.description==field(field(e,u"descr"),u"text").string));for(const auto& [key,value]:a.styles)expect(bool(value==field(e,key).string));}expect(!render_c4_svg(*actual,{}));
  }expect(count==10_u);
 };
};
}
