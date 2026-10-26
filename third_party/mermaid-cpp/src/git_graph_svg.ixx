// Mermaid 12.2.0 Git graph measured layout and SVG. MIT; see ../LICENSE.
export module folia.mermaid.git_graph_svg;
export import folia.mermaid.git_graph;
import std;
export namespace folia::mermaid {
struct GitGraphConfig {double padding=8,title_margin=25,font_size=16,commit_font_size=10,tag_font_size=10;bool show_branches=true,show_commit_labels=true,rotate_labels=true,parallel=false,dark=false;};
struct GitGraphTheme {std::u16string font_family;ConfigValue colors;};
struct GitGraphRenderInput {GitGraphDocument document;GitGraphConfig config;GitGraphTheme theme;};
struct GitGraphTextBox {double width=0,height=0,baseline=0;};
using GitGraphMeasure=std::function<std::expected<GitGraphTextBox,std::string>(std::u16string_view,double)>;
struct GitGraphSvg {std::string svg;double width=0,height=0;};
std::expected<GitGraphRenderInput,std::string> prepare_git_graph(std::u16string_view,const ConfigSession&);
std::expected<GitGraphSvg,std::string> render_git_graph_svg(const GitGraphRenderInput&,const GitGraphMeasure&);
}
