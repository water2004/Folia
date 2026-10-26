#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;using namespace folia::platform;using namespace folia::mermaid;
suite mermaid_architecture=[] {
    "mermaid_architecture_default_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"basic",u"architecture-beta\nservice api(server)[API]\nservice store(database)[Store]\napi:R --> L:store"},
            {"groups",u"architecture-beta\ngroup cloud(cloud)[Cloud]\ngroup app(server)[Application] in cloud\ngroup storage(database)[Storage] in cloud\nservice api(server)[API] in app\nservice db(database)[Database] in storage\napi{group}:R -- L:db{group}"},
            {"junction",u"architecture-beta\nservice a(internet)[Client]\nservice b(server)[Worker]\nservice c(disk)[Files]\njunction j\na:R -- L:j\nj:T -- B:b\nj:R --> L:c"},
            {"arrow-titles",u"architecture-beta\nservice a \"API\"[Read input]\nservice b(server)[Write output]\na:R <-[request]-> L:b"},
            {"metadata-config",u"---\nconfig:\n  architecture: {iconSize: 64, padding: 12}\n---\narchitecture-beta\ntitle Deployment\naccTitle: Services\naccDescr: Application and storage\nservice app(server)[Application]\nservice store(disk)[Files]\napp:B --> T:store"},
            {"alignment",u"architecture-beta\nservice a(server)[First]\nservice b(server)[Second]\nservice c(server)[Third]\na:R -- L:b\nb:R -- L:c\nalign row a b c"}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"},{u"look",u"classic"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            ConfigValue variables;variables.kind=ConfigKind::mapping;
            variables.mapping.emplace(u"fontFamily",ConfigValue{.kind=ConfigKind::string,.string=u"Arial"});
            site.mapping.emplace(u"themeVariables",std::move(variables));
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases) {
                auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;
                expect(result->width>0&&result->height>0);
                if(const auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";
                    std::ofstream output(std::filesystem::path(directory)/("architecture-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());
                }
            }
        }
    };
};
}
