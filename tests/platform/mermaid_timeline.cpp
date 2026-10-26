#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_timeline=[] {
    "mermaid_timeline_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"events",u"timeline\ntitle Development\n2024 : Design : Prototype\n2025 : Build\n2026 : Release"},
            {"sections",u"timeline\nsection Planning\nJanuary : Draft\nFebruary : Review\nsection Delivery\nMarch : Ship : Support"},
            {"periods",u"timeline\nFirst\nSecond : Event"},
            {"wrapped",u"timeline\nsection A section with a long heading\nA long period heading : First line<br>second line : An event description that needs several lines"},
            {"monochrome",u"%%{init: {\"timeline\":{\"disableMulticolor\":true}}}%%\ntimeline\nOne : Alpha\nTwo : Beta\nThree : Gamma"},
            {"configured",u"%%{init: {\"timeline\":{\"leftMargin\":80,\"padding\":20}}}%%\ntimeline TD\ntitle Milestones\naccTitle: Schedule\naccDescr: Main milestones\nsection Start\nDay 1 : Open\nDay 2 : Finish"}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("timeline-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
            }
        }
    };
};
}
