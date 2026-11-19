// Portable host-facing source-to-SVG facade. Font and math services stay owned
// by the caller; preparation, configuration and rendering use one pipeline.
export module folia.mermaid.renderer;
import std;
export import folia.mermaid.measured_render;
export import folia.mermaid.theme_options;

export namespace folia::mermaid {
// Reuse a configured session for several requests. Preparation copies its
// state, so source-level directives never leak into the next diagram.
std::expected<DiagramSvg, std::string> render_diagram(
    std::u16string_view source, const TextMeasurer& measurer,
    const ConfigSession& session, const MathLabelRender& render_math = {},
    FrontendLimits limits = {}, std::optional<double> available_width = {});

// Select a built-in theme or customize its variables without constructing a
// configuration tree. Omitted options preserve the pinned family defaults.
// Hosts needing arbitrary configuration or session budgets use the overload
// above with their own initialized ConfigSession.
std::expected<DiagramSvg, std::string> render_diagram(
    std::u16string_view source, const TextMeasurer& measurer,
    const ThemeOptions& theme = {}, const MathLabelRender& render_math = {},
    FrontendLimits limits = {}, std::optional<double> available_width = {});
}
