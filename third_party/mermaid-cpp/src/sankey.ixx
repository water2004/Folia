// Mermaid 12.2.0 Sankey DB and d3-sankey 0.12.3. MIT; see ../LICENSE.
export module folia.mermaid.sankey;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
struct SankeyNode {std::u16string id;double value=0,x0=0,x1=0,y0=0,y1=0;int depth=0,height=0,layer=0;std::vector<std::size_t> outgoing,incoming;};
struct SankeyLink {std::size_t source=0,target=0;double value=0,width=0,y0=0,y1=0;};
struct SankeyGraph {std::vector<SankeyNode> nodes;std::vector<SankeyLink> links;};
struct SankeyConfig {double width=600,height=400,node_width=10,node_padding=12;bool show_values=false,use_max_width=true;std::u16string alignment=u"justify",prefix,suffix,link_color=u"gradient",label_style=u"legacy";std::map<std::u16string,std::u16string> node_colors;};
std::expected<SankeyGraph,std::string> parse_sankey(std::u16string_view);
std::expected<SankeyGraph,std::string> layout_sankey(SankeyGraph,const SankeyConfig&);
}
