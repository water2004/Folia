#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_cynefin=[] {
    "mermaid_cynefin_measured_svg_public_entry"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"domains",u"cynefin-beta\ncomplex\n  \"Emergent practice\"\n  \"Probe-sense-respond\"\ncomplicated\n  \"Expert analysis\"\nclear\n  \"Best practice\"\nchaotic\n  \"Novel practice\"\nconfusion\n  \"Disorder\"\n"},
            {"transition",u"cynefin-beta:\ntitle Team practices\naccTitle: Cynefin model\naccDescr: Five domain framework\ncomplex\n  \"Retrospectives\"\ncomplicated\n  \"Code review\"\nclear\n  \"Deployment checklist\"\nchaotic\n  \"Incident response\"\nconfusion\n  \"New initiative\"\ncomplex --> complicated : \"Pattern emerges\"\ncomplicated --> clear : \"Codified\"\nchaotic --> complex\nconfusion --> chaotic : \"Stabilized\"\n"},
            {"empty-domain",u"cynefin-beta\ncomplex\nclear\n"},
            {"self-loop-filter",u"cynefin-beta\ncomplex\n  \"A\"\ncomplex --> complex : \"Self loop\"\n"},
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            site.mapping.emplace(u"theme",ConfigValue{.kind=ConfigKind::string,.string=theme});
            site.mapping.emplace(u"fontFamily",ConfigValue{.kind=ConfigKind::string,.string=u"Arial"});
            site.mapping.emplace(u"look",ConfigValue{.kind=ConfigKind::string,.string=u"classic"});
            ConfigValue cynefin;cynefin.kind=ConfigKind::mapping;cynefin.mapping.emplace(u"seed",ConfigValue{.kind=ConfigKind::number,.number=31});site.mapping.emplace(u"cynefin",std::move(cynefin));
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("cynefin-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
            }
        }
    };
};
}
