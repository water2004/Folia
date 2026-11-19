// Mermaid 12.2.0 treeView-beta syntax and indentation projection. MIT; see ../LICENSE.
export module folia.mermaid.tree_view;
import std;
export import folia.mermaid.frontend;
export namespace folia::mermaid {
enum class TreeViewNodeType { file, directory };
struct TreeViewNode {
    std::size_t id{}, level{};
    std::u16string name;
    TreeViewNodeType type{TreeViewNodeType::file};
    std::u16string css_class, icon, description;
};
struct TreeViewDocument {
    std::vector<TreeViewNode> nodes;
    std::u16string title, acc_title, acc_description;
};
// Supports Mermaid's indentation syntax, quoted names, node annotations and
// Unicode light/heavy box drawing. SVG rendering resolves Mermaid's built-in
// file/folder icons and uses the upstream missing-icon glyph for unregistered packs.
std::expected<TreeViewDocument,std::string> parse_tree_view(std::u16string_view source);
struct TreeViewSvgTheme {
    std::u16string font_family=u"Arial";
    double label_font_size=16;
    std::string label_color="black", line_color="black", icon_color="#546e7a";
    std::string description_color="#6a9955";
    std::string highlight_bg="rgba(255, 193, 7, 0.15)", highlight_stroke="#ffc107";
};
struct TreeViewSvgConfig {
    double row_indent=10, padding_x=5, padding_y=5, line_thickness=1;
    bool show_icons=false, use_max_width=true;
    std::u16string default_icon_pack;
    std::map<std::u16string, std::u16string> filename_icons, extension_icons;
};
std::u16string resolve_tree_view_icon(const TreeViewNode&, const TreeViewSvgConfig&);
struct TreeViewRenderInput {
    TreeViewDocument document;
    TreeViewSvgTheme theme;
    TreeViewSvgConfig config;
};
std::expected<TreeViewRenderInput,std::string> prepare_tree_view_render(const PreparedDiagram&);
}
