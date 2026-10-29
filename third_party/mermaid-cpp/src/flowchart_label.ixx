// Display labels from Mermaid 12.2.0 labelHelper/createText. MIT.
export module folia.mermaid.flowchart_label;
import std;

export namespace folia::mermaid {
// Convert the retained parser label into display lines. Entity decoding does
// not modify source/DB labels, and decoded angle brackets are text, not markup.
struct FlowchartLabelRun {
    std::u16string text;
    bool strong=false,emphasis=false;
};
using FlowchartLabelLine = std::vector<FlowchartLabelRun>;
std::expected<std::vector<FlowchartLabelLine>,std::string> flowchart_label_runs(
    std::u16string_view label, std::u16string_view type);
// createText's SVG words preserve literal HTML tags and use exclusive emphasis.
std::expected<std::vector<FlowchartLabelLine>,std::string> flowchart_svg_label_runs(
    std::u16string_view label, std::u16string_view type);
// Plain projection for consumers that only need display characters.
std::expected<std::vector<std::u16string>,std::string> flowchart_label_lines(
    std::u16string_view label, std::u16string_view type);
}
