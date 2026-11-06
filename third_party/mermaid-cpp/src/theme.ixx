// Official Mermaid 12.2.0 default/dark theme-variable derivation.
export module folia.mermaid.theme;
import std;
export import folia.mermaid.config_limits;

export namespace folia::mermaid {
enum class MermaidTheme { default_theme, dark };
// Pure owned result, no session singleton. Overrides must be a mapping; nullptr
// means omitted. Undefined derived fields are omitted, matching JSON output.
// Preserves unknown data keys and shallow-merges generated object variables.
// Rejects prototype/method keys, non-string evaluated colors, color literals
// over 4096 units and THEME_COLOR_LIMIT outside integer 0..12. Budget failures
// return errors without mutating inputs or exposing a partial result.
std::expected<ConfigValue,std::string> derive_theme_variables(
    MermaidTheme theme, const ConfigValue* overrides = nullptr, ConfigLimits limits = {});
}
