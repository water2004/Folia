#include "support/folia_test.hpp"
import folia.mermaid.elk_order;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
std::size_t number(std::string_view text) {
    std::size_t value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK sweep fixture number");return value;
}
void same(const std::vector<std::size_t>& values,std::string_view expected,std::string_view name) {
    const auto items=split(expected,',');expect(fatal(items.size()==values.size()))<<name;
    for(std::size_t i=0;i<values.size();++i)expect(values[i]==number(items[i]))<<name<<i;
}
suite elk_hierarchy_sweep=[] {
    "mermaid_elk_recursive_hierarchy_sweep_matches_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-hierarchy-sweep.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==7_u));const auto name=row[0];
            const auto specs=split(row[2],'|');std::vector<ElkLayeredGraph> graphs(specs.size());
            std::vector<ElkRandom> randoms;randoms.reserve(specs.size());
            for(std::size_t i=0;i<specs.size();++i)randoms.emplace_back(number(row[1]));
            std::vector<ElkHierarchyOrderGraph> entries(specs.size());
            for(std::size_t i=0;i<specs.size();++i) {
                const auto spec=split(specs[i],'@');auto& g=graphs[i];auto& entry=entries[i];
                entry.graph=&g;entry.random=&randoms[i];entry.parent_node=number(spec[3]);
                if(spec[2]!="-1")entry.parent_graph=number(spec[2]);
                for(const auto value:split(spec[0],',')) {
                    const auto n=g.nodes.size(),l=number(value);g.nodes.emplace_back();g.nodes.back().layer=l;
                    g.layers.resize(std::max(g.layers.size(),l+1));g.layers[l].push_back(n);
                }
                for(const auto value:split(spec[1],',')) {
                    const auto pair=split(value,':');const auto s=number(pair[0]),t=number(pair[1]);
                    const auto p=g.ports.size(),e=g.edges.size();
                    g.ports.push_back({s,ElkPortSide::east,0,0,{}, {e}});g.ports.push_back({t,ElkPortSide::west,0,0,{e}, {}});
                    g.nodes[s].ports.push_back(p);g.nodes[t].ports.push_back(p+1);g.edges.push_back({e,p,p+1});
                }
                for(const auto value:split(spec[4],',')) {
                    const auto pair=split(value,':');const auto n=number(pair[0]);
                    entry.ports.push_back({n,number(pair[1])});g.nodes[n].kind=ElkLayerNodeKind::external_port;
                }
                expect(fatal(elk_sort_flat_ports(g).has_value()))<<name;
            }
            const auto bottom=split(row[4],',');
            for(std::size_t i=1;i<entries.size();++i) {
                const auto& e=entries[i];expect(elk_hierarchy_uses_bottom_up(*e.graph,graphs[*e.parent_graph],e.parent_node,e.ports)==(number(bottom[i])!=0))<<name<<i;
            }
            const auto result=elk_order_hierarchy(entries);expect(fatal(result.has_value()))<<name;
            expect(*result==number(row[5]))<<name;
            const auto next_random=split(row[6],',');
            for(std::size_t i=0;i<randoms.size();++i) {
                const auto text=next_random[i];double expected{};
                const auto parsed=std::from_chars(text.data(),text.data()+text.size(),expected);
                expect(fatal(parsed.ec==std::errc{}));expect(std::abs(randoms[i].next_double()-expected)<1e-15)<<name<<i;
            }
            const auto expected=split(row[3],'|');expect(fatal(expected.size()==graphs.size()));
            for(std::size_t i=0;i<graphs.size();++i) {
                const auto parts=split(expected[i],'/'),layers=split(parts[0],';'),ports=split(parts[1],';');
                expect(fatal(layers.size()==graphs[i].layers.size()));expect(fatal(ports.size()==graphs[i].nodes.size()));
                for(std::size_t l=0;l<layers.size();++l)same(graphs[i].layers[l],layers[l],name);
                for(std::size_t n=0;n<ports.size();++n)same(graphs[i].nodes[n].ports,ports[n],name);
            }
            ++count;
        }
        expect(count==5_u);
    };
};
}
