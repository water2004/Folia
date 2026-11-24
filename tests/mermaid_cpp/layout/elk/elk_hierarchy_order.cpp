#include "support/folia_test.hpp"
import folia.mermaid.elk_hierarchy_order;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
std::size_t number(std::string_view text) {
    std::size_t value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK hierarchy fixture number");return value;
}
void same(const std::vector<std::size_t>& values,std::string_view expected,std::string_view name) {
    const auto items=split(expected,',');expect(fatal(items.size()==values.size()))<<name;
    for(std::size_t i=0;i<values.size();++i)expect(values[i]==number(items[i]))<<name<<i;
}
suite elk_hierarchy_order=[] {
    "mermaid_elk_hierarchical_sweep_selection_and_port_transfer_match_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-hierarchy-order.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==13_u));const auto name=row[0];
            const auto assignment=split(row[1],','),roles=split(row[2],',');
            ElkLayeredGraph child,parent;parent.nodes.resize(1);child.nodes.resize(roles.size());
            std::vector<ElkHierarchyPortLink> links;
            for(std::size_t n=0;n<roles.size();++n) {
                const auto l=number(assignment[n]);child.layers.resize(std::max(child.layers.size(),l+1));
                child.layers[l].push_back(n);child.nodes[n].layer=l;
                if(const auto role=number(roles[n])) {
                    child.nodes[n].kind=ElkLayerNodeKind::external_port;
                    const auto p=parent.ports.size();parent.ports.push_back({0,role==1?ElkPortSide::west:ElkPortSide::east});
                    links.push_back({n,p});
                }
            }
            for(const auto value:split(row[6],','))parent.nodes[0].ports.push_back(number(value));
            for(const auto value:split(row[3],',')) {
                const auto endpoints=split(value,':');const auto s=number(endpoints[0]),t=number(endpoints[1]);
                const auto p=child.ports.size(),e=child.edges.size();
                child.ports.push_back({s,ElkPortSide::east,0,0,{}, {e}});
                child.ports.push_back({t,ElkPortSide::west,0,0,{e}, {}});
                child.nodes[s].ports.push_back(p);child.nodes[t].ports.push_back(p+1);
                child.edges.push_back({e,p,p+1});
            }
            expect(elk_hierarchy_uses_bottom_up(child,parent,0,links,number(row[4])!=0)==(number(row[5])!=0))<<name;
            for(const bool forward:{true,false}) {
                auto working=child,outer=parent;const std::size_t offset=forward?7:10;
                expect(elk_hierarchy_enter(working,outer,0,links,forward)==(number(row[offset])!=0))<<name;
                same(forward?working.layers.front():working.layers.back(),row[offset+1],name);
                std::ranges::reverse(forward?working.layers.back():working.layers.front());
                elk_hierarchy_leave(working,outer,0,links,forward);
                same(outer.nodes[0].ports,row[offset+2],name);
            }
            ++count;
        }
        expect(count==6_u);
    };
};
}
