// Mermaid 12.2.0 utils.ts detectInit mapping. MIT, see ../LICENSE.
export module folia.mermaid.config_init;
import std;
export import folia.mermaid.config_limits;
export import folia.mermaid.directives;

export namespace folia::mermaid {
// Dispatch remains caller-owned: capture the current config in this resolver.
// Called once with unchanged source iff directive results are JS-truthy, even
// when there is no `config` property. A resolver error propagates unchanged.
using InitDiagramResolver = std::function<std::expected<std::u16string, std::string>(std::u16string_view)>;
// Single init args bypass sanitize/merge; multiple args are sanitized and
// assignWithDepth-merged. Only flowchart-v2 maps to flowchart. An empty optional
// means JS undefined, distinct from a present null/empty mapping. No shared state.
// Prototype setter effects are rejected; ordinary own __proto__ data is retained.
// Each composed config operation has ConfigLimits; parsing has DirectiveLimits.
// Optional scan_state is local to one preprocessing request, allowing its wrap
// scan to continue after malformed init JSON. Omission starts a fresh scan.
std::expected<std::optional<ConfigValue>, std::string> detect_init_config(
    std::u16string_view source, const InitDiagramResolver& resolve_diagram,
    ConfigLimits limits = {}, const DirectiveLimits& directive_limits = {},
    DirectiveScanState* scan_state = nullptr);
}
