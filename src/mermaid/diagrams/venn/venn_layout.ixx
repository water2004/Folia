// Venn.js 2.0.0 circle geometry and layout. MIT; see THIRD_PARTY_NOTICES.md.
export module folia.mermaid.venn_layout;
import std;
export import folia.mermaid.venn;

export namespace folia::mermaid {
struct VennPoint { double x, y; };
struct VennCircle { VennPoint center; double radius; };
struct VennArc { VennCircle circle; VennPoint start, end; bool large; };
struct VennRegion {
    VennArea data;
    VennPoint label;
    std::vector<VennCircle> circles;
    std::vector<VennArc> arcs;
};
std::expected<std::vector<VennRegion>, std::string> layout_venn(
    const VennDiagram&, double width, double height, double padding, bool include_zero_sets = false);
}
