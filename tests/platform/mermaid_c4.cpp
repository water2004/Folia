#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_c4=[] {
    "mermaid_c4_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"context",u"C4Context\ntitle Service context\nPerson(user, \"User\", \"Uses the service\")\nSystem(api, \"Service\", \"Handles requests\")\nSystem_Ext(mail, \"Mail\", \"Sends messages\")\nRel(user, api, \"Uses\", \"HTTPS\")\nRel(api, mail, \"Sends\")"},
            {"container",u"C4Container\nPerson(user, \"User\")\nSystem_Boundary(sys, \"Product\") {\nContainer(web, \"Web\", \"Browser\", \"Interface\")\nContainerDb(db, \"Data\", \"SQL\", \"Storage\")\n}\nRel(user, web, \"Visits\")\nRel(web, db, \"Reads\", \"SQL\")"},
            {"component",u"C4Component\nContainer_Boundary(api, \"API\") {\nComponent(controller, \"Controller\", \"C++\", \"Handles requests\")\nComponentDb(store, \"Store\", \"SQL\", \"Data access\")\n}\nRel(controller, store, \"Uses\")"},
            {"dynamic",u"C4Dynamic\nContainer(client, \"Client\", \"Browser\")\nContainer(server, \"Server\", \"C++\")\nRel(client, server, \"1. Request\", \"HTTPS\")\nRel_Back(client, server, \"2. Response\")"},
            {"deployment",u"C4Deployment\nDeployment_Node(host, \"Host\", \"Linux\") {\nContainer(api, \"API\", \"C++\")\nContainerDb(db, \"Database\", \"SQL\")\n}\nRel(api, db, \"Reads\")"},
            {"nested",u"C4Context\nEnterprise_Boundary(org, \"Organization\") {\nSystem_Boundary(team, \"Team\") {\nSystem(service, \"Service\", \"Application\")\n}\nSystem_Ext(other, \"Partner\")\n}\nRel(service, other, \"Calls\")"},
            {"styles",u"C4Context\nPerson(user, \"Reader\")\nSystem(service, \"Service\")\nRel(user, service, \"Uses\")\nUpdateElementStyle(user, \"#eeeeff\", \"#112233\", \"#556677\")\nUpdateRelStyle(user, service, \"#112233\", \"#cc4400\")"},
            {"rows",u"C4Container\nContainer(a, \"One\", \"C++\")\nContainer(b, \"Two\", \"C++\")\nContainer(c, \"Three\", \"C++\")\nUpdateLayoutConfig($c4ShapeInRow=\"2\", $c4BoundaryInRow=\"1\")"}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session,{},1280);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("c4-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
            }
        }
    };
};
}
