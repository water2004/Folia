// Mermaid 12.2.0 Agentflow semantic subset. MIT; see ../LICENSE.
export module folia.mermaid.agentflow;
import std;
export namespace folia::mermaid {
struct AgentflowNode {
    std::u16string id, label, shape = u"task", parent;
};
struct AgentflowEdge {
    std::u16string from, to, label;
    bool dashed = false;
};
struct AgentflowContainer {
    std::u16string id, label, parent;
};
struct AgentflowDocument {
    std::u16string direction = u"TB";
    std::vector<AgentflowNode> nodes;
    std::vector<AgentflowEdge> edges;
    std::vector<AgentflowContainer> containers;
};
std::expected<AgentflowDocument, std::string> parse_agentflow(std::u16string_view source);
} // namespace folia::mermaid
