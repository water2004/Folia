// Cytoscape 3.34.0 measured bounds consumed by Architecture. CYTOSCAPE-LICENSE.
export module folia.mermaid.architecture_measurement;
export import folia.mermaid.fcose_layout;
import std;
export namespace folia::mermaid {
struct ArchitectureCyLabel {double width=0,height=0;};
struct ArchitectureCyMeasurements {std::vector<ArchitectureCyLabel> nodes,edges;};
struct ArchitectureCyBounds {double x=0,y=0,width=0,height=0;};
// Update compound auto-sizes and return the rendered collection bounds used
// by fCoSE to retain its pre-layout component center.
std::expected<ArchitectureCyBounds,std::string> measure_architecture_cy_bounds(
    const ArchitectureDocument&,std::span<FCoseNode>,const ArchitectureCyMeasurements&);
}
