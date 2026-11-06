#include "support/folia_test.hpp"
import folia.mermaid.fcose_movement;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite fcose_movement_tests=[] {
    "mermaid_fcose_seeded_constraint_movement_matches_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(override?override:FOLIA_MERMAID_FIXTURE_DIR)/"architecture-layout.tsv"};expect(fatal(input.is_open()));
        std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto last=line.rfind('\t');expect(fatal(last!=line.npos));const auto data=json(decode(std::string_view(line).substr(last+1)));++count;
            const auto& passes=field(data,u"constraintPasses").sequence;expect(fatal(passes.size()==2_u));
            for(const auto& pass:passes) {
                ArchitectureConstraints constraints;const auto& original=field(pass,u"constraints");const auto& alignment=field(original,u"alignmentConstraint");
                for(const auto& [key,groups]:{std::pair{u"horizontal",&constraints.horizontal},{u"vertical",&constraints.vertical}})if(const auto* array=alignment.find(key))for(const auto& g:array->sequence){std::vector<std::u16string> ids;for(const auto& id:g.sequence)ids.push_back(id.string);groups->push_back(std::move(ids));}
                if(const auto* relative=original.find(u"relativePlacementConstraint"))for(const auto& edge:relative->sequence) {
                    ArchitectureRelativePlacement r;r.gap=field(edge,u"gap").number;
                    for(const auto& [key,value]:{std::pair{u"left",&r.left},{u"right",&r.right},{u"top",&r.top},{u"bottom",&r.bottom}})if(const auto* item=edge.find(key))*value=item->string;
                    constraints.relative.push_back(std::move(r));
                }
                const auto& movements=field(pass,u"movement").sequence;expect(fatal(movements.size()==10_u));
                std::vector<FCoseMovementNode> nodes;for(const auto& n:field(movements.front(),u"input").sequence)nodes.push_back({field(n,u"id").string,field(n,u"x").number,field(n,u"y").number});
                auto state=FCoseConstraintMovement::create(nodes,constraints);expect(fatal(state.has_value()));
                for(const auto& step:movements) {
                    const auto& before=field(step,u"input").sequence;const auto& expected=field(step,u"output").sequence;
                    for(std::size_t i=0;i<nodes.size();++i){nodes[i].dx=field(before[i],u"dx").number;nodes[i].dy=field(before[i],u"dy").number;}
                    state->update(nodes,static_cast<std::size_t>(field(step,u"iteration").number));
                    for(std::size_t i=0;i<nodes.size();++i){expect(std::abs(nodes[i].dx-field(expected[i],u"dx").number)<1e-9)<<nodes[i].id.size();expect(std::abs(nodes[i].dy-field(expected[i],u"dy").number)<1e-9)<<nodes[i].id.size();}
                }
            }
        }
        expect(count==6_u);
    };
};
}
