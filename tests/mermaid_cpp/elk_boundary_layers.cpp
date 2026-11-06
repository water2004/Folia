#include "support/folia_test.hpp"
import folia.mermaid.elk_boundary_layers;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
std::size_t number(std::string_view text) {
    std::size_t value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK boundary fixture number");return value;
}
suite elk_boundary_layers=[] {
    "mermaid_elk_external_boundary_layer_constraints_match_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-boundary-layers.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==6_u));const auto name=row[0];
            ElkCycleGraph graph;std::vector<ElkExternalLayerRole> roles;
            for(const auto item:split(row[1],','))roles.push_back(static_cast<ElkExternalLayerRole>(number(item)));graph.node_count=roles.size();
            for(const auto item:split(row[2],',')){const auto v=split(item,':');const auto p=graph.ports.size();graph.ports.push_back({number(v[0])});graph.ports.push_back({number(v[1])});graph.edges.push_back({p,p+1});}
            const auto result=elk_layer_boundary_graph(graph,roles);expect(fatal(result.has_value()))<<name;
            const auto reversed=split(row[3],',');expect(fatal(reversed.size()==result->reversed_edges.size()))<<name;
            for(std::size_t e=0;e<reversed.size();++e)expect(result->reversed_edges[e]==number(reversed[e]))<<name;
            const auto layers=split(row[4],',');expect(fatal(layers.size()==result->layers.size()));for(std::size_t n=0;n<layers.size();++n)expect(result->layers[n]==static_cast<int>(number(layers[n])))<<name<<n;
            const auto order=split(row[5],';');expect(fatal(order.size()==result->layer_nodes.size()))<<name;
            for(std::size_t l=0;l<order.size();++l){const auto nodes=split(order[l],',');expect(fatal(nodes.size()==result->layer_nodes[l].size()))<<name;for(std::size_t n=0;n<nodes.size();++n)expect(result->layer_nodes[l][n]==number(nodes[n]))<<name<<l<<n;}
            ++count;
        }
        expect(count==7_u);
    };
};
}
