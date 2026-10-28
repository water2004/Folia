#include "support/folia_test.hpp"
import folia.platform.mermaid_measurer;
import folia.mermaid.zenuml_svg;
namespace {
using namespace boost::ut;using namespace folia::platform;using namespace folia::mermaid;
suite mermaid_zenuml=[] {
 "mermaid_zenuml_measured_svg"_test=[] {
  MermaidMeasurer measurer;auto family=measurer.resolve_family(u"Helvetica, Verdana, serif");expect(fatal(family.has_value()));
  ZenSvgMeasure measure=[&](std::u16string_view text,double size,bool bold){auto m=measurer.measure(text,{*family,static_cast<float>(size),bold?600u:400u});if(!m)throw std::runtime_error(m.error());return m->advance;};
  const std::vector<std::pair<std::string,std::u16string>> cases={{"async",u"zenuml\ntitle Demo\nAlice->John: Hello John, how are you?\nJohn->Alice: Great!\nAlice->John: See you later!"},
{"participants",u"zenuml\ntitle Participants\n@Actor A as Alice\n@Database B as Store\nA->B: Save\nB->A: Saved"},
{"nested",u"zenuml\nA.method() {\n if(x) {\n  B.method() {\n   selfCall() { return X }\n  }\n }\n}"},
{"creation-return",u"zenuml\nnew A1\nnew A2(with, parameters)\nvalue = A1.read()\nA2.write(value) { return saved }"},
{"loop-alt",u"zenuml\nClient.send() {\n while(waiting) {\n  Service.poll()\n }\n if(ok) {\n  Service.accept()\n } else {\n  Service.reject()\n }\n}"}};
  for(const auto& sample:cases){auto doc=parse_zenuml(sample.second);expect(fatal(doc.has_value()))<<sample.first;auto svg=render_zenuml_svg(*doc,measure);expect(fatal(svg.has_value()))<<sample.first<<(svg?"":svg.error());expect(svg->width>0&&svg->height>0);if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);std::ofstream output(std::filesystem::path(directory)/("zenuml-"+sample.first+".svg"),std::ios::binary);output<<svg->svg;expect(output.good());}}
 };
};
}
