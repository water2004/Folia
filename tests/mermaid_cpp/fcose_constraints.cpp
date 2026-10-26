#include "support/folia_test.hpp"
import folia.mermaid.fcose_constraints;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite fcose_constraints_tests=[] {
    "mermaid_fcose_incremental_constraints_match_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(override?override:FOLIA_MERMAID_FIXTURE_DIR)/"architecture-layout.tsv"};expect(fatal(input.is_open()));
        std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            std::vector<std::string_view> fields;std::string_view rest=line;for(;;){auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}
            expect(fatal(fields.size()==4_u));++count;const auto data=json(decode(fields[3]));
            for(const auto& pass:field(data,u"constraintPasses").sequence) {
                std::vector<FCoseConstraintNode> nodes;for(const auto& n:field(pass,u"input").sequence)nodes.push_back({field(n,u"id").string,field(n,u"x").number,field(n,u"y").number});
                ArchitectureConstraints constraints;const auto& original=field(pass,u"constraints");const auto& alignment=field(original,u"alignmentConstraint");
                for(const auto& [key,groups]:{std::pair{u"horizontal",&constraints.horizontal},{u"vertical",&constraints.vertical}})if(const auto* array=alignment.find(key))for(const auto& g:array->sequence){std::vector<std::u16string> ids;for(const auto& id:g.sequence)ids.push_back(id.string);groups->push_back(std::move(ids));}
                if(const auto* relative=original.find(u"relativePlacementConstraint"))for(const auto& edge:relative->sequence) {
                    ArchitectureRelativePlacement r;r.gap=field(edge,u"gap").number;
                    for(const auto& [key,value]:{std::pair{u"left",&r.left},{u"right",&r.right},{u"top",&r.top},{u"bottom",&r.bottom}})if(const auto* item=edge.find(key))*value=item->string;
                    constraints.relative.push_back(std::move(r));
                }
                expect(fatal(enforce_fcose_constraints(nodes,constraints).has_value()));const auto& expected=field(pass,u"output").sequence;expect(fatal(nodes.size()==expected.size()));
                for(std::size_t i=0;i<nodes.size();++i){expect(std::abs(nodes[i].x-field(expected[i],u"x").number)<1e-9)<<fields[0];expect(std::abs(nodes[i].y-field(expected[i],u"y").number)<1e-9)<<fields[0];}
            }
        }
        expect(count==6_u);
    };
};
}
