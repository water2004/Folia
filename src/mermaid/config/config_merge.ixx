// Mermaid 12.2.0 assignWithDepth.ts. MIT, see ../LICENSE.
export module folia.mermaid.config_merge;
import std;
export import folia.mermaid.config_limits;

export namespace folia::mermaid {
// Owns the result; neither input is mutated. Arrays union primitives using
// SameValueZero and keep distinct object values. At depth zero object properties
// are shallow-assigned. Null source properties above that boundary are skipped.
// Object properties on arrays are rejected: ConfigValue represents dense trees,
// not JS arrays with custom properties or holes. No default/schema coercion.
std::expected<ConfigValue, std::string> assign_config_with_depth(
    const ConfigValue& destination, const ConfigValue& source, int merge_depth = 2,
    ConfigLimits limits = {});
}
