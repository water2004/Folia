// Mermaid 12.2.0 infoRenderer.ts / setupGraphViewbox.js port; MIT.
module folia.mermaid.info_svg;
import std;
import folia.mermaid.svg_text;

namespace folia::mermaid {
std::expected<InfoRenderInput, std::string> prepare_info_render(const PreparedDiagram& prepared) {
    auto parsed = parse_info(prepared.code.cleaned);
    if (!parsed) return std::unexpected("Invalid Info diagram syntax at UTF-16 offset " + std::to_string(parsed.error().offset) + ".");
    InfoRenderInput result;
    result.document = std::move(*parsed);
    if (auto family = prepared.effective_config.find(u"fontFamily"); family && family->kind == ConfigKind::string)
        result.theme.font_family = family->string;
    if (auto vars = prepared.effective_config.find(u"themeVariables")) {
        if (auto color = vars->find(u"textColor"); color && color->kind == ConfigKind::string)
            result.theme.text_color = color->string;
    }
    return result;
}

std::expected<InfoSvg, InfoError> render_info_svg(InfoRenderInput const& input) {
    auto version = escape_svg_text(u"v" + input.document.version, u"Info version");
    auto color = escape_svg_text(input.theme.text_color, u"Info text color");
    if (!version) return std::unexpected(InfoError{0, std::move(version.error())});
    if (!color) return std::unexpected(InfoError{0, std::move(color.error())});
    auto family = escape_svg_text(input.theme.font_family, u"Info font family");
    if (!family) return std::unexpected(InfoError{0, std::move(family.error())});
    auto attr_escape = [](std::string value) {
        std::string escaped;
        for (char c : value) escaped += c == '"' ? "&quot;" : std::string(1, c);
        return escaped;
    };
    // configureSvgSize(100, 400, true) sets width and max-width only. It
    // deliberately omits height and viewBox; host intrinsic sizing is 400x100.
    std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100%\" style=\"max-width: 400px;\"><g></g><g><text x=\"100\" y=\"40\" class=\"version\" font-size=\"32\" font-family=\"";
    svg += attr_escape(*family);
    svg += "\" text-anchor=\"middle\" fill=\"";
    svg += attr_escape(*color);
    svg += "\">";
    svg += *version;
    svg += "</text></g></svg>";
    return InfoSvg{std::move(svg), 400, 100};
}
}
