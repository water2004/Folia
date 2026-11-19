// Mermaid 12.2.0 diagram-api/frontmatter.ts. MIT, see ../LICENSE.
export module folia.mermaid.frontmatter;
import std;
export import folia.mermaid.config_value;

export namespace folia::mermaid {
struct YamlLimits {
    std::size_t source_units = 1024 * 1024;
    std::size_t depth = 64;
    std::size_t values = 100000;
    std::size_t expanded_units = 4 * 1024 * 1024;
};
struct FrontMatterMetadata {
    std::optional<std::u16string> title;
    std::optional<std::u16string> display_mode;
    std::optional<ConfigValue> config;
};
struct FrontMatterResult {
    std::u16string text;
    FrontMatterMetadata metadata;
};
// One YAML document, js-yaml 4.3.0 JSON_SCHEMA. Syntax/schema/limit errors are
// explicit. Aliases are copied by value within budgets; cyclic aliases fail.
// UTF-16 input must be well-formed; no platform API or JS runtime is involved.
std::expected<ConfigValue, std::string> parse_config_yaml(
    std::u16string_view source, YamlLimits limits = {});
// Owns the remaining source and metadata; does not normalize the diagram body,
// merge settings, interpret directives, apply defaults or discard config keys.
std::expected<FrontMatterResult, std::string> extract_front_matter(
    std::u16string_view source, YamlLimits limits = {});
}
