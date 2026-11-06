// Mermaid 12.2.0 Railroad grammars and AST transforms. MIT; see THIRD_PARTY_NOTICES.md.
export module folia.mermaid.railroad;
import std;

export namespace folia::mermaid {

enum class RailroadNodeKind { terminal, nonterminal, sequence, choice, optional, repetition, special };

struct RailroadNode {
    RailroadNodeKind kind{};
    std::u16string text;
    std::vector<RailroadNode> children;
    double min{};
    double max{std::numeric_limits<double>::infinity()};
};

struct RailroadRule {
    std::u16string name;
    RailroadNode definition;
};

struct RailroadDocument {
    std::vector<RailroadRule> rules;
    std::u16string title, acc_title, acc_description;
};

std::expected<RailroadDocument, std::string> parse_railroad(std::u16string_view source);

}
