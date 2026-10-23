// Official Mermaid 12.2.0 config.ts lifecycle, with explicit owned sessions.
export module folia.mermaid.config_session;
import std;
export import folia.mermaid.config_limits;

export namespace folia::mermaid {
struct ConfigSessionLimits {
    // Aggregate retained state, command input and intermediate tree walks.
    // Each delegated merge/sanitizer/theme call also uses these work limits.
    ConfigLimits config;
    std::size_t directives = 256; // Hard ceiling 256; bounds replay work.
};

// JSON-data projection of original defaults. Undefined properties and family
// font callbacks are absent. This is not a callback or renderer implementation.
ConfigValue default_session_config();

class ConfigSession {
public:
    static std::expected<ConfigSession, std::string> create(ConfigSessionLimits limits = {});
    // Returned trees and inputs never alias session state. A failed command
    // leaves every state field, including saved directives and warnings, intact.
    ConfigValue site_config() const { return site_; }
    ConfigValue current_config() const { return current_; }
    std::expected<ConfigValue, std::string> user_config() const;
    bool deprecated_diagrams_warning() const { return warned_; }

    std::expected<void, std::string> set_site_config(const ConfigValue& config);
    std::expected<void, std::string> update_site_config(const ConfigValue& config);
    std::expected<void, std::string> save_initialize_config(const ConfigValue& config);
    std::expected<void, std::string> set_config(const ConfigValue& config);
    std::expected<void, std::string> add_directive(const ConfigValue& config);
    // nullptr resets to site config; an explicit reset changes current only.
    // Both clear saved directives and diagram scope, retaining initialize data.
    std::expected<void, std::string> reset(const ConfigValue* config = nullptr);
    std::expected<void, std::string> set_diagram_scope(
        std::optional<std::u16string_view> diagram_type = {});
private:
    explicit ConfigSession(ConfigSessionLimits limits) : limits_(limits) {}
    enum class Operation { site, update, save, set, add, reset, scope };
    std::expected<void, std::string> apply(Operation operation, const ConfigValue* input,
        std::optional<std::u16string_view> scope = {});
    void inspect_state() const;
    ConfigSessionLimits limits_;
    ConfigValue site_, initialize_, delta_, current_;
    std::vector<ConfigValue> directives_;
    std::optional<std::u16string> scope_;
    bool warned_ = false;
};
}
