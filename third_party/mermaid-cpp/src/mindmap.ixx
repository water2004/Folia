// Mermaid 12.2.0 Mindmap grammar and MindmapDB. MIT; see ../LICENSE.
export module folia.mermaid.mindmap;
export import folia.mermaid.frontend;
import std;

export namespace folia::mermaid {
struct MindmapNode {
    std::u16string id, node_id, label, shape, classes, icon;
    std::u16string label_type = u"markdown";
    int level = 0, type = 0;
    std::optional<int> section;
    std::optional<std::size_t> parent;
    double width = 200, height = 0, padding = 10;
};
struct MindmapEdge {
    std::u16string id, start, end, classes;
    int depth = 0;
    std::optional<int> section;
};
struct MindmapInput {
    std::u16string title, layout, color_by;
    double padding = 10, max_node_width = 200;
    std::vector<MindmapNode> nodes;
    std::vector<MindmapEdge> edges;
    ConfigValue effective_config;
};
// Owned parser/DB input for the measured shape and layout stages. In particular,
// an absent user layout selects CoSE despite the global default being ELK.
// No layout or public renderer is provided by this module alone.
std::expected<MindmapInput, std::string> prepare_mindmap(
    std::u16string_view source, const ConfigSession& session, FrontendLimits limits = {});
}
