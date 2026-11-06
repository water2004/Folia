// Mindmap's flat-tree branch of CoSE-Bilkent 4.1.0/cose-base 1.0.3.
// Copyright i-Vis Research Group, Bilkent University. MIT; see ../COSE-LICENSE.
export module folia.mermaid.cose_tree;
import std;

export namespace folia::mermaid {
struct CoseTreeNode {std::u16string id;double width=0,height=0;};
struct CoseTreeEdge {std::u16string start,end;};
struct CosePosition {std::u16string id;double x=0,y=0;};
struct CoseTreeLayout {std::vector<CosePosition> nodes;std::size_t iterations=0;};
// Original proof-quality defaults, with measured node boxes. This consumer is
// a single connected tree, never the compound/non-tree Architecture branch.
std::expected<CoseTreeLayout,std::string> layout_cose_tree(
    std::span<const CoseTreeNode>,std::span<const CoseTreeEdge>);
}
