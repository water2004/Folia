#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;

suite mermaid_er = [] {
    "mermaid_er_measured_svg"_test = [] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"basic",u"erDiagram\nCUSTOMER ||--o{ ORDER : places"},
            {"attributes",u"erDiagram\nCUSTOMER {\nint id\nstring name\n}\nORDER {\nint id\nfloat total\n}\nCUSTOMER ||--o{ ORDER : places"},
            {"keys-comments",u"erDiagram\nUSER {\nint id PK \"Unique identifier\"\nstring email UK\nint team_id FK \"Owning team\"\n}\nTEAM {\nint id PK\n}\nTEAM ||--o{ USER : includes"},
            {"cardinality",u"erDiagram\nA ||--|| B : exactly\nB |o--o{ C : optional\nC }|--|{ D : relates"},
            {"non-identifying",u"erDiagram\nPERSON ||..o{ ADDRESS : uses"},
            {"aliases",u"erDiagram\nACCOUNT[\"User account\"] {\nint id PK\n}\nSESSION[\"Login session\"]\nACCOUNT ||--o{ SESSION : owns"},
            {"left-right",u"erDiagram\ndirection LR\nCUSTOMER ||--|{ ORDER : places\nORDER ||--o{ ITEM : contains"},
            {"configured",u"---\ntitle: Orders and items\nconfig:\n  er:\n    diagramPadding: 12\n    entityPadding: 8\n    nodeSpacing: 90\n    rankSpacing: 100\n---\nerDiagram\nORDER {\nint id PK\n}\nORDER ||--o{ ITEM : contains"},
        };
        for(const auto theme:{u"default",u"dark"}) for(const bool elk:{false,true}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"},{u"look",u"classic"}})
                site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            if(!elk) site.mapping.emplace(u"layout",ConfigValue{.kind=ConfigKind::string,.string=u"dagre"});
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases) {
                auto result=render_mermaid(source,*session);
                expect(result.has_value())<<name<<(result?"":result.error());
                if(!result)continue;
                expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);
                    const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";
                    std::ofstream output(std::filesystem::path(directory)/("er-"+name+(elk?"-elk-":"-")+suffix+".svg"),
                        std::ios::binary);
                    output<<result->svg;expect(output.good());
                }
            }
        }
    };
};
}
