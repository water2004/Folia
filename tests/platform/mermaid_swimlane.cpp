#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;

struct SwimlaneCase { std::string_view name; std::u16string_view source; };
constexpr std::array cases{
    SwimlaneCase{"two-lanes-lr", u"swimlane-beta LR\nsubgraph Intake\nStart\nReview\nend\nsubgraph Approval\nApprove\nFinish\nend\nStart --> Review --> Approve --> Finish\n"},
    SwimlaneCase{"decisions-tb", u"swimlane-beta TB\nsubgraph Customer\nRequest\nDecision{Complete?}\nend\nsubgraph Operations\nCheck\nReject\nend\nsubgraph Result\nDone\nend\nRequest --> Decision\nDecision -- yes --> Check --> Done\nDecision -- no --> Reject\n"},
    SwimlaneCase{"unassigned-default-lane", u"swimlane-beta LR\nsubgraph Assigned\nInLane\nend\nOutside --> InLane --> AlsoOutside\n"},
    SwimlaneCase{"edge-labels", u"swimlane-beta LR\nsubgraph First\nA[Begin]\nend\nsubgraph Second\nB[Work]\nend\nA -- routed --> B\n"},
    SwimlaneCase{"reverse-rl", u"swimlane-beta RL\nsubgraph East\nReview\nApprove\nend\nsubgraph West\nStart\nend\nStart --> Review --> Approve\n"},
    SwimlaneCase{"reverse-bt", u"swimlane-beta BT\nsubgraph Upper\nTop\nMiddle\nend\nsubgraph Lower\nReply\nFinish\nend\nTop --> Middle --> Reply --> Finish\n"},
    SwimlaneCase{"styles-multiline", u"swimlane-beta LR\nsubgraph Draft\nA[First line<br/>Second line]\nB{Pass?}\nend\nsubgraph Review\nC[Approved]\nend\nA --> B\nB -- yes --> C\nclassDef accent fill:#ff99cc,stroke:#003366,stroke-width:5px,color:#111111,font-weight:bold,font-size:18px,font-family:Georgia\nclass A accent\nstyle B fill:#fff2cc,stroke:#cc6600,stroke-width:3px,color:#222222\nlinkStyle 0 stroke:#ff6600,stroke-width:5px\n"},
};

suite mermaid_swimlane_platform = [] {
    "mermaid_swimlane_measured_svg_renders_browser_cases"_test = [] {
        for (const auto theme : {std::u16string_view(u"default"), std::u16string_view(u"dark")}) {
            for (const auto& test_case : cases) {
                auto session = ConfigSession::create();
                expect(fatal(session.has_value()));
                ConfigValue site;
                site.kind = ConfigKind::mapping;
                site.mapping.emplace(u"fontFamily", ConfigValue{.kind=ConfigKind::string,.string=u"Arial"});
                site.mapping.emplace(u"theme", ConfigValue{.kind=ConfigKind::string,.string=std::u16string(theme)});
                expect(fatal(session->save_initialize_config(site).has_value()));
                expect(fatal(session->set_site_config(site).has_value()));
                auto rendered = render_mermaid(test_case.source, *session);
                expect(rendered.has_value()) << test_case.name << (rendered ? "" : rendered.error());
                if (!rendered) continue;
                expect(rendered->width > 0 && rendered->height > 0) << test_case.name;
                expect(rendered->svg.find("swimlane-diagram") != std::string::npos) << test_case.name;
                expect(rendered->svg.find("<foreignObject") == std::string::npos) << test_case.name;
                expect(rendered->svg.find("<text") != std::string::npos) << test_case.name;
                if (test_case.name == "styles-multiline") {
                    expect(rendered->svg.find("#ff99cc") != std::string::npos);
                    expect(rendered->svg.find("#003366") != std::string::npos);
                    expect(rendered->svg.find("#ff6600") != std::string::npos);
                    expect(rendered->svg.find("Second line") != std::string::npos);
                    expect(rendered->svg.find("font-weight:700") != std::string::npos);
                    expect(rendered->svg.find("font-family:Georgia") != std::string::npos);
                }
                if (test_case.name == "edge-labels" || test_case.name == "styles-multiline")
                    expect(rendered->svg.find("edge-label-background") != std::string::npos) << test_case.name;
                if (test_case.name == "reverse-rl" || test_case.name == "reverse-bt")
                    expect(rendered->svg.find("rotate(-90)") == std::string::npos) << test_case.name;
                if (auto directory = std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);
                    std::string output_name(test_case.name);
                    output_name += theme == u"dark" ? "-dark" : "-default";
                    std::ofstream output(std::filesystem::path(directory) /
                        ("swimlane-" + output_name + ".svg"), std::ios::binary);
                    output << rendered->svg;
                    expect(output.good()) << output_name;
                }
            }
        }
    };
};
}
