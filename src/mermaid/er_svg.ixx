// Entity Relationship SVG renderer, Mermaid 12.2.0. MIT; see ../LICENSE.
export module folia.mermaid.er_svg;
import std;
export import folia.mermaid.er;

export namespace folia::mermaid {
struct ErTextMetrics {
    double advance = 0, width = 0, height = 0, baseline = 0;
};
using ErSvgMeasure = std::function<std::expected<ErTextMetrics,std::string>(
    std::u16string_view,double,bool)>;
struct ErSvgTheme {
    std::u16string font_family = u"Arial";
    std::string background = "#ECECFF", border = "#9370DB", text = "#333333";
    std::string line = "#333333", title_color = "#333333";
    std::string label_background = "rgba(249,255,236,0.5)", edge_background = "rgba(232,232,232,0.8)";
    std::string attribute_odd = "#ffffff", attribute_even = "#f1f1ff";
    double font_size = 16, title_top_margin = 25;
};
struct ErRenderInput { ErInput document; ErSvgTheme theme; };
struct ErSvg { std::string svg; double width = 0, height = 0; };
std::expected<ErRenderInput,std::string> prepare_er_render(
    std::u16string_view, const ConfigSession&, ErLimits = {});
// Builds the classic Dagre ER scene from platform measured text and paints the
// entity rows, relationship paths, crow's-foot cardinalities and labels.
std::expected<ErSvg,std::string> render_er_svg(
    const ErRenderInput&, const ErSvgMeasure&);
}
