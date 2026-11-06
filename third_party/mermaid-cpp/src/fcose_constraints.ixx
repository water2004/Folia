// CoSE 2.2.0 ConstraintHandler incremental Architecture path. See FCOSE-LICENSE.
export module folia.mermaid.fcose_constraints;
export import folia.mermaid.architecture_constraints;
import std;
export namespace folia::mermaid {
struct FCoseConstraintNode {std::u16string id;double x=0,y=0;};
// Architecture supplies no fixed-node constraints and disables randomization.
std::expected<void,std::string> enforce_fcose_constraints(std::span<FCoseConstraintNode>,const ArchitectureConstraints&);
}
