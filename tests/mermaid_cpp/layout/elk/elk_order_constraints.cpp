#include "support/folia_test.hpp"
import folia.mermaid.elk_order_constraints;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
std::size_t number(std::string_view text) {
    std::size_t value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK N/S fixture number");return value;
}
void same(const std::vector<std::size_t>& actual,std::string_view text,std::string_view name) {
    const auto values=split(text,',');expect(fatal(values.size()==actual.size()))<<name;
    for(std::size_t i=0;i<values.size();++i)expect(actual[i]==number(values[i]))<<name<<i;
}
void layers(const ElkLayeredGraph& graph,std::string_view text,std::string_view name) {
    const auto expected=split(text,';');expect(fatal(expected.size()==graph.layers.size()))<<name;
    for(std::size_t l=0;l<expected.size();++l)same(graph.layers[l],expected[l],name);
}
void edges(const ElkLayeredGraph& graph,std::string_view text,std::string_view name) {
    const auto expected=split(text,',');expect(fatal(expected.size()==graph.edges.size()))<<name;
    for(std::size_t e=0;e<expected.size();++e){const auto p=split(expected[e],':');expect(graph.edges[e].source_port==number(p[0])&&graph.edges[e].target_port==number(p[1]))<<name<<e;}
}
suite elk_order_constraints=[] {
    "mermaid_elk_north_south_forster_constraints_and_count_match_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-order-constraints.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==8_u));const auto name=row[0];ElkLayeredGraph g;
            for(const auto item:split(row[1],',')){const auto l=number(item),n=g.nodes.size();g.nodes.emplace_back();g.nodes.back().layer=l;g.layers.resize(std::max(g.layers.size(),l+1));g.layers[l].push_back(n);}
            for(const auto item:split(row[2],',')) {
                const auto v=split(item,':');const auto s=number(v[0]),t=number(v[1]),p=g.ports.size(),e=g.edges.size();
                g.ports.push_back({s,static_cast<ElkPortSide>(number(v[2])),0,0,{}, {e}});g.ports.push_back({t,static_cast<ElkPortSide>(number(v[3])),0,0,{e}, {}});
                g.nodes[s].ports.push_back(p);g.nodes[t].ports.push_back(p+1);g.edges.push_back({e,p,p+1});
            }
            for(const auto item:split(row[3],','))g.nodes[number(item)].fixed_port_positions=true;
            const auto prepared=elk_prepare_north_south_ports(g);expect(fatal(prepared.has_value()))<<name;
            const auto constraints=elk_make_order_constraints(g,*prepared);
            std::vector<std::optional<double>> barycenters;
            for(const auto value:split(row[4],',')) {
                double number{};const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
                expect(fatal(parsed.ec==std::errc{}));barycenters.push_back(number);
            }
            const auto counts=split(row[7],',');
            for(std::size_t l=0;l<g.layers.size();++l) {
                auto& layer=g.layers[l];std::stable_sort(layer.begin(),layer.end(),[&](auto a,auto b){return barycenters[a]<barycenters[b];});
                elk_resolve_order_constraints(constraints,layer,barycenters);
                expect(elk_count_north_south_crossings(g,*prepared,layer)==number(counts[l]))<<name<<l;
            }
            layers(g,row[5],name);const auto values=split(row[6],',');expect(fatal(values.size()==barycenters.size()));
            for(std::size_t n=0;n<values.size();++n){double expected{};const auto value=values[n];const auto parsed=std::from_chars(value.data(),value.data()+value.size(),expected);expect(fatal(parsed.ec==std::errc{}));expect(std::abs(*barycenters[n]-expected)<1e-12)<<name<<n;}
            ++count;
        }
        expect(count==4_u);
    };
};
}
