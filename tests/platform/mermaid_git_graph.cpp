#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_git_graph=[] {
    "mermaid_git_graph_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"commits",u"gitGraph\ncommit id: \"A\" tag: \"v1\"\ncommit id: \"B\" type: REVERSE\ncommit id: \"C\" type: HIGHLIGHT tag: \"ready\" tag: \"v2\""},
            {"merge",u"gitGraph\ncommit id: \"A\"\nbranch feature\ncommit id: \"B\"\ncheckout main\ncommit id: \"C\"\nmerge feature id: \"M\""},
            {"top-bottom",u"gitGraph TB:\ncommit id: \"A\"\nbranch feature\ncommit id: \"B\"\ncheckout main\ncommit id: \"C\"\nmerge feature id: \"M\""},
            {"bottom-top",u"gitGraph BT:\ncommit id: \"A\"\nbranch feature\ncommit id: \"B\"\ncheckout main\ncommit id: \"C\"\nmerge feature id: \"M\""},
            {"parallel",u"%%{init: {\"gitGraph\":{\"parallelCommits\":true}}}%%\ngitGraph\ncommit id: \"A\"\nbranch feature\ncommit id: \"B\"\ncheckout main\ncommit id: \"C\"\nmerge feature id: \"M\""},
            {"configured",u"%%{init: {\"gitGraph\":{\"mainBranchName\":\"trunk\",\"mainBranchOrder\":2,\"rotateCommitLabel\":false}}}%%\ngitGraph\ncommit id: \"A\"\nbranch feature order: 1\ncommit id: \"B\"\ncheckout trunk\ncommit id: \"C\""},
            {"cherry",u"gitGraph\ncommit id: \"A\"\nbranch feature\ncommit id: \"B\"\ncheckout main\ncommit id: \"C\"\ncherry-pick id: \"B\""},
            {"cherry-merge",u"gitGraph\ncommit id: \"A\"\nbranch feature\ncommit id: \"B\"\ncheckout main\ncommit id: \"C\"\nmerge feature id: \"M\"\nbranch release\ncommit id: \"R\"\ncheckout feature\ncherry-pick id: \"M\" parent: \"C\" tag: \"backport\""}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("git-graph-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
            }
        }
    };
};
}
