#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;using namespace folia::platform;using namespace folia::mermaid;
suite mermaid_mindmap=[] {
    "mermaid_mindmap_default_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"shapes",u"mindmap\nroot((Root))\n  plain\n  round(Round)\n  rect[Rectangle]\n  cloud)Cloud(\n  bang))Bang((\n  hex{{Hexagon}}"},
            {"labels",u"mindmap\nroot[\"Read **source**\"]\n  child[\"Line one\nLine two\"]\n  end[\"Caf\u00e9 &amp; \u6587\u4ef6\"]"}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"},{u"look",u"classic"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases) {
                auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;
                expect(result->width>0&&result->height>0);
                if(const auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";
                    std::ofstream output(std::filesystem::path(directory)/("mindmap-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());
                }
            }
        }
    };
};
}
