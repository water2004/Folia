#include "support/folia_test.hpp"
import folia.mermaid.agentflow;
namespace {
using namespace boost::ut;
using namespace folia::mermaid;
suite agentflow = [] {
    "mermaid_agentflow_nodes_edges_and_direction"_test = [] {
        auto d = parse_agentflow(u"agentflow-beta LR\nA[Planner] -->|request| "
                                 u"B[Tool] \nB -.- C[Result]\n");
        expect(fatal(d.has_value()));
        expect(d->direction == u"LR");
        expect(d->nodes.size() == 3_u);
        expect(d->edges.size() == 2_u);
        expect(d->nodes[0].label == u"Planner");
        expect(d->edges[0].label == u"request");
        expect(d->edges[1].dashed);
    };
    "mermaid_agentflow_metadata_shapes_and_labeled_branch_syntax"_test = [] {
        auto d = parse_agentflow(u"agentflow-beta TB\nflow reviewer[Review]\n  input[\"Gather "
                                 u"changes\"]@{ shape: input }\n  task[\"Analyse diff\"]@{ shape: task "
                                 u"}\n  input -- inspect --> task\nend\n");
        expect(fatal(d.has_value()));
        expect(d->nodes.size() == 2_u);
        expect(d->nodes[0].label == u"Gather changes");
        expect(d->nodes[0].shape == u"input");
        expect(d->nodes[1].shape == u"task");
        expect(d->edges.size() == 1_u);
        expect(d->edges[0].label == u"inspect");
    };
    "mermaid_agentflow_nested_flows_and_connectors"_test = [] {
        auto d = parse_agentflow(u"agentflow-beta LR\nflow outer[Review]\n  intake[Intake]\n  flow "
                                 u"inner[Checks]\n    policy[Policy]\n  end\nend\nconnector queue[Work "
                                 u"queue]\n");
        expect(fatal(d.has_value()));
        expect(d->containers.size() == 2_u);
        expect(d->containers[0].id == u"outer");
        expect(d->containers[1].parent == u"outer");
        expect(d->nodes.size() == 3_u);
        expect(d->nodes[0].parent == u"outer");
        expect(d->nodes[1].parent == u"inner");
        expect(d->nodes[2].shape == u"connector");
    };
    "mermaid_agentflow_invalid_header_and_direction"_test = [] {
        expect(!parse_agentflow(u"flowchart LR\nA-->B"));
        expect(!parse_agentflow(u"agentflow-beta XX\nA"));
        expect(!parse_agentflow(u"agentflow-beta\n"));
    };
};
} // namespace
