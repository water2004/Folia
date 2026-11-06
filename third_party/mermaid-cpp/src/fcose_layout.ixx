// fCoSE 2.2.0 incremental compound spring embedder. See FCOSE-LICENSE.
export module folia.mermaid.fcose_layout;
export import folia.mermaid.fcose_movement;
import std;
export namespace folia::mermaid {
struct FCoseNode {std::u16string id,parent;double x=0,y=0,width=0,height=0,padding=0;};
struct FCoseEdge {std::u16string source,target;double ideal=50,elasticity=.45;};
struct FCoseLayoutInput {
    std::vector<FCoseNode> nodes;std::vector<FCoseEdge> edges;ArchitectureConstraints constraints;
    std::size_t max_iterations=2500;std::uint32_t seed=1;
};
struct FCoseLayoutResult {std::vector<FCoseNode> nodes;std::size_t iterations=0;};
std::expected<FCoseLayoutResult,std::string> layout_fcose_incremental(const FCoseLayoutInput&);
}
