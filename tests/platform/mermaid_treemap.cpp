#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_treemap=[] {
    "mermaid_treemap_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"flat",u"treemap-beta\n\"Fruit\"\n  \"Apples\": 6\n  \"Pears\": 4\n  \"Berries\": 2"},
            {"multiple",u"treemap-beta\n\"North\"\n  \"A\": 7\n  \"B\": 3\n\"South\"\n  \"C\": 5\n  \"D\": 5"},
            {"nested",u"treemap-beta\n\"Company\"\n  \"Sales\"\n    \"Direct\": 12\n    \"Online\": 8\n  \"Services\": 10"},
            {"small-long",u"%%{init: {\"treemap\":{\"nodeWidth\":40,\"nodeHeight\":25}}}%%\ntreemap-beta\n\"A section with a very long title\"\n  \"A long label that must fit\": 20\n  \"Tiny\": 1\n  \"Other\": 3"},
            {"configured",u"%%{init: {\"treemap\":{\"nodeWidth\":64,\"nodeHeight\":36,\"padding\":6,\"diagramPadding\":12,\"showValues\":false}}}%%\ntreemap-beta\ntitle Revenue\n\"Items\"\n  \"A\": 4\n  \"B\": 2"},
            {"currency",u"%%{init: {\"treemap\":{\"valueFormat\":\"$0,0\"}}}%%\ntreemap-beta\n\"Budget\"\n  \"Build\": 12500\n  \"Review\": 2500"},
            {"classes",u"treemap-beta\nclassDef emphasis fill:#ffeecc,stroke:#aa4400,color:#663300\n\"Group\":::emphasis\n  \"Marked\": 8:::emphasis\n  \"Plain\": 4"},
            {"metadata",u"treemap-beta\ntitle Resource allocation\naccTitle: Allocation\naccDescr: Team work\n\"Engineering\"\n  \"Build\": 6\n  \"Support\": 4"}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("treemap-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
            }
        }
    };
};
}
