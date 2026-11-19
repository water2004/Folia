// Mermaid 12.2.0 Info SVG renderer; MIT; see ../LICENSE.
export module folia.mermaid.info_svg;
import std;
export import folia.mermaid.info;
export import folia.mermaid.frontend;
export namespace folia::mermaid {
struct InfoSvgTheme { std::u16string font_family = u"Arial"; std::u16string text_color = u"black"; };
struct InfoRenderInput { InfoDocument document; InfoSvgTheme theme; };
struct InfoSvg { std::string svg; double width{}, height{}; };
std::expected<InfoRenderInput, std::string> prepare_info_render(const PreparedDiagram& prepared);
std::expected<InfoSvg, InfoError> render_info_svg(InfoRenderInput const& input);
}
