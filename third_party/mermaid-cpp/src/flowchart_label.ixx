// Plain display labels from Mermaid 12.2.0 labelHelper/createText. MIT.
export module folia.mermaid.flowchart_label;
import std;

export namespace folia::mermaid {
// Convert the retained parser label into display lines. Entity decoding does
// not modify source/DB labels, and decoded angle brackets are text, not markup.
// Styled HTML/Markdown labels are a separate, not-yet-implemented adapter.
std::expected<std::vector<std::u16string>,std::string> flowchart_label_lines(
    std::u16string_view label, std::u16string_view type);
}
