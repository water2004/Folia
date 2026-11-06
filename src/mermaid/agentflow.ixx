// Mermaid 12.2.0 Agentflow semantic subset. MIT; see ../LICENSE.
export module folia.mermaid.agentflow;
import std;
export import folia.mermaid.config_value;
export namespace folia::mermaid {
enum class AgentflowDiagnosticCode { shape_unsupported, shape_removed, containment_violation };
enum class AgentflowDiagnosticSeverity { warning, error };
struct AgentflowSourceSpan {
    std::size_t start_line{}, start_column{}, end_line{}, end_column{};
};
struct AgentflowDiagnostic {
    AgentflowDiagnosticCode code;
    AgentflowDiagnosticSeverity severity;
    std::u16string message, node_id;
    std::optional<AgentflowSourceSpan> position;
};
struct AgentflowNode {
    std::u16string id, label, shape = u"task", parent;
    ConfigValue metadata;
};
struct AgentflowEdge {
    std::u16string from, to, label;
    bool dashed = false;
    std::u16string id;
    ConfigValue metadata;
};
struct AgentflowContainer {
    std::u16string id, label, parent;
    ConfigValue metadata;
};
struct AgentflowDocument {
    std::u16string direction = u"TB";
    std::vector<AgentflowNode> nodes;
    std::vector<AgentflowEdge> edges;
    std::vector<AgentflowContainer> containers;
    std::vector<AgentflowDiagnostic> diagnostics;
};
std::expected<AgentflowDocument, std::string> parse_agentflow(std::u16string_view source);
} // namespace folia::mermaid
