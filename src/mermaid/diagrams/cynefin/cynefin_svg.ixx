// Mermaid 12.2.0 Cynefin SVG renderer. MIT; see ../LICENSE.
export module folia.mermaid.cynefin_svg;
import std;
export import folia.mermaid.cynefin;
import folia.mermaid.frontend;
import folia.mermaid.svg_text_measure;
export namespace folia::mermaid {
struct CynefinTheme {
    std::u16string font_family=u"Arial";
    std::string complex_bg="#E8F5E9", complicated_bg="#E3F2FD", clear_bg="#FFF8E1", chaotic_bg="#FBE9E7", confusion_bg="#F3E5F5";
    std::string boundary_color="#333333", cliff_color="#8B0000", arrow_color="#333333", text_color="#333333", label_color="#333333";
    double boundary_width=2,cliff_width=4,arrow_width=2,domain_font_size=16,item_font_size=12;
};
struct CynefinConfig { double width=800,height=600,padding=40,boundary_amplitude=8; bool show_domain_descriptions=true,use_max_width=true; unsigned seed=0; };
struct CynefinRenderInput { CynefinDocument document; CynefinTheme theme; CynefinConfig config; };
struct CynefinSvg { std::string svg; double width=0,height=0; };
std::expected<CynefinRenderInput,std::string> prepare_cynefin_render(const PreparedDiagram&);
std::expected<CynefinSvg,std::string> render_cynefin_svg(const CynefinRenderInput&,const SvgTextMeasure&);
}
