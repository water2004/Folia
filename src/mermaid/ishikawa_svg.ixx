// Mermaid 12.2.0 Ishikawa SVG projection. MIT; see ../LICENSE.
export module folia.mermaid.ishikawa_svg;
import std;
export import folia.mermaid.ishikawa;
import folia.mermaid.frontend;
import folia.mermaid.svg_text_measure;
export namespace folia::mermaid {
struct IshikawaSvgTheme {
    std::u16string font_family=u"Arial"; double font_size=16, padding=20;
    std::string line="#333333", background="#ffffff", text="#333333";
};
struct IshikawaSvgConfig { double diagram_padding=20; bool use_max_width=false; };
struct IshikawaRenderInput { IshikawaDocument document; IshikawaSvgTheme theme; IshikawaSvgConfig config; };
struct IshikawaSvg { std::string svg; double width=0,height=0; };
std::expected<IshikawaRenderInput,std::string> prepare_ishikawa_render(const PreparedDiagram&);
std::expected<IshikawaSvg,std::string> render_ishikawa_svg(const IshikawaRenderInput&,const SvgTextMeasure&);
}
