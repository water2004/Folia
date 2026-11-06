#include "support/folia_test.hpp"
import folia.mermaid.swimlane_svg;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

suite mermaid_swimlane = [] {
    "swimlane_reuses_flowchart_parser_and_preserves_lanes"_test = [] {
        PreparedDiagram prepared;
        prepared.type = u"swimlane";
        prepared.code.cleaned =
            u"swimlane-beta LR\n"
            u"subgraph Intake\nStart\nReview\nend\n"
            u"subgraph Approval\nApprove\nend\n"
            u"Start --> Review --> Approve\n";
        prepared.effective_config = default_session_config();
        auto parsed = prepare_swimlane_render(prepared);
        expect(parsed.has_value()) << (parsed ? "" : parsed.error());
        if (!parsed) return;
        expect(parsed->document.direction == u"LR");
        expect(parsed->document.nodes.size() == 5_u);
        expect(parsed->document.edges.size() == 2_u);
        expect(std::ranges::any_of(parsed->document.nodes, [](const auto& node) {
            return node.group && node.id == u"Intake";
        }));
        expect(std::ranges::any_of(parsed->document.nodes, [](const auto& node) {
            return node.group && node.id == u"Approval";
        }));
    };
};
}
