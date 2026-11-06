// Mermaid 12.2.0 Venn syntax. MIT; see THIRD_PARTY_NOTICES.md.
export module folia.mermaid.venn;
import std;

export namespace folia::mermaid {
using VennSets = std::vector<std::u16string>;
using VennPaint = std::map<std::u16string, std::u16string, std::less<>>;
struct VennArea {
    VennSets sets;
    double size;
    std::u16string label;
};
struct VennTextNode {
    VennSets sets;
    std::u16string id, label;
};
struct VennDiagram {
    std::u16string title;
    std::vector<VennArea> areas;
    std::vector<VennTextNode> texts;
    std::map<VennSets, VennPaint> styles;
};
std::expected<VennDiagram, std::string> parse_venn(std::u16string_view source);
}
