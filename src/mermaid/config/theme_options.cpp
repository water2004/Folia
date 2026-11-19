module folia.mermaid.theme_options;
import std;
import folia.mermaid.config_support;

namespace folia::mermaid {
std::expected<ConfigSession, std::string> make_theme_session(
    const ThemeOptions& options, ConfigSessionLimits limits) {
    try {
        // Inspect before copying public values, including inactive payloads.
        config_detail::Budget budget{limits.config};
        budget.inspect(options.variables);
        if (options.variables.kind != ConfigKind::mapping)
            return std::unexpected("Theme variables must be a mapping");

        ConfigValue config{.kind = ConfigKind::mapping};
        if (options.theme) {
            const auto name = mermaid_theme_name(*options.theme);
            if (!name) return std::unexpected("Unsupported native theme");
            config.mapping.emplace(u"theme", config_detail::string(*name));
        }
        if (!options.variables.mapping.empty())
            config.mapping.emplace(u"themeVariables", options.variables);

        auto session = ConfigSession::create(limits);
        if (!session) return std::unexpected(session.error());
        if (auto configured = session->initialize(config); !configured)
            return std::unexpected(configured.error());
        return session;
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}
}
