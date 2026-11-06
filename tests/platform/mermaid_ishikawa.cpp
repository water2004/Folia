#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace { using namespace boost::ut; using namespace folia::platform; using namespace folia::mermaid;
suite mermaid_ishikawa=[] {
 "mermaid_ishikawa_source_to_measured_svg"_test=[] {
  const std::vector<std::pair<std::string,std::u16string>> cases={
   {"basic",u"ishikawa-beta\nProblem\nCause A\n  Subcause A1\nCause B\n"},
   {"multilevel",u"ishikawa-beta\nProduction yield is consistently below target\n  Process\n    Out of focus\n      Camera shake during exposure\n    Incorrect calibration\n  People\n    Training\n      Infrequent refreshers\n  Materials\n    Paper stock\n"},
   {"long-labels",u"ishikawa\nA long effect label that should be measured and wrapped naturally\nLong primary cause with several words\n  A long subordinate cause with enough words to wrap\n    Deep cause label with additional descriptive text\n"},
   {"effect-only",u"ishikawa-beta\nProblem with no listed causes\n"}
  };
  for(const auto theme:{u"default",u"dark"}) {auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
   for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
   expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
   for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
    if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("ishikawa-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
   }
   auto invalid=render_mermaid(u"ishikawa-beta\n",*session);expect(!invalid);
  }
 };
}; }
