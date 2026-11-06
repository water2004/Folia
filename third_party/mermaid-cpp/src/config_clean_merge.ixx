// Mermaid 12.2.0 utils.ts + es-toolkit 1.45.1 compat merge. MIT.
export module folia.mermaid.config_clean_merge;
import std;
export import folia.mermaid.config_limits;

export namespace folia::mermaid {
// merge({}, defaults, data): owned recursive merge, arrays merge by index (not
// union), primitive root sources are ignored, and __proto__ writes are skipped.
// The pinned library treats null array members as undefined. Preserve that
// internally, then reject final undefined values/holes/custom array properties
// instead of silently converting them to null or dropping them. Inputs remain
// unchanged. ConfigLimits also bounds cumulative intermediate nodes/text.
std::expected<ConfigValue, std::string> clean_and_merge_config(
    const ConfigValue& defaults, const ConfigValue* data = nullptr, ConfigLimits limits = {});
}
