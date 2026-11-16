module;
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

export module folia.platform.mermaid_renderer;
import std;
import folia.platform.mermaid_measurer;
import folia.platform.mermaid_math;
export import folia.mermaid.measured_render;

export namespace folia::platform {
// The Windows host supplies the same DirectWrite and native MathJax services
// to the portable production pipeline exercised by the regression runner.
std::expected<folia::mermaid::DiagramSvg, std::string> render_mermaid(
    std::u16string_view source, const folia::mermaid::ConfigSession& session,
    folia::mermaid::FrontendLimits limits = {}, std::optional<double> available_width = {}) {
    auto input = folia::mermaid::prepare_native_diagram(source, session, limits);
    if (!input) return std::unexpected(input.error());
    if (!available_width && std::holds_alternative<folia::mermaid::C4RenderInput>(input->content)) {
        RECT area{};
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0))
            return std::unexpected("Cannot obtain C4 available work area");
        available_width = area.right - area.left;
    }
    MermaidMeasurer measurer;
    MermaidMathRenderer math_renderer;
    return folia::mermaid::render_measured_diagram(std::move(*input), measurer,
        [&](std::u16string_view tex, double size) { return math_renderer.render(tex, size); },
        available_width);
}
}
