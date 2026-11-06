// Internal Khroma 2.1.0 primitives used by the pinned Mermaid themes. MIT.
export module folia.mermaid.theme_color;
import std;

export namespace folia::mermaid::theme_detail {
// Throw runtime_error for unsupported color syntax/resource limits. Values are
// CSS literal strings, not mutable Khroma Channels or browser-resolved paints.
std::u16string color_adjust(std::u16string_view, const std::map<char, double>&);
std::u16string color_lighten(std::u16string_view, double amount);
std::u16string color_invert(std::u16string_view);
std::u16string color_rgba(double r, double g, double b, double a);
bool color_is_dark(std::u16string_view);
}
