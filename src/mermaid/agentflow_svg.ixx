// Mermaid 12.2.0 Agentflow SVG adapter. MIT; see ../LICENSE.
export module folia.mermaid.agentflow_svg;
import std;
export import folia.mermaid.agentflow;
import folia.mermaid.frontend;
import folia.mermaid.svg_text_measure;
import folia.mermaid.flowchart_svg;
export namespace folia::mermaid {
struct AgentflowSvgTheme {
    std::u16string font_family = u"Arial";
    std::string text = "#333333", background = "#ffffff", line = "#333333";
    std::vector<std::string> border_colors, bkg_colors;
    double font_size = 16, diagram_padding = 8, min_node_width = 120, wrapping_width = 120;
    bool neo = true;
};
struct AgentflowRenderInput {
    AgentflowDocument document;
    AgentflowSvgTheme theme;
};
struct AgentflowSvg {
    std::string svg;
    double width = 0, height = 0;
};
std::expected<AgentflowRenderInput, std::string> prepare_agentflow_render(const PreparedDiagram &);
std::expected<AgentflowSvg, std::string> render_agentflow_svg(const AgentflowRenderInput &,
                                                              const SvgTextMeasure &);
} // namespace folia::mermaid
