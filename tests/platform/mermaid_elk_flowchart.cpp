#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_elk_flowchart=[] {
    "mermaid_elk_flowchart_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"elk-flat-right",u"flowchart LR\nA[Read] -->|next| B[Write]\nA --> C[Check]\nC --> B"},
            {"elk-flat-down",u"flowchart TD\nA{Ready?} -->|yes| B((Done))\nA -->|no| C[Wait]"},
            {"elk-flat-left",u"flowchart RL\nA[Read] --> B[Write]\nB --> C[Done]"},
            {"elk-flat-up",u"flowchart BT\nA[Read] --> B[Write]\nB --> C[Done]"},
            {"elk-group-right",u"flowchart LR\nsubgraph g[Processing]\nA[Read] --> B[Write]\nend\nB --> C[Done]"},
            {"elk-groups-down",u"flowchart TD\nsubgraph a[Input]\nA[Read] --> B[Check]\nend\nsubgraph b[Output]\nC[Write] --> D[Done]\nend\nB -->|save| C"},
        };
        for(const auto theme:{u"default",u"dark"})for(const auto& sample:cases) {
            const auto& name=sample.first;const auto& body=sample.second;
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            ConfigValue site;site.kind=ConfigKind::mapping;
            site.mapping.emplace(u"theme",ConfigValue{.kind=ConfigKind::string,.string=theme});
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            const auto source=std::u16string(u"---\nconfig:\n  theme: ")+theme+
                u"\n  look: classic\n  layout: elk\n  fontFamily: Arial\n  themeVariables: {fontFamily: Arial}\n  htmlLabels: true\n---\n"+body;
            auto svg=render_mermaid(source,*session);
            expect(svg.has_value())<<name<<(svg?"":svg.error());if(!svg)continue;
            expect(svg->width>0&&svg->height>0);
            if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                std::filesystem::create_directories(directory);
                const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";
                std::ofstream out(std::filesystem::path(directory)/("flowchart-"+name+"-"+suffix+".svg"),std::ios::binary);
                out<<svg->svg;expect(out.good());
            }
        }
    };
};
}
