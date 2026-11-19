// Explicit error diagram from Mermaid 12.2.0. MIT, see LICENSE.
export module folia.mermaid.error_svg;
import std;
import folia.mermaid.frontend;

export namespace folia::mermaid {
struct ErrorSvgTheme {
    std::u16string font_family, icon_color, text_color;
};
struct ErrorRenderInput { ErrorSvgTheme theme; };
struct ErrorSvg { std::string svg; double width{}, height{}; };

ErrorRenderInput prepare_error_render(const PreparedDiagram&);
std::expected<ErrorSvg, std::string> render_error_svg(const ErrorRenderInput&);
}
