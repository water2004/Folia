#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace { using namespace boost::ut; using namespace folia::platform; using namespace folia::mermaid;
suite mermaid_usecase=[] {
 "mermaid_usecase_source_to_measured_svg"_test=[] {
  const std::vector<std::pair<std::string,std::u16string>> cases={
   {"basic",u"usecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n"},
   {"quoted",u"usecase-beta\ndirection TB\nactor \"Customer Service\"\n\"Customer Service\" --> \"Sign In\"\n"},
   {"mixed",u"usecase-beta\nactor Customer\nactor Admin\nCustomer --> Browse\nCustomer --> Checkout\nCheckout ..> : include Verify\nAdmin -- Audit\n"},
   {"basic-elk",u"---\nconfig:\n  layout: elk\n---\nusecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n"},
   {"boundary-elk",u"---\nconfig:\n  layout: elk\n---\nusecase-beta\nactor Customer\nsystemBoundary sb[Checkout] @{ type: package }\nCheckout(Place order)\nend\nCustomer --> Checkout\nnote for Checkout additional requirements\n"},
   {"variants",u"usecase-beta\nactor Hollow(Hollow) @{ type: hollow, business: true }\nactor Awesome(Awesome) @{ type: awesome }\nItem[Review]\nHollow --o Item\nAwesome x-- Item\n"},
   {"json",u"usecase-beta\nactor User\njson Record @{\n  \"z\": \"last\",\n  \"a\": [1, true],\n  \"nested\": {\"b\": null}\n}\nUser --> Record\n"},
   {"neo",u"---\nconfig:\n  layout: elk\n  look: neo\n---\nusecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n"},
   {"neo-variants",u"---\nconfig:\n  look: neo\n---\nusecase-beta\nactor Hollow(Hollow) @{ type: hollow }\nactor Awesome(Awesome) @{ type: awesome }\nItem[Review]\nHollow --o Item\nAwesome x-- Item\n"},
   {"neo-palette",u"---\nconfig:\n  layout: elk\n  look: neo\n  theme: redux-color\n  usecase:\n    colorScheme: rotate\n---\nusecase-beta\nactor Member, Person\nsystemBoundary App\n  Task[Verify]\nend\nMember --|> Person\nMember --> Task\n"},
   {"rough-basic",u"---\nconfig:\n  look: handDrawn\n  handDrawnSeed: 7\n---\nusecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n"},
   {"rough-boundary",u"---\nconfig:\n  look: handDrawn\n  handDrawnSeed: 7\n---\nusecase-beta\nactor Customer\nsystemBoundary sb[Checkout] @{ type: package }\nCheckout(Place order)\nend\nCustomer --> Checkout\nnote for Checkout additional requirements\n"},
   {"rough-json-elk",u"---\nconfig:\n  layout: elk\n  look: handDrawn\n  handDrawnSeed: 7\n---\nusecase-beta\nactor User\njson Record @{ \"z\": \"last\", \"a\": [1, true] }\nUser --> Record\n"}
  };
  for(const auto theme:{u"default",u"dark"}) {auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;for(auto [key,value]:{std::pair{u"theme",theme},std::pair{u"fontFamily",u"Arial"},std::pair{u"look",u"classic"},std::pair{u"layout",u"dagre"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
   for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("usecase-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}}
   expect(!render_mermaid(u"usecase-beta\njson Record @{ \"a\": [1,] }\n",*session));
  }
 };
}; }
