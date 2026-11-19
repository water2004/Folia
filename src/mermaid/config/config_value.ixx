// Owned values at the Mermaid configuration boundary. No renderer defaults here.
export module folia.mermaid.config_value;
import std;

export namespace folia::mermaid {
enum class ConfigKind { null, boolean, number, string, sequence, mapping };
struct ConfigValue {
    ConfigKind kind = ConfigKind::null;
    bool boolean = false;
    double number = 0;
    std::u16string string;
    std::vector<ConfigValue> sequence;
    std::map<std::u16string, ConfigValue> mapping;
    [[nodiscard]] const ConfigValue* find(std::u16string_view key) const {
        if (kind != ConfigKind::mapping) return nullptr;
        const auto found = mapping.find(std::u16string(key));
        return found == mapping.end() ? nullptr : &found->second;
    }
};
// JavaScript truthiness and String conversion, including arrays and non-finite
// numbers. An own non-callable toString on a mapping produces an error.
bool config_truthy(const ConfigValue& value);
std::expected<std::u16string, std::string> config_to_string(const ConfigValue& value);
}
