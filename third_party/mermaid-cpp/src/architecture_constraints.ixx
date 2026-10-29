// Original Architecture renderer getAlignments/getRelativeConstraints. MIT.
export module folia.mermaid.architecture_constraints;
export import folia.mermaid.architecture;
import std;
export namespace folia::mermaid {
struct ArchitectureRelativePlacement {std::u16string left,right,top,bottom;double gap=0;};
struct ArchitectureConstraints {
    std::vector<std::vector<std::u16string>> horizontal,vertical;
    std::vector<ArchitectureRelativePlacement> relative;
};
std::expected<ArchitectureConstraints,std::string> architecture_constraints(const ArchitectureDocument&);
}
