module folia.mermaid.theme;
import std;
import folia.mermaid.config_support;
import folia.mermaid.theme_color;

namespace folia::mermaid {
namespace {
// Missing is distinct from explicit null while evaluating ||, ?? and copies.
// Only the public result drops missing properties, as JSON.stringify does.
struct Value {
    std::optional<ConfigValue> data;
    Value() = default;
    Value(const ConfigValue& v):data(v) {}
    Value(ConfigValue&& v):data(std::move(v)) {}
    Value(std::u16string_view text):data(config_detail::string(text)) {}
    Value(const char16_t* text):Value(std::u16string_view{text}) {}
    Value(std::u16string text):Value(std::u16string_view{text}) {}
    Value(double n):data(ConfigValue{}) { data->kind=ConfigKind::number; data->number=n; }
    Value(int n):Value(static_cast<double>(n)) {}
    Value(bool b):data(ConfigValue{}) { data->kind=ConfigKind::boolean; data->boolean=b; }
};
bool truthy(const Value& value) { return value.data && config_truthy(*value.data); }
template<class F> Value fallback(Value value,F next) { return truthy(value)?std::move(value):next(); }
template<class F> Value nullish(Value value,F next) { return value.data && value.data->kind!=ConfigKind::null?std::move(value):next(); }
template<class F> Value both(Value value,F next) { return truthy(value)?next():std::move(value); }
const std::u16string& text(const Value& value) {
    if(!value.data || value.data->kind!=ConfigKind::string) throw std::runtime_error("Theme color/key must be a string");
    return value.data->string;
}
double number(const Value& value) {
    if(!value.data) return std::numeric_limits<double>::quiet_NaN();
    if(value.data->kind!=ConfigKind::number) throw std::runtime_error("Theme numeric operand must be a number");
    return value.data->number;
}
Value numeric(const Value& v) { return number(v); }
Value negate(const Value& v) { return -number(v); }
Value add(const Value& a,const Value& b) {
    if(a.data && a.data->kind==ConfigKind::string) {
        const auto suffix=config_to_string(*b.data);
        if(!suffix) throw std::runtime_error(suffix.error());
        return a.data->string+*suffix;
    }
    return number(a)+number(b);
}
Value subtract(const Value& a,const Value& b) { return number(a)-number(b); }
Value multiply(const Value& a,const Value& b) { return number(a)*number(b); }
Value different(const Value& a,const Value& b) {
    return !a.data || !b.data || a.data->kind!=b.data->kind ||
        a.data->kind!=ConfigKind::string || a.data->string!=b.data->string;
}
Value property(const Value& object,const Value& key) {
    if(!object.data) return {};
    const auto value=object.data->find(text(key)); return value?Value(*value):Value{};
}
Value object(std::initializer_list<std::pair<std::u16string,Value>> items) {
    auto result=config_detail::map();
    for(const auto& [key,value]:items) if(value.data) result.mapping.emplace(key,*value.data);
    return result;
}
Value adjust(const Value& color,const Value& changes) {
    std::map<char,double> channels;
    for(const auto& [key,value]:changes.data->mapping) channels.emplace(static_cast<char>(key.front()),value.number);
    return theme_detail::color_adjust(text(color),channels);
}
Value lighten(const Value& color,const Value& amount={}) { return theme_detail::color_lighten(text(color),number(amount)); }
Value darken(const Value& color,const Value& amount={}) { return theme_detail::color_lighten(text(color),-number(amount)); }
Value invert(const Value& color) { return theme_detail::color_invert(text(color)); }
Value isDark(const Value& color) { return theme_detail::color_is_dark(text(color)); }
Value rgba(const Value& r,const Value& g,const Value& b,const Value& a) {
    return theme_detail::color_rgba(number(r),number(g),number(b),number(a));
}
Value mkBorder(const Value& color,const Value& darkMode) {
    return theme_detail::color_adjust(text(color),{{'s',-40},{'l',truthy(darkMode)?10:-10}});
}
struct Theme {
    MermaidTheme name;
    ConfigValue values=config_detail::map();
    config_detail::Budget budget;
    // Cumulative intermediate assignments are bounded as well as final output.
    config_detail::Budget assigned;
    Value get(const Value& key) {
        budget.step(); const auto v=values.find(text(key)); return v?Value(*v):Value{};
    }
    void set(const Value& key,const Value& value) {
        budget.step();
        if(value.data) { assigned.text(text(key).size()); assigned.inspect(*value.data); values.mapping.insert_or_assign(text(key),*value.data); }
        else values.mapping.erase(text(key));
    }
    int color_limit() {
        const auto count=number(get(u"THEME_COLOR_LIMIT"));
        if(!std::isfinite(count) || count<0 || count>12 || std::trunc(count)!=count)
            throw std::runtime_error("Theme color limit must be an integer from 0 through 12");
        return static_cast<int>(count);
    }
    void construct_default();
    void update_default();
    void construct_dark();
    void update_dark();
    void update() { if(name==MermaidTheme::default_theme) update_default(); else update_dark(); }
};
#include "theme_default.inc"
#include "theme_dark.inc"
}
std::expected<ConfigValue,std::string> derive_theme_variables(MermaidTheme name,const ConfigValue* overrides,ConfigLimits limits) {
    try {
        if(name!=MermaidTheme::default_theme && name!=MermaidTheme::dark) throw std::runtime_error("Unsupported native theme");
        Theme theme{name,config_detail::map(),{limits},{limits}};
        if(overrides) {
            theme.budget.inspect(*overrides);
            if(overrides->kind!=ConfigKind::mapping) throw std::runtime_error("Theme overrides must be a mapping");
            for(const auto& [key,value]:overrides->mapping) {
                if(key==u"__proto__" || key==u"constructor" || key==u"prototype" || key==u"updateColors" || key==u"calculate")
                    throw std::runtime_error("Prototype or theme-method overrides are not supported");
                if(key==u"THEME_COLOR_LIMIT" && (value.kind!=ConfigKind::number || !std::isfinite(value.number) || value.number<0 || value.number>12 || std::trunc(value.number)!=value.number))
                    throw std::runtime_error("Theme color limit must be an integer from 0 through 12");
            }
        }
        if(name==MermaidTheme::default_theme) {
            theme.construct_default();
            // Official default calculate clears only exact 'calculated' values.
            std::erase_if(theme.values.mapping,[](const auto& entry) {
                return entry.second.kind==ConfigKind::string && entry.second.string==u"calculated";
            });
        } else theme.construct_dark();
        if(overrides) for(const auto& [key,value]:overrides->mapping) theme.set(key,value);
        theme.update();
        if(overrides) for(const auto& [key,value]:overrides->mapping) {
            auto generated=theme.get(key);
            // applyOverride is a shallow merge, not recursive assignWithDepth.
            if(generated.data && generated.data->kind==ConfigKind::mapping && value.kind==ConfigKind::mapping) {
                for(const auto& [child,item]:value.mapping) generated.data->mapping.insert_or_assign(child,item);
                theme.set(key,generated);
            } else theme.set(key,value);
        }
        theme.budget.output(theme.values);
        return std::move(theme.values);
    } catch(const std::exception& error) { return std::unexpected(error.what()); }
}
}
