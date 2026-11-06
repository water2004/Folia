#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_block=[] {
    "mermaid_block_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"grid",u"block-beta\ncolumns 2\na[\"Alpha\"] b[\"Beta\"]\nc[\"Gamma\"] d[\"Delta\"]"},
            {"span",u"block-beta\ncolumns 3\na[\"Wide\"]:2 b[\"Side\"]\nc[\"Full row\"]:3"},
            {"nested",u"block-beta\ncolumns 2\nblock:group\ncolumns 2\na[\"One\"] b[\"Two\"]\nc[\"Three\"]:2\nend\nd[\"Outside\"]"},
            {"spaces",u"block-beta\ncolumns 3\na[\"Start\"] space:2\nb[\"End\"]"},
            {"links",u"block-beta\ncolumns 2\na[\"Start\"] b[\"Finish\"]\na -- \"next\" --> b"},
            {"shapes",u"block-beta\ncolumns 3\na(\"Round\") b{\"Choice\"} c[\"Rect\"]"},
            {"arrow",u"block-beta\ncolumns 3\na<[\"Go\"]>(right):2 b[\"Target\"]"},
            {"styles",u"block-beta\ncolumns 2\na[\"Accent\"] b[\"Plain\"]\nclassDef accent fill:#ffeecc,stroke:#aa4400,color:#663300\nclass a accent\nstyle b fill:#ddeeff,stroke:#335577"}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("block-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
            }
        }
    };
};
}
