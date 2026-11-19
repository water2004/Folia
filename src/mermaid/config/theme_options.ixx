// Typed host theme selection; the existing session owns configuration semantics.
export module folia.mermaid.theme_options;
import std;
export import folia.mermaid.theme;
export import folia.mermaid.config_session;

export namespace folia::mermaid {
struct ThemeOptions {
    // Omitted retains each diagram family's pinned default. An explicit
    // default_theme selects "default" even for families whose default is redux.
    std::optional<MermaidTheme> theme;
    // Mermaid themeVariables, before derivation. Nested family variables are
    // supported by the same bounded configuration rules as initialize().
    ConfigValue variables{.kind = ConfigKind::mapping};
};

// Builds an independent initialized session. Source frontmatter/directives can
// override the selection, and look/layout retain their own family defaults.
// Use MermaidTheme::base for a custom palette. Inputs are never modified or
// retained by reference; invalid options return an error without a session.
std::expected<ConfigSession, std::string> make_theme_session(
    const ThemeOptions& options = {}, ConfigSessionLimits limits = {});
}
