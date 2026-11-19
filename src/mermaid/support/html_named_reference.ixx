export module folia.mermaid.html_named_reference;
import std;

export namespace folia::mermaid {
// The name excludes the leading ampersand and terminating semicolon.
std::optional<std::u16string> mermaid_html_named_reference(std::u16string_view name);
}
