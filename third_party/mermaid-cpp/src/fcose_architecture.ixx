// Architecture's two-pass fCoSE consumer. MIT, FCOSE-LICENSE and CYTOSCAPE-LICENSE.
export module folia.mermaid.fcose_architecture;
export import folia.mermaid.fcose_layout;
export import folia.mermaid.architecture_measurement;
import std;
export namespace folia::mermaid {
struct ArchitectureLayoutResult {std::vector<FCoseNode> nodes;std::array<std::size_t,2> iterations{};};
std::expected<ArchitectureLayoutResult,std::string> layout_architecture(const ArchitectureDocument&,const ArchitectureCyMeasurements* = nullptr);
}
