// Owned configuration operations have no JavaScript runtime dependency.
export module folia.mermaid.config_limits;
import std;
export import folia.mermaid.config_value;

export namespace folia::mermaid {
struct ConfigLimits {
    std::size_t depth = 64; // Root depth zero, hard ceiling 64.
    std::size_t values = 100000;
    std::size_t string_units = 4 * 1024 * 1024; // UTF-16 keys and values.
    std::size_t operations = 1000000; // Includes array-union comparisons.
};
}
