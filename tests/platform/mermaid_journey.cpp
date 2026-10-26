#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_journey=[] {
    "mermaid_journey_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"scores",u"journey\ntitle Daily work\nsection Start\nPlan: 5: Alice, Bob\nImplement: 3: Alice\nReview: 1: Bob"},
            {"sections",u"journey\nsection First\nOne: 4: Team\nTwo: 2: Team\nsection Second\nThree: 5: Team\nsection First\nFour: 3: Team"},
            {"no-actors",u"journey\nsection Tasks\nRead: 4\nWrite: 2"},
            {"long-actors",u"%%{init: {\"journey\":{\"maxLabelWidth\":90}}}%%\njourney\nsection Work\nTask: 4: A person with a long display name, Supercalifragilisticexpialidocious"},
            {"breaks",u"journey\nsection First<br>part\nLine one<br/>Line two: 4: Reader"},
            {"configured",u"%%{init: {\"journey\":{\"width\":180,\"height\":60,\"diagramMarginX\":30,\"diagramMarginY\":15,\"taskMargin\":40,\"leftMargin\":120,\"maxLabelWidth\":90,\"taskFontSize\":16,\"taskFontFamily\":\"Arial\",\"titleFontSize\":22,\"titleFontFamily\":\"Arial\",\"titleColor\":\"#884400\"}}}%%\njourney\ntitle Editor path\naccTitle: Editing\naccDescr: Preview and save\nsection Edit\nInput: 5: Editor\nSave: 3: Editor"}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("journey-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
            }
        }
    };
};
}
