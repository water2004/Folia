// CoSE 2.2.0 incremental constraint movement. See FCOSE-LICENSE.
export module folia.mermaid.fcose_movement;
export import folia.mermaid.fcose_constraints;
import std;
export namespace folia::mermaid {
struct FCoseMovementNode {std::u16string id;double x=0,y=0,dx=0,dy=0;};
class FCoseConstraintMovement {
    struct Relation {std::size_t other=0;double gap=0;bool forward=false;};
    struct Slot {std::u16string id;std::vector<std::size_t> members;std::vector<Relation> relations;double position=0;};
    struct Axis {std::vector<std::vector<std::size_t>> alignments;std::vector<Slot> slots;std::vector<std::size_t> order;};
    Axis x_,y_;std::uint32_t state_=1;
    double random();
    static Axis build_axis(std::span<const FCoseMovementNode>,const ArchitectureConstraints&,bool);
    void update_axis(Axis&,std::span<FCoseMovementNode>,bool);
    void shuffle(Axis&);
public:
    static std::expected<FCoseConstraintMovement,std::string> create(
        std::span<const FCoseMovementNode>,const ArchitectureConstraints&,std::uint32_t seed=1);
    void update(std::span<FCoseMovementNode>,std::size_t iteration);
};
}
