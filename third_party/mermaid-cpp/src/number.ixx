export module folia.mermaid.number;
import std;

export namespace folia::mermaid {

// ECMAScript Number::toString decimal formatting for finite binary64 values.
// Uses shortest significant digits, decimal notation in [1e-6, 1e21), and
// canonical zero (including negative zero). Non-finite inputs have no result.
std::optional<std::string> format_number(double value);

}
