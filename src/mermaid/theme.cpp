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
Value modulo(const Value& a,const Value& b) { return std::fmod(number(a),number(b)); }
Value equal(const Value& a,const Value& b) {
    return a.data && b.data && a.data->kind==ConfigKind::string &&
        b.data->kind==ConfigKind::string && a.data->string==b.data->string;
}
Value different(const Value& a,const Value& b) { return !truthy(equal(a,b)); }
Value property(const Value& object,const Value& key) {
    if(!object.data) return {};
    const auto value=object.data->find(text(key)); return value?Value(*value):Value{};
}
Value length(const Value& value) {
    if(!value.data || value.data->kind==ConfigKind::null)
        throw std::runtime_error("Cannot read theme length from null or undefined");
    if(value.data->kind==ConfigKind::sequence) return static_cast<double>(value.data->sequence.size());
    if(value.data->kind==ConfigKind::string) return static_cast<double>(value.data->string.size());
    return property(value,u"length");
}
Value indexed(const Value& value,const Value& key) {
    if(!value.data || value.data->kind==ConfigKind::null)
        throw std::runtime_error("Cannot index a null or undefined theme value");
    const auto index=number(key);
    if(value.data->kind==ConfigKind::sequence || value.data->kind==ConfigKind::string) {
        if(!std::isfinite(index) || index<0 || std::trunc(index)!=index ||
           index>=number(length(value))) return {};
        const auto position=static_cast<std::size_t>(index);
        if(value.data->kind==ConfigKind::sequence) return value.data->sequence[position];
        return value.data->string.substr(position,1);
    }
    if(value.data->kind==ConfigKind::mapping) {
        if(std::isnan(index)) return property(value,u"NaN");
        if(const auto string=config_to_string(*key.data)) return property(value,*string);
    }
    return {};
}
Value sequence(std::initializer_list<Value> items) {
    ConfigValue result;
    result.kind=ConfigKind::sequence;
    for(const auto& item:items) result.sequence.push_back(item.data.value_or(ConfigValue{}));
    return result;
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
    using Method=void (Theme::*)();
    Method update_colors;
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
    void construct_base();
    void update_base();
    void construct_forest();
    void update_forest();
    void construct_neutral();
    void update_neutral();
    void construct_neo();
    void update_neo();
    void construct_neo_dark();
    void update_neo_dark();
    void construct_redux();
    void update_redux();
    void construct_redux_dark();
    void update_redux_dark();
    void construct_redux_color();
    void update_redux_color();
    void construct_redux_dark_color();
    void update_redux_dark_color();
    void update() { (this->*update_colors)(); }
};
#include "theme_default.inc"
#include "theme_dark.inc"
#include "theme_base.inc"
#include "theme_forest.inc"
#include "theme_neutral.inc"
#include "theme_neo.inc"
#include "theme_neo_dark.inc"
#include "theme_redux.inc"
#include "theme_redux_dark.inc"
#include "theme_redux_color.inc"
#include "theme_redux_dark_color.inc"

struct NamedTheme {
    std::u16string_view name;
    MermaidTheme id;
    Theme::Method construct;
    Theme::Method update;
};
constexpr NamedTheme named_themes[]={
    {u"default",MermaidTheme::default_theme,&Theme::construct_default,&Theme::update_default},
    {u"dark",MermaidTheme::dark,&Theme::construct_dark,&Theme::update_dark},
    {u"base",MermaidTheme::base,&Theme::construct_base,&Theme::update_base},
    {u"forest",MermaidTheme::forest,&Theme::construct_forest,&Theme::update_forest},
    {u"neutral",MermaidTheme::neutral,&Theme::construct_neutral,&Theme::update_neutral},
    {u"neo",MermaidTheme::neo,&Theme::construct_neo,&Theme::update_neo},
    {u"neo-dark",MermaidTheme::neo_dark,&Theme::construct_neo_dark,&Theme::update_neo_dark},
    {u"redux",MermaidTheme::redux,&Theme::construct_redux,&Theme::update_redux},
    {u"redux-dark",MermaidTheme::redux_dark,&Theme::construct_redux_dark,&Theme::update_redux_dark},
    {u"redux-color",MermaidTheme::redux_color,&Theme::construct_redux_color,&Theme::update_redux_color},
    {u"redux-dark-color",MermaidTheme::redux_dark_color,&Theme::construct_redux_dark_color,&Theme::update_redux_dark_color},
};
}
std::optional<MermaidTheme> find_mermaid_theme(std::u16string_view name) {
    for(const auto& entry:named_themes) if(entry.name==name) return entry.id;
    return {};
}
std::expected<ConfigValue,std::string> derive_theme_variables(MermaidTheme name,const ConfigValue* overrides,ConfigLimits limits) {
    try {
        const auto entry=std::ranges::find(named_themes,name,&NamedTheme::id);
        if(entry==std::end(named_themes)) throw std::runtime_error("Unsupported native theme");
        Theme theme{entry->update,config_detail::map(),{limits},{limits}};
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
        (theme.*entry->construct)();
        if(name==MermaidTheme::default_theme) {
            // Official default calculate clears only exact 'calculated' values.
            std::erase_if(theme.values.mapping,[](const auto& entry) {
                return entry.second.kind==ConfigKind::string && entry.second.string==u"calculated";
            });
        }
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
        // The base theme's neo stroke gradient otherwise covers an explicit border.
        if(name==MermaidTheme::base && overrides && overrides->find(u"nodeBorder") &&
           !overrides->find(u"useGradient")) theme.set(u"useGradient",false);
        theme.budget.output(theme.values);
        return std::move(theme.values);
    } catch(const std::exception& error) { return std::unexpected(error.what()); }
}
}
