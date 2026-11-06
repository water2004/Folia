#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;using namespace folia::platform;using namespace folia::mermaid;
suite mermaid_zenuml=[] {
 "mermaid_zenuml_measured_svg"_test=[] {
  auto session=ConfigSession::create();
  expect(fatal(session.has_value()));
  const std::vector<std::pair<std::string,std::u16string>> cases={{"async",u"zenuml\ntitle Demo\nAlice->John: Hello John, how are you?\nJohn->Alice: Great!\nAlice->John: See you later!"},
{"participants",u"zenuml\ntitle Participants\n@Actor A as Alice\n@Database B as Store\nA->B: Save\nB->A: Saved"},
{"nested",u"zenuml\nA.method() {\n if(x) {\n  B.method() {\n   selfCall() { return X }\n  }\n }\n}"},
{"creation-return",u"zenuml\nnew A1\nnew A2(with, parameters)\nvalue = A1.read()\nA2.write(value) { return saved }"},
{"loop-alt",u"zenuml\nClient.send() {\n while(waiting) {\n  Service.poll()\n }\n if(ok) {\n  Service.accept()\n } else {\n  Service.reject()\n }\n}"}};
  for(const auto& sample:cases){
      const auto& name=sample.first;
      auto svg=render_mermaid(sample.second,*session);
      expect(fatal(svg.has_value()))<<name<<(svg?"":svg.error());
      expect(svg->width>0&&svg->height>0);
      if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){
          std::filesystem::create_directories(directory);
          std::ofstream output(std::filesystem::path(directory)/("zenuml-"+name+".svg"),std::ios::binary);
          output<<svg->svg;
          expect(output.good());
      }
  }
 };
};
}
