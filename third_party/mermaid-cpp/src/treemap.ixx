// Mermaid 12.2.0 Treemap parser/DB, d3-hierarchy 3.1.2 layout. MIT / ISC; see ../THIRD_PARTY_NOTICES.md.
export module folia.mermaid.treemap;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
struct TreemapNode {std::u16string name,class_selector;std::vector<std::u16string> styles;std::vector<std::size_t> children;std::optional<double> own_value;std::size_t parent=0;int depth=0;double value=0,x0=0,y0=0,x1=0,y1=0;};
struct TreemapDocument {std::u16string title,acc_title,acc_description;std::vector<TreemapNode> nodes{TreemapNode{}};};
struct TreemapConfig {double width=960,height=500,padding=10,diagram_padding=8;bool show_values=true,use_max_width=true;std::u16string value_format=u",";};
std::expected<TreemapDocument,std::string> parse_treemap(std::u16string_view);
TreemapDocument layout_treemap(TreemapDocument,const TreemapConfig&);
}
