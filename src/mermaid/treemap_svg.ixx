// Mermaid 12.2.0 Treemap renderer. MIT; see ../LICENSE.
export module folia.mermaid.treemap_svg;
export import folia.mermaid.treemap;
import std;
export namespace folia::mermaid {
struct TreemapTheme {std::u16string font_family,title_color,text_color;double title_size=14;std::vector<std::u16string> colors,peer_colors,label_colors;};
struct TreemapRenderInput {TreemapDocument document;TreemapConfig config;TreemapTheme theme;};
struct TreemapTextBox {double advance=0,width=0,height=0,baseline=0,middle_offset=0;};
using TreemapMeasure=std::function<std::expected<TreemapTextBox,std::string>(std::u16string_view,double,unsigned,bool)>;
struct TreemapSvg {std::string svg;double width=0,height=0;};
std::expected<TreemapRenderInput,std::string> prepare_treemap(std::u16string_view,const ConfigSession&);
std::expected<TreemapSvg,std::string> render_treemap_svg(const TreemapRenderInput&,const TreemapMeasure&);
}
