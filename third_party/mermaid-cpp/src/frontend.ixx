// Official Mermaid 12.2.0 preprocess.ts / mermaidAPI.ts sequencing.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see ../LICENSE.
export module folia.mermaid.frontend;
import std;
export import folia.mermaid.config_session;
export import folia.mermaid.frontmatter;
export import folia.mermaid.directives;

export namespace folia::mermaid {
struct FrontendLimits {
    std::size_t source_units = 1024 * 1024;
    YamlLimits yaml;
    DirectiveLimits directives;
    ConfigLimits config;
};
struct DiagramCode {
    std::u16string raw;
    std::u16string cleaned;
    std::u16string with_comments;
    std::size_t frontmatter_line_offset = 0;
};
struct PreparedDiagram {
    DiagramCode code;
    std::optional<std::u16string> title;
    // Extracted frontmatter/directive configuration before session sanitizing.
    ConfigValue config;
    std::u16string type;
    // Configuration for family init/render, including final detector writes.
    ConfigValue effective_config;
};
// Compose preprocessing -> reset -> detect scope -> apply directive -> detect.
// A private session copy isolates requests; the caller's state/source are never
// changed, including on failure. Builtin detection does not imply an available
// parser/renderer. No grammar validation, entity encoding or SVG generation here.
std::expected<PreparedDiagram, std::string> prepare_diagram(
    std::u16string_view source, const ConfigSession& session, FrontendLimits limits = {});
}
