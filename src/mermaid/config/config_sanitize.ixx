// Mermaid 12.2.0 utils/sanitizeDirective.ts and config.ts sanitize. MIT.
export module folia.mermaid.config_sanitize;
import std;
export import folia.mermaid.config_limits;

export namespace folia::mermaid {
// Both operations copy their input and publish only a complete result. These
// are distinct upstream policies, not general HTML/CSS security validators.
std::expected<ConfigValue, std::string> sanitize_directive_config(
    const ConfigValue& value, ConfigLimits limits = {});
// Secure names come from the site config. "secure" itself is always protected.
// Removing an array index would create a JS sparse array; owned trees reject
// that shape explicitly instead of shifting indices or replacing holes by null.
std::expected<ConfigValue, std::string> sanitize_secure_config(
    const ConfigValue& value, const std::vector<std::u16string>& secure,
    ConfigLimits limits = {});
}
