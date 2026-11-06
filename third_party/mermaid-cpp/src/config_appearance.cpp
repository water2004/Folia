module folia.mermaid.config_appearance;
import std;
import folia.mermaid.config_support;
import folia.mermaid.config_data;

namespace folia::mermaid {
std::u16string diagram_config_key(std::u16string_view type) {
    if (type == u"flowchart-v2" || type == u"flowchart-elk") return u"flowchart";
    if (type == u"classDiagram") return u"class";
    if (type == u"stateDiagram") return u"state";
    if (type == u"xychart") return u"xyChart";
    if (type == u"railroadAbnf" || type == u"railroadEbnf" || type == u"railroadPeg") return u"railroad";
    return std::u16string{type};
}
namespace {
using namespace config_detail;
bool usable(std::u16string_view key, const ConfigValue& value) {
    if (key == u"theme") {
        // Object.hasOwn performs ToPropertyKey, but preserve the selected value.
        const auto name = config_to_string(value);
        if (!name) throw std::runtime_error(name.error());
        return (value.kind == ConfigKind::string && value.string == u"null") || known_theme(*name);
    }
    if (key == u"look") return value.kind == ConfigKind::string &&
        (value.string == u"classic" || value.string == u"handDrawn" || value.string == u"neo");
    return true; // Layout is extensible. Upstream accepts null and non-string values too.
}
const ConfigValue* read(const ConfigValue& layer, std::u16string_view section, std::u16string_view key, Budget& budget) {
    budget.step();
    const auto scoped = layer.find(section);
    if (scoped) if (const auto value = scoped->find(key); value && usable(key, *value)) return value;
    const auto value = layer.find(key);
    return value && usable(key, *value) ? value : nullptr;
}
}
std::expected<ConfigValue, std::string> resolve_config_appearance(
    const ConfigValue& config, const ConfigValue& directive_sum,
    const std::vector<ConfigValue>& saved_directives, const ConfigValue& site_delta,
    std::optional<std::u16string_view> diagram_type, ConfigLimits limits) {
    try {
        Budget budget{limits}; budget.inspect(config); budget.inspect(directive_sum); budget.inspect(site_delta);
        for (const auto& layer : saved_directives) budget.inspect(layer);
        if (diagram_type) budget.text(diagram_type->size());
        auto result = config;
        if (!diagram_type || diagram_type->empty()) return result;
        if (result.kind != ConfigKind::mapping) return std::unexpected("Appearance destination must be a mapping");
        const auto section = diagram_config_key(*diagram_type);
        for (const auto key : {u"theme", u"look", u"layout"}) {
            const ConfigValue* selected = read(directive_sum, section, key, budget);
            for (auto it = saved_directives.rbegin(); !selected && it != saved_directives.rend(); ++it) selected = read(*it, section, key, budget);
            if (!selected) selected = read(site_delta, section, key, budget);
            ConfigValue fallback;
            if (!selected) {
                auto value = default_appearance(section, key);
                if (!value) value = default_appearance(u"", key);
                if (value) { fallback = config_detail::string(*value); selected = &fallback; }
            }
            if (!selected) continue;
            result.mapping.insert_or_assign(key, *selected);
            auto scoped = result.mapping.find(section);
            if (scoped != result.mapping.end() && scoped->second.kind == ConfigKind::mapping) {
                auto field = scoped->second.mapping.find(key);
                if (field != scoped->second.mapping.end()) field->second = *selected;
            }
        }
        budget.output(result); return result;
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
