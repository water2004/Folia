module folia.mermaid.config_sanitize;
import std;
import folia.mermaid.config_support;
import folia.mermaid.config_data;

namespace folia::mermaid {
namespace {
using namespace config_detail;
bool ascii_alpha(char16_t c) { return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z'); }
bool digit(char16_t c) { return c >= u'0' && c <= u'9'; }
bool space(char16_t c) {
    return (c >= 9 && c <= 13) || c == 0x20 || c == 0xa0 || c == 0x1680 ||
        (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 ||
        c == 0x202f || c == 0x205f || c == 0x3000 || c == 0xfeff;
}
bool icon(std::u16string_view s) {
    bool has_part = false, colon = false;
    for (auto c : s) {
        if (c == u':') { if (!has_part || colon) return false; colon = true; has_part = false; }
        else if (ascii_alpha(c) || digit(c) || c == u'_' || c == u'-') has_part = true;
        else return false;
    }
    return has_part;
}
bool color(std::u16string_view s) {
    if (s.empty()) return false;
    if (s.front() == u'#') return s.size() >= 4 && s.size() <= 9 &&
        std::ranges::all_of(s.substr(1), [](char16_t c) { return digit(c) || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F'); });
    if (std::ranges::all_of(s, ascii_alpha)) return true;
    if (s.size() < 6 || s[3] != u'(' || s.back() != u')') return false;
    const auto lower = [](char16_t c) { return c >= u'A' && c <= u'Z' ? char16_t(c + 32) : c; };
    const std::u16string prefix{lower(s[0]), lower(s[1]), lower(s[2])};
    if (prefix != u"rgb" && prefix != u"hsl") return false;
    return std::ranges::all_of(s.substr(4, s.size() - 5), [](char16_t c) { return digit(c) || space(c) || c == u'%' || c == u',' || c == u'.'; });
}
bool theme_value(std::u16string_view s) {
    return !s.empty() && std::ranges::all_of(s, [](char16_t c) {
        return ascii_alpha(c) || digit(c) || std::u16string_view{u" \"#%(),.;"}.find(c) != std::u16string_view::npos;
    });
}
void css(ConfigValue& value) {
    if (value.kind != ConfigKind::string) throw std::runtime_error("CSS configuration is not iterable text");
    std::size_t opens = 0, closes = 0;
    for (auto c : value.string) {
        if (opens < closes) break;
        if (c == u'{') ++opens;
        if (c == u'}') ++closes;
    }
    if (opens != closes) value.string = u"{ /* ERROR: Unbalanced CSS */ }";
}
void dictionary(ConfigValue& value, bool colors, Budget& budget) {
    auto valid = [&](const ConfigValue& item) {
        return item.kind == ConfigKind::string && (colors ? color(item.string) : icon(item.string));
    };
    if (value.kind == ConfigKind::mapping) {
        std::erase_if(value.mapping, [&](const auto& entry) {
            budget.step(); return suspicious(entry.first) || !valid(entry.second);
        });
    } else if (value.kind == ConfigKind::sequence) {
        for (const auto& item : value.sequence) {
            budget.step();
            if (!valid(item)) throw std::runtime_error("Dictionary sanitization would create a sparse array");
        }
    }
}
void sanitize(ConfigValue& value, Budget& budget) {
    budget.step();
    if (value.kind == ConfigKind::sequence) {
        for (auto& item : value.sequence) sanitize(item, budget);
        return;
    }
    if (value.kind != ConfigKind::mapping) return;
    for (auto it = value.mapping.begin(); it != value.mapping.end();) {
        budget.step();
        auto& [key, item] = *it;
        if (suspicious(key) || !known_key(key) || item.kind == ConfigKind::null) { it = value.mapping.erase(it); continue; }
        if (object(item)) {
            if (key == u"nodeColors" || key == u"filenameIcons" || key == u"extensionIcons") dictionary(item, key == u"nodeColors", budget);
            else sanitize(item, budget);
        } else {
            for (const auto css_key : {u"themeCSS", u"fontFamily", u"altFontFamily"}) if (key.find(css_key) != key.npos) css(item);
        }
        ++it;
    }
    auto found = value.mapping.find(u"themeVariables");
    if (found == value.mapping.end()) return;
    auto check = [&](ConfigValue& item) {
        budget.step();
        if (item.kind == ConfigKind::string && !theme_value(item.string)) item.string.clear();
    };
    auto& vars = found->second;
    if (vars.kind == ConfigKind::mapping) for (auto& [key, item] : vars.mapping) check(item);
    if (vars.kind == ConfigKind::sequence) for (auto& item : vars.sequence) check(item);
    if (vars.kind == ConfigKind::string) for (const auto c : vars.string) {
        budget.step();
        // The original attempts assignment to the primitive string on failure.
        if (!theme_value(std::u16string_view{&c, 1})) throw std::runtime_error("Cannot sanitize a primitive themeVariables string");
    }
}
void secure_walk(ConfigValue& value, const std::vector<std::u16string>& secure, Budget& budget) {
    auto denied = [&](std::u16string_view key, const ConfigValue& item) {
        budget.step();
        if (key == u"secure" || key.starts_with(u"__")) return true;
        for (const auto& name : secure) { budget.step(); if (name == key) return true; }
        return item.kind == ConfigKind::string && (item.string.find(u'<') != item.string.npos ||
            item.string.find(u'>') != item.string.npos || item.string.find(u"url(data:") != item.string.npos);
    };
    if (value.kind == ConfigKind::mapping) {
        for (auto it = value.mapping.begin(); it != value.mapping.end();) {
            if (denied(it->first, it->second)) it = value.mapping.erase(it);
            else { if (object(it->second)) secure_walk(it->second, secure, budget); ++it; }
        }
    } else if (value.kind == ConfigKind::sequence) {
        for (std::size_t i = 0; i < value.sequence.size(); ++i) {
            const auto ascii = std::to_string(i);
            if (denied(std::u16string(ascii.begin(), ascii.end()), value.sequence[i])) throw std::runtime_error("Secure sanitization would create a sparse array");
            if (object(value.sequence[i])) secure_walk(value.sequence[i], secure, budget);
        }
    } else if (value.kind == ConfigKind::string) {
        for (std::size_t i = 0; i < value.string.size(); ++i) {
            const auto ascii = std::to_string(i);
            const auto character = config_detail::string(std::u16string_view{value.string}.substr(i, 1));
            if (denied(std::u16string(ascii.begin(), ascii.end()), character))
                throw std::runtime_error("Cannot delete an indexed property of a primitive string");
        }
    }
}
}
std::expected<ConfigValue, std::string> sanitize_directive_config(const ConfigValue& value, ConfigLimits limits) {
    try { Budget budget{limits}; budget.inspect(value); auto result = value; sanitize(result, budget); budget.output(result); return result; }
    catch (const std::exception& error) { return std::unexpected(error.what()); }
}
std::expected<ConfigValue, std::string> sanitize_secure_config(const ConfigValue& value, const std::vector<std::u16string>& secure, ConfigLimits limits) {
    try {
        Budget budget{limits}; budget.inspect(value);
        for (const auto& key : secure) { budget.step(); budget.text(key.size()); }
        auto result = value; secure_walk(result, secure, budget); budget.output(result); return result;
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
