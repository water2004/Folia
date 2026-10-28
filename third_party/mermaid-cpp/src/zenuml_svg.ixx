export module folia.mermaid.zenuml_svg;
export import folia.mermaid.zenuml;
import std;
export namespace folia::mermaid {
struct ZenTheme {std::u16string font_family=u"Helvetica, Verdana, serif";};
struct ZenRenderInput {ZenDocument document;ZenTheme theme;};
using ZenSvgMeasure=std::function<double(std::u16string_view,double,bool)>;
struct ZenSvg {std::string svg;double width{},height{};};
std::expected<ZenSvg,std::string> render_zenuml_svg(const ZenDocument&,const ZenSvgMeasure&);
}
