// Mermaid 12.2.0 Use Case SVG rendering with measured Dagre and ELK layouts.
export module folia.mermaid.usecase_svg;
import std;
export import folia.mermaid.usecase;
import folia.mermaid.dagre_rank;
import folia.mermaid.svg_text_measure;
export namespace folia::mermaid {
struct UsecaseRenderTheme {
    std::u16string font_family = u"Arial", actor_font_family = u"Arial", usecase_font_family = u"Arial";
    double font_size = 12, actor_font_size = 14;
    std::string text = "#333333", actor_text = "#333333", svg_text = "#333333", line = "#333333";
    std::string background = "#ffffff", node_fill = "#ECECFF", node_border = "#9370DB";
    std::string actor_fill = "#ffffff", actor_border = "#333333", label_background = "#ffffff";
    std::string include_line = "#333333", extend_line = "#333333";
    std::string boundary_fill = "#ffffde", boundary_border = "#aaaa33", boundary_text = "#333333";
    std::string note_fill = "#fff5ad", note_border = "#aaaa33", note_text = "#333333";
    std::string drop_shadow;
    std::string rough_border = "#9370DB";
    std::vector<std::string> palette_fill, palette_border;
    unsigned actor_weight = 400, usecase_weight = 400;
    double diagram_font_size = 16, neo_actor_path_width = 1;
};
struct UsecaseRenderInput {
    UsecaseDocument document;
    UsecaseRenderTheme theme;
    double node_spacing=50,rank_spacing=50,diagram_padding=20,min_node_width=120, wrapping_width=120;
    double group_title_top = 0, group_title_bottom = 0;
    bool elk = true, neo = false, hand_drawn = false, use_max_width = true, dark = false, rotate_colors = false;
    bool html_labels = true;
    std::uint32_t hand_drawn_seed = 0;
};
struct UsecaseSvg { std::string svg; double width=0,height=0; };
using UsecaseTextMeasure = SvgFamilyTextMeasure;
std::expected<UsecaseRenderInput,std::string> prepare_usecase_render(const PreparedDiagram&);
std::expected<UsecaseSvg,std::string> render_usecase_svg(const UsecaseRenderInput&,const UsecaseTextMeasure&);
}
