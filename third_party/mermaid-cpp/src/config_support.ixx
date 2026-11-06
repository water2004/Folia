// Internal helpers shared by the bounded configuration operations.
export module folia.mermaid.config_support;
import std;
import folia.mermaid.config_limits;

export namespace folia::mermaid::config_detail {
struct Budget {
    ConfigLimits limits;
    std::size_t values = 0, units = 0, operations = 0;
    void step() {
        if (operations >= limits.operations) throw std::runtime_error("Configuration operation budget exceeded");
        ++operations;
    }
    void text(std::size_t count) {
        if (count > limits.string_units - units) throw std::runtime_error("Configuration string budget exceeded");
        units += count;
    }
    void inspect(const ConfigValue& value, std::size_t depth = 0) {
        step();
        if (depth > std::min(limits.depth, std::size_t{64})) throw std::runtime_error("Configuration depth budget exceeded");
        if (values >= limits.values) throw std::runtime_error("Configuration value budget exceeded");
        ++values;
        // ConfigValue is publicly constructible. Reject hidden inactive payloads
        // before copying, so they cannot bypass the active-tree size/depth walk.
        if ((value.kind != ConfigKind::string && !value.string.empty()) ||
            (value.kind != ConfigKind::sequence && !value.sequence.empty()) ||
            (value.kind != ConfigKind::mapping && !value.mapping.empty()))
            throw std::runtime_error("Configuration value has an inactive payload");
        if (value.kind == ConfigKind::string) text(value.string.size());
        if (value.kind == ConfigKind::sequence) for (const auto& item : value.sequence) inspect(item, depth + 1);
        if (value.kind == ConfigKind::mapping) for (const auto& [key, item] : value.mapping) {
            text(key.size()); inspect(item, depth + 1);
        }
    }
    void output(const ConfigValue& value) {
        // Input and output each have the same size bounds; operation work is cumulative.
        values = units = 0;
        inspect(value);
    }
};
inline bool object(const ConfigValue& value) {
    return value.kind == ConfigKind::null || value.kind == ConfigKind::mapping || value.kind == ConfigKind::sequence;
}
inline ConfigValue map() { ConfigValue value; value.kind = ConfigKind::mapping; return value; }
inline ConfigValue string(std::u16string_view text) {
    ConfigValue value; value.kind = ConfigKind::string; value.string = text; return value;
}
inline bool suspicious(std::u16string_view key) {
    return key.starts_with(u"__") || key.find(u"proto") != key.npos || key.find(u"constr") != key.npos;
}
}
