// Mermaid 12.2.0 requirement diagram layout and SVG. MIT; see ../LICENSE.
export module folia.mermaid.requirement_svg;
import std;
export import folia.mermaid.requirement;

export namespace folia::mermaid {
struct RequirementTextMetrics {
    // SVG font ascent and ascent/descent box. HTML labels use their CSS line box.
    double advance=0, ink_width=0, bounds_width=0, svg_height=0, svg_baseline=0;
};
using RequirementSvgMeasure = std::function<std::expected<RequirementTextMetrics,std::string>(
    std::u16string_view,double,bool)>;
struct RequirementSvgTheme {
    std::u16string font_family=u"Arial";
    double font_size=16, padding=20, gap=20, stroke_width=1.3;
    std::string fill="#ECECFF", border="#9370DB", text="#333333";
    std::string line="#333333", label_background="rgba(232,232,232,0.8)", relation_label="#333333";
    std::string title_color="#333333";
};
struct RequirementSvgConfig {
    std::u16string layout=u"elk";
    double diagram_padding=8, node_spacing=50, rank_spacing=50, title_top_margin=25;
    bool html_labels=true, use_max_width=true;
};
struct RequirementRenderInput {
    RequirementDocument document;
    RequirementSvgTheme theme;
    RequirementSvgConfig config;
};
struct RequirementSvg { std::string svg; double width=0, height=0; };
std::expected<RequirementSvg,std::string> render_requirement_svg(
    const RequirementRenderInput&,const RequirementSvgMeasure&);
}
