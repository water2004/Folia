#include "support/folia_test.hpp"
import folia.mermaid.zenuml;
import folia.mermaid.directives;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite zenuml=[] {
 "mermaid_zenuml_original_parser_and_measured_coordinates"_test=[] {
  const auto dir=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(dir?dir:FOLIA_MERMAID_FIXTURE_DIR)/"zenuml.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
  while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;auto a=line.find('\t'),b=line.find('\t',a+1);expect(fatal(a!=line.npos&&b!=line.npos));auto name=line.substr(0,a);auto doc=parse_zenuml(decode(std::string_view(line).substr(a+1,b-a-1)));expect(fatal(doc.has_value()))<<name<<(doc?"":doc.error());auto expected=json(decode(std::string_view(line).substr(b+1)));expect(bool(doc->title==field(expected,u"title").string))<<name;
   const auto& participants=field(expected,u"participants").sequence;expect(fatal(doc->participants.size()==participants.size()))<<name;for(std::size_t i=0;i<participants.size();++i){expect(bool(doc->participants[i].name==field(participants[i],u"name").string))<<name;for(auto key:{u"label",u"type"})if(auto e=participants[i].find(key))expect(bool((std::u16string_view(key)==u"label"?doc->participants[i].label:doc->participants[i].type)==e->string))<<name;}
   auto measure=[&](std::u16string_view text){for(const auto& entry:field(expected,u"measurements").sequence)if(entry.sequence[0].string==text)return entry.sequence[1].number;throw std::runtime_error("Missing original ZenUML measurement");};auto layout=layout_zenuml(*doc,measure);expect(fatal(layout.has_value()))<<name;expect(std::abs(layout->height-field(expected,u"height").number)<1e-8)<<name;
   const auto& positions=field(expected,u"positions").sequence;for(std::size_t i=0;i<positions.size();++i)expect(std::abs(layout->positions[i]-field(positions[i],u"x").number)<1e-8)<<name;
   const auto& matrix=field(expected,u"constraints").sequence;for(std::size_t i=0;i<matrix.size();++i)for(std::size_t j=0;j<matrix[i].sequence.size();++j)expect(std::abs(layout->constraints[i][j]-matrix[i].sequence[j].number)<1e-8)<<name;
   const auto& vertical=field(expected,u"vertical").sequence;expect(fatal(layout->vertical.size()==vertical.size()))<<name;for(std::size_t i=0;i<vertical.size();++i){auto& e=vertical[i].sequence[1];expect(std::abs(layout->vertical[i].top-field(e,u"top").number)<1e-8)<<name;expect(std::abs(layout->vertical[i].height-field(e,u"height").number)<1e-8)<<name;}++count;
  }expect(count==5_u);
 };
};
}
