export module folia.mermaid.zenuml_svg;
export import folia.mermaid.zenuml;
import std;
export namespace folia::mermaid {
using ZenSvgMeasure=std::function<double(std::u16string_view,double,bool)>;
struct ZenSvg {std::string svg;double width{},height{};};
std::expected<ZenSvg,std::string> render_zenuml_svg(const ZenDocument&,const ZenSvgMeasure&);
}
