module folia.mermaid.config_init;
import std;
import folia.mermaid.config_support;
import folia.mermaid.config_sanitize;
import folia.mermaid.config_merge;

namespace folia::mermaid {
std::expected<std::optional<ConfigValue>, std::string> detect_init_config(
    std::u16string_view source, const InitDiagramResolver& resolve_diagram,
    ConfigLimits limits, const DirectiveLimits& directive_limits) {
    try {
        auto directives = detect_directives(source, DirectiveFilter::init, directive_limits);
        if (!directives) return std::unexpected(directives.error());
        config_detail::Budget budget{limits};
        ConfigValue result;
        if (directives->size() == 1) {
            budget.inspect(directives->front().args);
            result = std::move(directives->front().args);
        } else {
            ConfigValue args; args.kind = ConfigKind::sequence;
            for (auto& directive : *directives) args.sequence.push_back(std::move(directive.args));
            auto sanitized = sanitize_directive_config(args, limits);
            if (!sanitized) return std::unexpected(sanitized.error());
            auto merged = assign_config_with_depth(config_detail::map(), *sanitized, 2, limits);
            if (!merged) return std::unexpected(merged.error());
            result = std::move(*merged);
        }
        if (!config_truthy(result)) return std::optional<ConfigValue>{};
        auto type = resolve_diagram(source);
        if (!type) return std::unexpected(type.error());
        budget.text(type->size());
        if (result.kind == ConfigKind::mapping) {
            const auto found = result.mapping.find(u"config");
            if (found != result.mapping.end()) {
                if (*type == u"flowchart-v2") *type = u"flowchart";
                // Assignment occurs before delete: a diagram literally named
                // `config` loses that property, as the official function does.
                auto value = std::move(found->second);
                if (*type == u"__proto__" && !result.mapping.contains(*type)) {
                    if (value.kind == ConfigKind::mapping || value.kind == ConfigKind::sequence || value.kind == ConfigKind::null)
                        return std::unexpected("Init mapping would invoke a JavaScript prototype setter");
                    // Object.prototype's setter ignores primitive arguments.
                } else result.mapping.insert_or_assign(*type, std::move(value));
                result.mapping.erase(u"config");
            }
        }
        budget.output(result);
        return std::optional<ConfigValue>{std::move(result)};
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
