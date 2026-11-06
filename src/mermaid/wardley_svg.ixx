// Mermaid 12.2.0 Wardley SVG renderer. MIT; see THIRD_PARTY_NOTICES.md.
export module folia.mermaid.wardley_svg;
import std;
export import folia.mermaid.wardley;
export import folia.mermaid.frontend;
import folia.mermaid.svg_text_measure;
export namespace folia::mermaid {
struct WardleyTheme { std::u16string font_family=u"Arial"; std::string background="#ffffff",axis="#000000",text="#222222",grid="rgba(100, 100, 100, 0.2)",component_fill="#ffffff",component_stroke="#000000",link="#000000",evolution="#dc3545",annotation_stroke="#000000",annotation_text="#222222",annotation_fill="#ffffff"; };
struct WardleyRenderInput { WardleyDiagram document; WardleyTheme theme; double padding=48,node_radius=6,label_offset=8,axis_font=12,label_font=10; bool show_grid=false,use_max_width=true; };
struct WardleySvg { std::string svg; double width=0,height=0; };
std::expected<WardleyRenderInput,std::string> prepare_wardley_render(const PreparedDiagram&);
std::expected<WardleySvg,std::string> render_wardley_svg(const WardleyRenderInput&,const SvgTextMeasure&);
}
