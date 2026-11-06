#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_sankey = [] {
    "mermaid_sankey_source_to_measured_svg"_test = [] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"split",u"sankey-beta\nInput,Left,6\nInput,Right,4\nLeft,Output,6\nRight,Output,4"},
            {"align-left",u"%%{init: {\"sankey\":{\"nodeAlignment\":\"left\"}}}%%\nsankey-beta\nA,B,5\nA,C,3\nB,D,5\nC,E,3\nA,E,2"},
            {"align-right",u"%%{init: {\"sankey\":{\"nodeAlignment\":\"right\"}}}%%\nsankey-beta\nA,B,5\nA,C,3\nB,D,5\nC,E,3\nA,E,2"},
            {"align-center",u"%%{init: {\"sankey\":{\"nodeAlignment\":\"center\"}}}%%\nsankey-beta\nA,B,5\nA,C,3\nB,D,5\nC,E,3\nA,E,2"},
            {"align-justify",u"%%{init: {\"sankey\":{\"nodeAlignment\":\"justify\"}}}%%\nsankey-beta\nA,B,5\nA,C,3\nB,D,5\nC,E,3\nA,E,2"},
            {"quoted-values",u"%%{init: {\"sankey\":{\"showValues\":true,\"prefix\":\"$\",\"suffix\":\" USD\",\"linkColor\":\"source\"}}}%%\nsankey-beta\n\"Raw, input\",\"Part \"\"A\"\"\",12.345\n\"Part \"\"A\"\"\",Output,12.345"},
            {"outlined",u"%%{init: {\"sankey\":{\"width\":480,\"height\":320,\"nodeWidth\":18,\"nodePadding\":20,\"labelStyle\":\"outlined\",\"nodeColors\":{\"Input\":\"#ff6600\",\"Output\":\"#006699\"},\"linkColor\":\"target\"}}}%%\nsankey-beta\nInput,Left,6\nInput,Right,4\nLeft,Output,6\nRight,Output,4"},
            {"duplicate-links",u"%%{init: {\"sankey\":{\"linkColor\":\"#8844aa\"}}}%%\nsankey-beta\nA,B,3\nA,B,2\nB,C,5"},
            {"disconnected",u"%%{init: {\"sankey\":{\"showValues\":false}}}%%\nsankey-beta\nA,B,5\nC,D,2"},
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})
                site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases) {
                auto result=render_mermaid(source,*session);
                expect(result.has_value())<<name<<(result?"":result.error());
                if(!result) continue;
                expect(result->width>0 && result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);
                    const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";
                    std::ofstream output(std::filesystem::path(directory)/("sankey-"+name+"-"+suffix+".svg"),std::ios::binary);
                    output<<result->svg;expect(output.good());
                }
            }
        }
    };
};
}
