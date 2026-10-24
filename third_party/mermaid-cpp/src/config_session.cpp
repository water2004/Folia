module folia.mermaid.config_session;
import std;
import folia.mermaid.config_support;
import folia.mermaid.config_merge;
import folia.mermaid.config_sanitize;
import folia.mermaid.config_appearance;
import folia.mermaid.config_data;
import folia.mermaid.theme;

namespace folia::mermaid {
ConfigValue default_session_config() {
#include "config_defaults.inc"
}
namespace {
using namespace config_detail;
#include "config_callbacks.inc"
ConfigValue required(std::expected<ConfigValue, std::string> value) {
    if (!value) throw std::runtime_error(value.error());
    return std::move(*value);
}
// assignWithDepth shares deeper containers and array-object identities in JS.
// This API accepts data for which those identities cannot affect later replay.
void owned_shape(const ConfigValue& value, std::size_t depth = 0, std::u16string_view section = {}) {
    if (value.kind == ConfigKind::mapping || value.kind == ConfigKind::sequence) {
        if (depth > 2) throw std::runtime_error("Session input contains a shared-depth container");
    }
    if (value.kind == ConfigKind::sequence) for (const auto& item : value.sequence) {
        if (item.kind == ConfigKind::mapping || item.kind == ConfigKind::sequence)
            throw std::runtime_error("Session arrays must contain primitive values");
    }
    if (value.kind == ConfigKind::mapping) for (const auto& [key, item] : value.mapping) {
        if (depth == 1 && (item.kind == ConfigKind::mapping || item.kind == ConfigKind::sequence))
            for (const auto& callback : default_callback_fields)
                if (section == callback.section && key == callback.key)
                    throw std::runtime_error("Session cannot replace an unported default callback with a container");
        if (key == u"__proto__") throw std::runtime_error("Session prototype setters are unsupported");
        if (key == u"theme" && (item.kind == ConfigKind::mapping || item.kind == ConfigKind::sequence))
            throw std::runtime_error("Session theme names must be primitive values");
        owned_shape(item, depth + 1, depth == 0 ? std::u16string_view{key} : section);
    }
}
std::vector<std::u16string> secure_keys(const ConfigValue& site) {
    std::vector<std::u16string> result;
    const auto keys = site.find(u"secure");
    if (!keys || keys->kind == ConfigKind::null) return result;
    if (keys->kind != ConfigKind::sequence) throw std::runtime_error("Session secure keys must be a string list");
    for (const auto& key : keys->sequence) {
        if (key.kind != ConfigKind::string) throw std::runtime_error("Session secure keys must be strings");
        result.push_back(key.string);
    }
    return result;
}
std::optional<MermaidTheme> native_theme(const ConfigValue* value) {
    if (!value || !config_truthy(*value)) return {};
    auto name = config_to_string(*value);
    if (!name) throw std::runtime_error(name.error());
    if (!known_theme(*name)) return {};
    if (*name == u"default") return MermaidTheme::default_theme;
    if (*name == u"dark") return MermaidTheme::dark;
    throw std::runtime_error("Session theme derivation is not yet ported for this name");
}
bool equal_theme(const ConfigValue* a, const ConfigValue* b) {
    if (!a || !b) return a == b;
    if (a->kind != b->kind) return false;
    switch (a->kind) {
    case ConfigKind::null: return true;
    case ConfigKind::boolean: return a->boolean == b->boolean;
    case ConfigKind::number: return a->number == b->number;
    case ConfigKind::string: return a->string == b->string;
    default: return false;
    }
}
}
void ConfigSession::inspect_state() const {
    Budget budget{limits_.config};
    budget.inspect(site_); budget.inspect(initialize_); budget.inspect(delta_); budget.inspect(current_);
    if (directives_.size() > std::min(limits_.directives, std::size_t{256}))
        throw std::runtime_error("Session directive budget exceeded");
    for (const auto& value : directives_) budget.inspect(value);
    if (scope_) budget.text(scope_->size());
}
std::expected<ConfigSession, std::string> ConfigSession::create(ConfigSessionLimits limits) {
    try {
        ConfigSession session{limits};
        auto defaults = default_session_config();
        session.site_ = required(assign_config_with_depth(map(), defaults, 2, limits.config));
        session.current_ = session.site_;
        session.initialize_ = map(); session.delta_ = map();
        session.inspect_state();
        return session;
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
std::expected<ConfigValue, std::string> ConfigSession::user_config() const {
    try {
        auto result = required(assign_config_with_depth(map(), initialize_, 2, limits_.config));
        for (const auto& directive : directives_)
            result = required(assign_config_with_depth(result, directive, 2, limits_.config));
        return result;
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
std::expected<ConfigValue, std::string> ConfigSession::site_config() const {
    return assign_config_with_depth(map(), site_, 2, limits_.config);
}
std::expected<ConfigValue, std::string> ConfigSession::current_config() const {
    return assign_config_with_depth(map(), current_, 2, limits_.config);
}
std::expected<void, std::string> ConfigSession::apply(Operation operation, const ConfigValue* input,
    std::optional<std::u16string_view> scope) {
    try {
        // Inspect before copying public trees, including inactive payloads.
        Budget budget{limits_.config};
        if (input) {
            budget.inspect(*input);
            if (input->kind != ConfigKind::mapping) throw std::runtime_error("Session command input must be a mapping");
            owned_shape(*input);
            // Validate even if assignWithDepth would retain an existing array.
            (void)secure_keys(*input);
        }
        if (scope) budget.text(scope->size());
        if (operation == Operation::add && directives_.size() >= std::min(limits_.directives, std::size_t{256}))
            throw std::runtime_error("Session directive budget exceeded");
        inspect_state();
        auto next = *this;
        const auto merge = [&](const ConfigValue& dst, const ConfigValue& src) {
            auto value = required(assign_config_with_depth(dst, src, 2, limits_.config));
            // Bound cumulative command intermediate allocation/inspection as
            // well as each primitive's independent comparison/work budget.
            budget.inspect(value);
            return value;
        };
        auto copy = [&](const ConfigValue& value) { return merge(map(), value); };
        auto derive = [&](ConfigValue& target, MermaidTheme theme, const ConfigValue* overrides) {
            auto variables = required(derive_theme_variables(theme, overrides, limits_.config));
            budget.inspect(variables);
            target.mapping.insert_or_assign(u"themeVariables", std::move(variables));
        };
        auto recompute = [&](const ConfigValue& base, std::vector<ConfigValue>& active) {
            auto cfg = copy(base);
            auto sum = map();
            const auto secure = secure_keys(next.site_);
            for (auto& directive : active) {
                directive = required(sanitize_secure_config(directive, secure, limits_.config));
                budget.inspect(directive);
                sum = merge(sum, directive);
            }
            cfg = merge(cfg, sum);
            cfg = required(resolve_config_appearance(cfg, sum, next.directives_, next.delta_,
                next.scope_ ? std::optional<std::u16string_view>{*next.scope_} : std::nullopt, limits_.config));
            budget.inspect(cfg);
            const auto selected = cfg.find(u"theme");
            const auto directive_theme = sum.find(u"theme");
            const bool changed = (directive_theme && config_truthy(*directive_theme)) || !equal_theme(selected, base.find(u"theme"));
            if (changed) if (const auto theme = native_theme(selected)) {
                auto initial = copy(next.initialize_);
                const auto vars = initial.find(u"themeVariables");
                auto overrides = vars && config_truthy(*vars) ? *vars : map();
                if (const auto extra = sum.find(u"themeVariables")) overrides = merge(overrides, *extra);
                derive(cfg, *theme, &overrides);
            }
            next.current_ = std::move(cfg);
            for (const auto key : {u"lazyLoadedDiagrams", u"loadExternalDiagramsAtStartup"})
                if (const auto value = next.current_.find(key); value && config_truthy(*value)) next.warned_ = true;
        };
        switch (operation) {
        case Operation::save:
            next.initialize_ = copy(*input);
            break;
        case Operation::site:
            next.site_ = merge(copy(default_session_config()), *input);
            next.delta_ = copy(*input);
            if (const auto theme = native_theme(input->find(u"theme")))
                derive(next.site_, *theme, input->find(u"themeVariables"));
            recompute(next.site_, next.directives_);
            break;
        case Operation::update:
            next.site_ = merge(next.site_, *input);
            next.delta_ = merge(next.delta_, *input);
            recompute(next.site_, next.directives_);
            break;
        case Operation::set: {
            std::vector<ConfigValue> temporary{*input};
            recompute(next.current_, temporary);
            break;
        }
        case Operation::add: {
            auto directive = required(sanitize_directive_config(*input, limits_.config));
            const auto font = directive.find(u"fontFamily");
            const auto vars = directive.find(u"themeVariables");
            const auto theme_font = vars ? vars->find(u"fontFamily") : nullptr;
            if (font && config_truthy(*font) && (!theme_font || !config_truthy(*theme_font))) {
                if (vars && vars->kind != ConfigKind::mapping)
                    throw std::runtime_error("Directive font propagation needs mapping themeVariables");
                auto variables = vars ? *vars : map();
                variables.mapping.insert_or_assign(u"fontFamily", *font);
                directive.mapping.insert_or_assign(u"themeVariables", std::move(variables));
            }
            budget.inspect(directive);
            next.directives_.push_back(std::move(directive));
            recompute(next.site_, next.directives_);
            break;
        }
        case Operation::reset:
            next.directives_.clear(); next.scope_.reset();
            recompute(input ? *input : next.site_, next.directives_);
            break;
        case Operation::scope:
            if (scope) next.scope_ = std::u16string{*scope}; else next.scope_.reset();
            recompute(next.site_, next.directives_);
            break;
        }
        next.inspect_state();
        *this = std::move(next);
        return {};
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
std::expected<void, std::string> ConfigSession::set_site_config(const ConfigValue& config) { return apply(Operation::site, &config); }
std::expected<void, std::string> ConfigSession::update_site_config(const ConfigValue& config) { return apply(Operation::update, &config); }
std::expected<void, std::string> ConfigSession::save_initialize_config(const ConfigValue& config) { return apply(Operation::save, &config); }
std::expected<void, std::string> ConfigSession::set_config(const ConfigValue& config) { return apply(Operation::set, &config); }
std::expected<void, std::string> ConfigSession::add_directive(const ConfigValue& config) { return apply(Operation::add, &config); }
std::expected<void, std::string> ConfigSession::reset(const ConfigValue* config) { return apply(Operation::reset, config); }
std::expected<void, std::string> ConfigSession::set_diagram_scope(std::optional<std::u16string_view> scope) { return apply(Operation::scope, nullptr, scope); }
}
