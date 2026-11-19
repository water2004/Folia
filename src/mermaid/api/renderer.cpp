module folia.mermaid.renderer;
import std;

namespace folia::mermaid {
std::expected<DiagramSvg, std::string> render_diagram(
    std::u16string_view source, const TextMeasurer& measurer,
    const ConfigSession& session, const MathLabelRender& render_math,
    FrontendLimits limits, std::optional<double> available_width) {
    auto diagram = prepare_native_diagram(source, session, limits);
    if (!diagram) return std::unexpected(diagram.error());
    return render_measured_diagram(std::move(*diagram), measurer,
        render_math, available_width);
}

std::expected<DiagramSvg, std::string> render_diagram(
    std::u16string_view source, const TextMeasurer& measurer,
    const ThemeOptions& theme, const MathLabelRender& render_math,
    FrontendLimits limits, std::optional<double> available_width) {
    auto session = make_theme_session(theme);
    if (!session) return std::unexpected(session.error());
    return render_diagram(source, measurer, *session, render_math,
        limits, available_width);
}
}
