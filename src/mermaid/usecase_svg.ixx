// Mermaid 12.2.0 usecase measured-Dagre SVG projection.
export module folia.mermaid.usecase_svg;
import std;
export import folia.mermaid.usecase;
import folia.mermaid.dagre_rank;
import folia.mermaid.svg_text_measure;
export namespace folia::mermaid {
struct UsecaseRenderTheme {
    std::u16string font_family = u"Arial", actor_font_family = u"Arial", usecase_font_family = u"Arial";
    double font_size = 12, actor_font_size = 14;
    std::string text = "#333333", actor_text = "#333333", line = "#333333";
    std::string background = "#ffffff", node_fill = "#ECECFF", node_border = "#9370DB";
    std::string actor_fill = "#ffffff", actor_border = "#333333", label_background = "#ffffff";
    std::string include_line = "#333333", extend_line = "#333333";
    std::string boundary_fill = "#ffffde", boundary_border = "#aaaa33", boundary_text = "#333333";
    std::string note_fill = "#fff5ad", note_border = "#aaaa33", note_text = "#333333";
    unsigned actor_weight = 400, usecase_weight = 400;
    double diagram_font_size = 16;
};
struct UsecaseRenderInput {
    UsecaseDocument document;
    UsecaseRenderTheme theme;
    double node_spacing=50,rank_spacing=50,diagram_padding=20,min_node_width=120, wrapping_width=120;
    double group_title_top = 0, group_title_bottom = 0;
    bool elk = true, neo = false, use_max_width = true;
};
struct UsecaseSvg { std::string svg; double width=0,height=0; };
using UsecaseTextMeasure = std::function<std::expected<SvgTextMetrics, std::string>(
    std::u16string_view, std::u16string_view, double, unsigned)>;
std::expected<UsecaseRenderInput,std::string> prepare_usecase_render(const PreparedDiagram&);
std::expected<UsecaseSvg,std::string> render_usecase_svg(const UsecaseRenderInput&,const UsecaseTextMeasure&);
}
