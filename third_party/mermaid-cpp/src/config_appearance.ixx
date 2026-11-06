// Mermaid 12.2.0 config.ts resolveAppearance and diagramConfigKeys.ts. MIT.
export module folia.mermaid.config_appearance;
import std;
export import folia.mermaid.config_limits;

export namespace folia::mermaid {
std::u16string diagram_config_key(std::u16string_view diagram_type);
// Selects theme/look/layout only. The caller supplies the already-merged config
// and sanitized directive layers. Precedence: sum, saved directives in reverse,
// initialize delta, pinned defaults; each layer checks diagram scope before root.
// Missing scope leaves cfg unchanged. This does not derive theme color variables,
// detect diagram syntax, sanitize inputs or apply configuration to a renderer.
std::expected<ConfigValue, std::string> resolve_config_appearance(
    const ConfigValue& config, const ConfigValue& directive_sum,
    const std::vector<ConfigValue>& saved_directives, const ConfigValue& site_delta,
    std::optional<std::u16string_view> diagram_type, ConfigLimits limits = {});
}
