// Static error diagram from Mermaid 12.2.0 errorRenderer.ts. MIT, see LICENSE.
module folia.mermaid.error_svg;
import std;
import folia.mermaid.svg_text;
import folia.mermaid.info;

namespace folia::mermaid {
namespace {
constexpr std::array<std::string_view, 6> icon_paths{
#include "error_icon.inc"
};

std::string attribute(std::u16string_view value) {
    const auto text = escape_svg_text(value, u"Error diagram style");
    if (!text) throw std::runtime_error("Invalid error diagram style");
    std::string result;
    for (const char c : *text) {
        if (c == '"') result += "&quot;";
        else result += c;
    }
    return result;
}
}

ErrorRenderInput prepare_error_render(const PreparedDiagram& prepared) {
    const auto read = [](const ConfigValue* value, std::u16string_view key,
                         std::u16string_view fallback) -> std::u16string {
        const auto* entry = value ? value->find(key) : nullptr;
        return entry && entry->kind == ConfigKind::string ? entry->string : std::u16string{fallback};
    };
    const auto* variables = prepared.effective_config.find(u"themeVariables");
    return {{
        read(&prepared.effective_config, u"fontFamily", u"Arial"),
        read(variables, u"errorBkgColor", u"#552222"),
        read(variables, u"errorTextColor", u"#552222"),
    }};
}

std::expected<ErrorSvg, std::string> render_error_svg(const ErrorRenderInput& input) {
    try {
        const auto family = attribute(input.theme.font_family);
        const auto icon = attribute(input.theme.icon_color);
        const auto text = attribute(input.theme.text_color);
        const auto version = escape_svg_text(info_version, u"Mermaid version");
        if (!version) return std::unexpected("Invalid Mermaid version text");
        std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100%\" "
            "viewBox=\"0 0 2412 512\" style=\"max-width: 512px;\" font-family=\"" + family + "\"><g>";
        for (const auto path : icon_paths) {
            svg += "<path class=\"error-icon\" fill=\"" + icon + "\" d=\"";
            svg += path;
            svg += "\"/>";
        }
        const auto add_label = [&](std::string_view x, std::string_view y,
                                   std::string_view size, std::string_view label) {
            svg += "<text class=\"error-text\" x=\"";
            svg += x;
            svg += "\" y=\"";
            svg += y;
            svg += "\" font-size=\"";
            svg += size;
            svg += "\" text-anchor=\"middle\" fill=\"" + text + "\" stroke=\"" + text + "\">";
            svg += label;
            svg += "</text>";
        };
        add_label("1440", "250", "150", "Syntax error in text");
        add_label("1250", "400", "100", "mermaid version " + *version);
        svg += "</g></svg>";
        return ErrorSvg{std::move(svg), 2412, 512};
    } catch (const std::runtime_error& e) {
        return std::unexpected(e.what());
    }
}
}
