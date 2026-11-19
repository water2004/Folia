module folia.mermaid.config_value;
import std;
import folia.mermaid.number;

namespace folia::mermaid {
bool config_truthy(const ConfigValue& value) {
    switch (value.kind) {
    case ConfigKind::null: return false;
    case ConfigKind::boolean: return value.boolean;
    case ConfigKind::number: return value.number != 0 && !std::isnan(value.number);
    case ConfigKind::string: return !value.string.empty();
    default: return true;
    }
}
std::expected<std::u16string, std::string> config_to_string(const ConfigValue& value) {
    switch (value.kind) {
    case ConfigKind::null: return u"null";
    case ConfigKind::boolean: return value.boolean ? u"true" : u"false";
    case ConfigKind::string: return value.string;
    case ConfigKind::number: {
        if (std::isnan(value.number)) return u"NaN";
        if (std::isinf(value.number)) return value.number < 0 ? u"-Infinity" : u"Infinity";
        const auto text = *format_number(value.number);
        return std::u16string(text.begin(), text.end());
    }
    case ConfigKind::mapping:
        if (value.find(u"toString")) return std::unexpected("Non-callable metadata toString");
        return u"[object Object]";
    case ConfigKind::sequence: {
        std::u16string result;
        for (std::size_t i = 0; i < value.sequence.size(); ++i) {
            if (i) result += u',';
            if (value.sequence[i].kind == ConfigKind::null) continue;
            const auto item = config_to_string(value.sequence[i]);
            if (!item) return std::unexpected(item.error());
            result += *item;
        }
        return result;
    }
    }
    return std::unexpected("Invalid config value kind");
}
}
