#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_state=[] {
    "mermaid_state_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"basic",u"stateDiagram-v2\n[*] --> Idle\nIdle --> Running\nRunning --> [*]"},
            {"aliases",u"stateDiagram-v2\nstate \"Waiting for input\" as Waiting\nWaiting --> Done"},
            {"labels",u"stateDiagram-v2\nIdle --> Active: begin\nActive --> Idle: stop"},
            {"left-right",u"stateDiagram-v2\ndirection LR\n[*] --> First\nFirst --> Second\nSecond --> [*]"},
            {"composite",u"stateDiagram-v2\n[*] --> Ready\nstate Ready {\n[*] --> Waiting\nWaiting --> Working: run\nWorking --> [*]\n}\nReady --> Done"},
            {"nested",u"stateDiagram-v2\nstate Outer {\nstate Inner {\nA --> B\n}\nInner --> C\n}"},
            {"styles",u"stateDiagram-v2\nIdle --> Busy\nstyle Idle fill:#ffeecc,stroke:#884400,color:#112233"},
            {"configured",u"---\ntitle: State lifecycle\nconfig:\n  state:\n    nodeSpacing: 65\n    rankSpacing: 70\n---\nstateDiagram-v2\nA --> B: continue"}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"},{u"look",u"classic"},{u"layout",u"dagre"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            ConfigValue variables;variables.kind=ConfigKind::mapping;variables.mapping.emplace(u"fontFamily",ConfigValue{.kind=ConfigKind::string,.string=u"Arial"});site.mapping.emplace(u"themeVariables",std::move(variables));
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("state-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
            }
        }
    };
};
}
