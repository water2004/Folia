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
    "mermaid_agentflow_diagnostics_distinguish_removed_unsupported_and_unknown_shapes"_test = [] {
        const auto parsed = parse_agentflow(u"agentflow-beta TB\n  a[\"Alpha\"]\n"
            u"  a@{ shape: triangle }\n  b[\"Beta\"]@{ shape: circle }\n  a --> b\n");
        expect(fatal(parsed.has_value())) << (parsed ? "" : parsed.error());
        if (!parsed) return;
        expect(fatal(parsed->nodes.size() == 2_u));
        expect(fatal(parsed->diagnostics.size() == 2_u));
        expect(parsed->nodes[0].shape == u"task");
        expect(parsed->nodes[1].shape == u"task");
        const auto& warning = parsed->diagnostics[0];
        const auto& error = parsed->diagnostics[1];
        expect(warning.code == AgentflowDiagnosticCode::shape_unsupported);
        expect(warning.severity == AgentflowDiagnosticSeverity::warning);
        expect(warning.node_id == u"a");
        expect(warning.message == u"shape \"triangle\" is not supported, using \"roundedRect\"");
        expect(fatal(warning.position.has_value()));
        expect(warning.position->start_line == 2_u);
        expect(warning.position->start_column == 2_u);
        expect(warning.position->end_column == 12_u);
        expect(error.code == AgentflowDiagnosticCode::shape_removed);
        expect(error.severity == AgentflowDiagnosticSeverity::error);
        expect(error.node_id == u"b");
        expect(error.message == u"shape \"circle\" was removed in v0.8.1, using \"roundedRect\"");
        expect(fatal(error.position.has_value()));
        expect(error.position->start_line == 4_u);
        expect(error.position->end_column == 28_u);

        const auto inline_shape = parse_agentflow(u"agentflow-beta TB\n  a((Alpha)) --> b");
        expect(fatal(inline_shape.has_value()));
        expect(fatal(inline_shape->diagnostics.size() == 1_u));
        expect(inline_shape->diagnostics.front().code == AgentflowDiagnosticCode::shape_removed);
        expect(inline_shape->nodes.front().label == u"Alpha");
        const auto unknown = parse_agentflow(u"agentflow-beta\n  a@{ shape: made-up }");
        const auto uppercase = parse_agentflow(u"agentflow-beta\n  a@{ shape: roundedRect }");
        expect(!unknown && unknown.error() == "No such shape: made-up.");
        expect(!uppercase && uppercase.error() == "No such shape: roundedRect. Shape names should be lowercase.");
    };
    "mermaid_agentflow_rejects_only_the_parent_assignment_closing_a_cycle"_test = [] {
        const auto parsed = parse_agentflow(u"agentflow-beta TB\n  flow A[\"Flow A\"]\n"
            u"    a1[\"Task A1\"]\n    a1 --> B\n  end\n  flow B[\"Flow B\"]\n"
            u"    b1[\"Task B1\"]\n    b1 --> A\n  end");
        expect(fatal(parsed.has_value())) << (parsed ? "" : parsed.error());
        if (!parsed) return;
        expect(fatal(parsed->containers.size() == 2_u && parsed->nodes.size() == 2_u));
        expect(parsed->containers[0].parent == u"B");
        expect(parsed->containers[1].parent.empty());
        expect(parsed->nodes[0].parent == u"A");
        expect(parsed->nodes[1].parent == u"B");
        expect(fatal(parsed->edges.size() == 2_u && parsed->diagnostics.size() == 1_u));
        expect(parsed->edges[0].to == u"B");
        expect(parsed->edges[1].to == u"A");
        const auto& finding = parsed->diagnostics.front();
        expect(finding.code == AgentflowDiagnosticCode::containment_violation);
        expect(finding.severity == AgentflowDiagnosticSeverity::warning);
        expect(finding.node_id == u"B");
        expect(fatal(finding.position.has_value()));
        expect(finding.position->start_line == 4_u);
        expect(finding.position->start_column == 11_u);
        const auto self = parse_agentflow(u"agentflow-beta\nflow A[Flow A]\n a1 --> A\nend");
        expect(fatal(self.has_value()));
        expect(self->diagnostics.empty());
        expect(self->containers.front().parent.empty());
        expect(self->nodes.size() == 1_u);
    };
    "mermaid_agentflow_retains_metadata_without_inventing_semantic_diagnostics"_test = [] {
        const auto parsed = parse_agentflow(u"agentflow-beta TB\n  flow f[\"F\"]\n"
            u"    a[\"Before\"]@{ shape: input, label: After, myExtension: 42, icon: \"fa:rocket\" }\n"
            u"    a e1@--> b\n  end\n  f@{ totallyCustomKey: opaque }\n"
            u"  e1@{ instruction: \"hand off\", weight: 5 }");
        expect(fatal(parsed.has_value())) << (parsed ? "" : parsed.error());
        if (!parsed) return;
        expect(parsed->diagnostics.empty());
        expect(fatal(parsed->nodes.size() == 2_u && parsed->containers.size() == 1_u && parsed->edges.size() == 1_u));
        const auto& node = parsed->nodes.front();
        const auto* shape = node.metadata.find(u"shape");
        const auto* extension = node.metadata.find(u"myExtension");
        const auto* icon = node.metadata.find(u"icon");
        expect(node.label == u"After");
        expect(shape && shape->string == u"lean-right");
        expect(extension && extension->number == 42.0);
        expect(icon && icon->string == u"fa:rocket");
        const auto* custom = parsed->containers.front().metadata.find(u"totallyCustomKey");
        expect(custom && custom->string == u"opaque");
        const auto& edge = parsed->edges.front();
        const auto* instruction = edge.metadata.find(u"instruction");
        expect(edge.id == u"e1");
        expect(instruction && instruction->string == u"hand off");
        const auto global = parse_agentflow(u"agentflow-beta\nflow f[F]\na --> b\nend\nglobal\nb[Beta]\nend");
        expect(fatal(global.has_value()));
        expect(fatal(global->nodes.size() == 2_u));
        expect(global->nodes[0].parent == u"f");
        expect(global->nodes[1].parent.empty());
        expect(global->diagnostics.empty());
    };
};
} // namespace
