// Source-generated key registry and appearance defaults, not a runtime schema validator.
export module folia.mermaid.config_data;
import std;
import folia.mermaid.config_value;

namespace {
#include "config_schema_data.inc"
}
export namespace folia::mermaid::config_detail {
bool known_key(std::u16string_view key) {
    return std::binary_search(std::begin(config_keys), std::end(config_keys), key);
}
bool known_theme(std::u16string_view name) {
    return std::binary_search(std::begin(theme_names), std::end(theme_names), name);
}
std::optional<std::u16string_view> default_appearance(std::u16string_view section, std::u16string_view key) {
    for (const auto& item : appearance_defaults) if (item.section == section && item.key == key) return item.value;
    return {};
}
}
