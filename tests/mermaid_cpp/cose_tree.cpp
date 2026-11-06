#include "support/folia_test.hpp"
import folia.mermaid.cose_tree;
import folia.mermaid.frontend;
import folia.mermaid.number;

namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite cose_tree=[] {
    "mermaid_mindmap_cose_tree_matches_original_positions_and_iterations"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"mindmap-layout.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            std::vector<std::string_view> fields;std::string_view rest=line;
            for(;;){auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}
            expect(fatal(fields.size()==3_u));++count;
            const auto data=json(decode(fields[1])),expected=json(decode(fields[2]));
            std::vector<CoseTreeNode> nodes;std::vector<CoseTreeEdge> edges;
            for(const auto& n:field(data,u"nodes").sequence)nodes.push_back({field(n,u"id").string,field(n,u"width").number,field(n,u"height").number});
            for(const auto& e:field(data,u"edges").sequence)edges.push_back({field(e,u"source").string,field(e,u"target").string});
            const auto actual=layout_cose_tree(nodes,edges);
            expect(fatal(actual.has_value()))<<fields[0]<<(actual?"":actual.error());
            expect(actual->iterations==field(expected,u"iterations").number)<<fields[0];
            const auto& positions=field(expected,u"nodes").sequence;
            expect(fatal(actual->nodes.size()==positions.size()));
            for(std::size_t i=0;i<positions.size();++i) {
                expect(bool(actual->nodes[i].id==field(positions[i],u"id").string));
                expect(std::abs(actual->nodes[i].x-field(positions[i],u"x").number)<1e-6)<<fields[0]<<actual->nodes[i].x<<field(positions[i],u"x").number;
                expect(std::abs(actual->nodes[i].y-field(positions[i],u"y").number)<1e-6)<<fields[0]<<actual->nodes[i].y<<field(positions[i],u"y").number;
            }
        }
        expect(count==5_u);
    };
};
}
