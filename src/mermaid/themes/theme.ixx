// Official Mermaid 12.2.0 theme-variable derivation.
export module folia.mermaid.theme;
import std;
export import folia.mermaid.config_limits;

export namespace folia::mermaid {
enum class MermaidTheme {
    default_theme, dark, base, forest, neutral, neo, neo_dark, redux,
    redux_dark, redux_color, redux_dark_color
};
struct MermaidThemeInfo {
    MermaidTheme id;
    std::u16string_view name;
};
// Names are the pinned Mermaid configuration identifiers. The catalog and its
// string views have static lifetime; an invalid enum has no name.
std::span<const MermaidThemeInfo> mermaid_themes();
std::optional<std::u16string_view> mermaid_theme_name(MermaidTheme theme);
std::optional<MermaidTheme> find_mermaid_theme(std::u16string_view name);
// Pure owned result, no session singleton. Overrides must be a mapping; nullptr
// means omitted. Undefined derived fields are omitted, matching JSON output.
// Preserves unknown data keys and shallow-merges generated object variables.
// Rejects prototype/method keys, non-string evaluated colors, color literals
// over 4096 units and THEME_COLOR_LIMIT outside integer 0..12. Budget failures
// return errors without mutating inputs or exposing a partial result.
std::expected<ConfigValue,std::string> derive_theme_variables(
    MermaidTheme theme, const ConfigValue* overrides = nullptr, ConfigLimits limits = {});
}
