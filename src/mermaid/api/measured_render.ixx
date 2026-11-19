// The production source-to-SVG pipeline, with host-owned font and math services.
export module folia.mermaid.measured_render;
import std;
export import folia.mermaid.render_entry;
export import folia.mermaid.text_measurer;
export import folia.mermaid.math_label;

export namespace folia::mermaid {
std::expected<DiagramSvg, std::string> render_measured_diagram(
    NativeDiagram diagram, const TextMeasurer& measurer,
    const MathLabelRender& render_math = {},
    std::optional<double> available_width = {});
}
