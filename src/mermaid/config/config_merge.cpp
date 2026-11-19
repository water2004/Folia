module folia.mermaid.config_merge;
import std;
import folia.mermaid.config_support;

namespace folia::mermaid {
namespace {
using namespace config_detail;
bool same_primitive(const ConfigValue& a, const ConfigValue& b) {
    if (a.kind != b.kind) return false;
    switch (a.kind) {
    case ConfigKind::null: return true;
    case ConfigKind::boolean: return a.boolean == b.boolean;
    case ConfigKind::number: return a.number == b.number || (std::isnan(a.number) && std::isnan(b.number));
    case ConfigKind::string: return a.string == b.string;
    default: return false; // Owned objects have separate identities, even if structurally equal.
    }
}
void assign(ConfigValue& dst, const ConfigValue& src, int depth, Budget& budget, bool accept_replacement = true) {
    budget.step();
    if (src.kind == ConfigKind::sequence) {
        if (dst.kind == ConfigKind::sequence) {
            for (const auto& item : src.sequence) {
                budget.step();
                bool found = false;
                for (const auto& existing : dst.sequence) {
                    budget.step();
                    if (same_primitive(existing, item)) { found = true; break; }
                }
                if (!found) dst.sequence.push_back(item);
            }
        } else {
            // Upstream forEach discards each call's return, retaining only mutations.
            for (const auto& item : src.sequence) assign(dst, item, depth, budget, false);
        }
        return;
    }
    if (dst.kind == ConfigKind::sequence && src.kind == ConfigKind::mapping)
        throw std::runtime_error("Object properties on configuration arrays are unsupported");
    if (dst.kind == ConfigKind::null || depth <= 0) {
        if (dst.kind != ConfigKind::null && object(dst) && object(src)) {
            if (dst.kind == ConfigKind::mapping && src.kind == ConfigKind::mapping)
                for (const auto& [key, item] : src.mapping) {
                    budget.step();
                    if (key == u"__proto__" && !dst.mapping.contains(key))
                        throw std::runtime_error("Configuration merge would invoke a JavaScript prototype setter");
                    dst.mapping.insert_or_assign(key, item);
                }
        } else if (accept_replacement) dst = src;
        return;
    }
    if (dst.kind != ConfigKind::mapping || src.kind != ConfigKind::mapping) return;
    for (const auto& [key, item] : src.mapping) {
        budget.step();
        auto found = dst.mapping.find(key);
        if (object(item)) {
            if (item.kind == ConfigKind::null) continue;
            if (found == dst.mapping.end()) {
                ConfigValue empty; empty.kind = item.kind;
                found = dst.mapping.emplace(key, std::move(empty)).first;
            }
            if (object(found->second)) assign(found->second, item, depth - 1, budget);
        } else if (found == dst.mapping.end() || !object(found->second)) dst.mapping.insert_or_assign(key, item);
    }
}
}
std::expected<ConfigValue, std::string> assign_config_with_depth(
    const ConfigValue& destination, const ConfigValue& source, int merge_depth, ConfigLimits limits) {
    try {
        Budget budget{limits}; budget.inspect(destination); budget.inspect(source);
        auto result = destination; assign(result, source, merge_depth, budget); budget.output(result); return result;
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
