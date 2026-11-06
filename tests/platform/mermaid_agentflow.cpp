#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_agentflow = [] {
    "mermaid_agentflow_source_to_measured_elk_svg"_test = [] {
        const std::vector<std::pair<std::string, std::u16string>> cases = {
            {"basic", u"agentflow-beta TB\n  changes[\"Gather changes\"]@{ shape: input }\n "
                      u" analyse[\"Analyse diff\"]@{ shape: task }\n  "
                      u"lint[\"run_linter\"]@{ shape: tool }\n  ok[\"Clean?\"]@{ shape: "
                      u"decision }\n  changes --> analyse --> lint --> ok\n"},
            {"shapes", u"agentflow-beta LR\n  brief[\"Brief\"]@{ shape: input }\n  "
                       u"draft[\"Draft copy\"]@{ shape: task }\n  "
                       u"spellcheck[\"spell_check\"]@{ shape: tool }\n  "
                       u"guide[\"Style guide\"]@{ shape: refdoc }\n  "
                       u"publish[\"Publish\"]@{ shape: action }\n  brief --> draft "
                       u"--> spellcheck --> publish\n  draft -.- guide\n"},
            {"decision", u"agentflow-beta TB\n  triage[\"Triage\"]@{ shape: "
                         u"decision }\n  triage -- bug --> bugfix[\"Bug Fix\"]\n  "
                         u"triage -- feature --> feature_work[\"Feature Work\"]\n  "
                         u"triage -- question --> answer[\"Answer\"]\n"},
            {"nested-flow", u"agentflow-beta LR\n  start[\"Receive request\"]\n  flow "
                            u"review[\"Review\"]\n    intake[\"Intake\"]\n    flow "
                            u"checks[\"Checks\"]\n      policy[\"Policy\"]\n    end\n    "
                            u"approval[\"Approval\"]\n  end\n  connector queue[\"Work queue\"]\n  "
                            u"finish[\"Complete\"]\n  start --> intake --> policy --> approval "
                            u"--> finish\n"},
            {"frame-endpoints", u"agentflow-beta LR\nflow F[\"Frame\"]\n A --> B\nend\n X --> F --> Y"},
            {"frame-descendant-crossing", u"---\nconfig:\n  elk:\n    lineHops: false\n---\nagentflow-beta LR\nflow F[\"Frame\"]\n A --> B\nend\n X --> F --> Y\n Z --> A"},
            {"frame-crossing-default-hop", u"agentflow-beta LR\nflow F[\"Frame\"]\n A --> B\nend\n X --> F --> Y\n Z --> A"},
            {"frame-crossing-gap", u"---\nconfig:\n  elk:\n    lineHops: gap\n---\nagentflow-beta LR\nflow F[\"Frame\"]\n A --> B\nend\n X --> F --> Y\n Z --> A"},
            {"frame-inside-outside", u"agentflow-beta LR\nflow F[\"Frame\"]\n A --> F\nend\n X --> F"},
            {"shape-fallback", u"agentflow-beta TB\n  a[\"Alpha\"]\n  a@{ shape: triangle }\n"
                               u"  b[\"Beta\"]@{ shape: circle }\n  a --> b\n"}};
        for (auto theme : {u"default", u"dark"}) {
            auto session = ConfigSession::create();
            expect(fatal(session.has_value()));
            ConfigValue site;
            site.kind = ConfigKind::mapping;
            site.mapping.emplace(u"theme", ConfigValue{.kind = ConfigKind::string, .string = theme});
            site.mapping.emplace(u"fontFamily", ConfigValue{.kind = ConfigKind::string, .string = u"Arial"});
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            for (const auto &[name, source] : cases) {
                auto result = render_mermaid(source, *session);
                expect(result.has_value()) << name << (result ? "" : result.error());
                if (!result)
                    continue;
                expect(result->width > 0 && result->height > 0);
                if (auto dir = std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(dir);
                    std::ofstream f(std::filesystem::path(dir) /
                                        ("agentflow-" + name +
                                         (theme == std::u16string_view(u"dark") ? "-dark" : "-default") +
                                         ".svg"),
                                    std::ios::binary);
                    f << result->svg;
                    expect(f.good());
                }
            }
        }
    };
};
} // namespace
