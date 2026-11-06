#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;

suite mermaid_requirement = [] {
    "mermaid_requirement_measured_svg"_test = [] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"basic",u"requirementDiagram\nrequirement req1 {\nid: R1\ntext: Must be safe\nrisk: high\nverifyMethod: test\n}\n"},
            {"types",u"requirementDiagram\nfunctionalRequirement \"User authentication\" {\nid: FR1\ntext: Users must sign in\nrisk: medium\nverifyMethod: demonstration\n}\ninterfaceRequirement API {\nid: IR2\ntext: Expose HTTPS\nrisk: low\nverifyMethod: inspection\n}\nperformanceRequirement Latency {\nid: PR3\ntext: Under 100 ms\nrisk: high\nverifyMethod: analysis\n}\nphysicalRequirement Device {\nid: PH4\ntext: Must fit enclosure\nrisk: low\nverifyMethod: test\n}\ndesignConstraint Budget {\nid: DC5\ntext: Under budget\nrisk: medium\nverifyMethod: inspection\n}\n"},
            {"elements",u"requirementDiagram\nelement \"Login service\" {\ntype: software\ndocRef: docs/login.md\n}\nelement DB {\ntype: database\ndocRef: DB101\n}\n"},
            {"relationships",u"requirementDiagram\nrequirement R1 {\nid: 1\ntext: First\nrisk: low\nverifyMethod: test\n}\nrequirement R2 {\nid: 2\ntext: Second\nrisk: high\nverifyMethod: analysis\n}\nelement E1 {\ntype: module\ndocRef: sourceone\n}\nR1 - contains -> R2\nR2 <- derives - E1\nR1 - verifies -> E1\n"},
            {"direction-title-access",u"requirementDiagram\ndirection LR\naccTitle: Requirements\naccDescr: A compact model\nrequirement R {\nid: reqr\ntext: \"Keep < 2s\"\nrisk: low\nverifyMethod: inspection\n}\n"},
            {"classes-style",u"requirementDiagram\nrequirement R:::critical {\nid: R1\ntext: Important\nrisk: high\nverifyMethod: test\n}\nelement E {\ntype: api\ndocRef: spec\n}\nclassDef critical fill:#ffeeee,stroke:#cc0000\nstyle E fill:#eeeeff,stroke:#0000cc\nR - satisfies -> E\n"},
            {"comment-quoted",u"requirementDiagram\n# top comment\nrequirement \"Long requirement name\" {\nid: \"RQ 1\"\ntext: \"Text with spaces and: colon\"\nrisk: medium\nverifyMethod: demonstration\n}\n%% comment\n"},
            {"rich-labels",u"requirementDiagram\nrequirement Access {\nid: R1\ntext: \"A **signed** request <b>must</b><br/>stay _private_ for permitted clients\"\nrisk: high\nverifyMethod: test\n}\nelement UI {\ntype: \"Approved **client**\"\n}\nAccess - verifies -> UI\n"},
            {"rich-svg-labels",u"---\nconfig:\n  htmlLabels: false\n---\nrequirementDiagram\nrequirement Access {\nid: R1\ntext: \"A **signed** request <b>must</b><br/>stay _private_ for permitted clients\"\nrisk: high\nverifyMethod: test\n}\nelement UI {\ntype: \"Approved **client**\"\n}\nAccess - verifies -> UI\n"},
            {"label-wrapping",u"requirementDiagram\nrequirement Stable {\ntext: \"Keep the system stable with recoverable errors and monitor delayed operations\"\n}\nstyle Stable font-size:24px,color:#335577\n"},
            {"svg-labels-title",u"---\ntitle: Main\nconfig:\n  htmlLabels: false\n---\nrequirementDiagram\nrequirement R {\nverifyMethod: test\n}\nelement E {\ntype: module\n}\nR - verifies -> E\n"},
        };
        for(const auto theme:{u"default",u"dark"}) for(const bool elk:{false,true}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            ConfigValue site;site.kind=ConfigKind::mapping;
            for(const auto& [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"},{u"look",u"classic"}})
                site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            if(!elk)site.mapping.emplace(u"layout",ConfigValue{.kind=ConfigKind::string,.string=u"dagre"});
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& sample:cases) {
                const auto& name=sample.first;
                auto result=render_mermaid(sample.second,*session);
                expect(result.has_value())<<name<<(result?"":result.error());
                if(!result)continue;
                expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);
                    const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";
                    const auto layout_suffix=elk?"-elk-":"-";
                    std::ofstream output(std::filesystem::path(directory)/("requirement-"+name+layout_suffix+suffix+".svg"),std::ios::binary);
                    output<<result->svg;expect(output.good());
                }
            }
        }
    };
};
}
