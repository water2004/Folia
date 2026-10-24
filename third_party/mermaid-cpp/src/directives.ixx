// Mermaid 12.2.0 utils.ts detectDirective, MIT licensed; see ../LICENSE.
export module folia.mermaid.directives;
import std;
export import folia.mermaid.config_value;

export namespace folia::mermaid {
// Exact filters used by the common frontend: /(?:init\b)|(?:initialize\b)/
// and the case-sensitive, unanchored string pattern 'wrap' (also matches nowrap).
enum class DirectiveFilter { all, init, wrap };
struct Directive {
    // Absent on a syntax failure, matching upstream's {type: undefined,args:null}.
    std::optional<std::u16string> type;
    ConfigValue args;
};
struct DirectiveLimits {
    std::size_t source_units = 1024 * 1024;
    std::size_t depth = 64; // Hard ceiling 64; root value has depth zero.
    std::size_t values = 100000; // Includes overwritten duplicate object values.
    std::size_t string_units = 4 * 1024 * 1024; // Decoded keys, strings and types.
    std::size_t directives = 10000; // All matches, even if filtered out.
};
// A single preprocessing request shares the original regexp cursor between
// init and wrap scans. Syntax failures retain it; a completed scan resets it.
// Never share this state between requests or store it globally.
struct DirectiveScanState { std::size_t cursor = 0; };
std::expected<std::vector<Directive>, std::string> detect_directives(
    std::u16string_view text, DirectiveFilter filter, DirectiveScanState& state,
    const DirectiveLimits& limits = {});
// Owned, ordered results; the JS singleton/array union is normalized to a vector.
// No match returns one entry with the transformed input as type and null args.
// Invalid selected JSON discards earlier results and returns one absent type.
// Resource failures return unexpected, never a successful partial result.
// Every call starts scanning at zero; no shared regex lastIndex leaks across calls.
// This raw boundary neither sanitizes settings nor merges/applies configuration.
std::expected<std::vector<Directive>, std::string> detect_directives(
    std::u16string_view text, DirectiveFilter filter = DirectiveFilter::all,
    const DirectiveLimits& limits = {});
}
