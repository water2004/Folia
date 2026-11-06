#include "support/folia_test.hpp"
import folia.mermaid.elk_compound_graph;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
int number(std::string_view text) {
    int value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK compound graph fixture number");return value;
}
std::string join(const std::vector<std::string>& values,char delimiter) {
    std::string result;for(const auto& v:values){if(!result.empty())result+=delimiter;result+=v;}return result.empty()?"-":result;
}
std::string index(std::optional<std::size_t> value){return value?std::to_string(*value):"-1";}
suite elk_compound_graph=[] {
    "mermaid_elk_compound_preprocessor_matches_original_segments_and_label_owners"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-compound-graph.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==10_u));const auto name=row[0];ElkCompoundInput graph;
            for(const auto p:split(row[1],',')){const auto n=number(p);graph.nodes.push_back({.parent=n<0?std::nullopt:std::optional{static_cast<std::size_t>(n)}});}
            const auto dirs=split(row[3],',');graph.direction=static_cast<ElkLayoutDirection>(number(dirs[0]));std::size_t di=1;
            for(const auto g:split(row[2],',')){auto& node=graph.nodes[number(g)];node.group=true;node.direction=static_cast<ElkLayoutDirection>(number(dirs[di++]));}
            for(const auto item:split(row[4],',')){const auto v=split(item,':');graph.edges.push_back({static_cast<std::size_t>(number(v[0])),static_cast<std::size_t>(number(v[1]))});}
            const auto prepared=elk_prepare_compound_graph(graph);expect(fatal(prepared.has_value()))<<name;
            const auto& result=*prepared;std::vector<std::string> graphs,ports,nodes,chains,owners;
            for(const auto& g:result.graphs){std::vector<std::string> ns;for(const auto n:g.nodes)ns.push_back(std::to_string(n));graphs.push_back(join(ns,','));}
            for(const auto& p:result.ports)ports.push_back(std::to_string(p.node)+","+std::to_string(static_cast<int>(p.side)));
            for(const auto& n:result.nodes)nodes.push_back(std::to_string(n.graph)+","+index(n.nested_graph)+","+index(n.boundary_port)+","+std::to_string(n.boundary?static_cast<int>(n.boundary->side):0));
            const auto key=[&](std::size_t i){const auto& s=result.segments[i];return std::to_string(s.graph)+","+std::to_string(s.source_port)+","+std::to_string(s.target_port);};
            for(std::size_t e=0;e<result.edges.size();++e){const auto& edge=result.edges[e];std::vector<std::string> segments;
                for(const auto s:edge.segments){expect(result.segments[s].original_edge==e)<<name;segments.push_back(key(s));}
                chains.push_back(join(segments,':'));owners.push_back(key(edge.center_segment)+":"+key(edge.tail_segment)+":"+key(edge.head_segment));}
            expect(join(graphs,';')==row[5])<<name<<join(graphs,';');expect(join(ports,';')==row[6])<<name<<join(ports,';');
            expect(join(nodes,';')==row[7])<<name<<join(nodes,';');expect(join(chains,';')==row[8])<<name<<join(chains,';');expect(join(owners,';')==row[9])<<name<<join(owners,';');++count;
        }
        expect(count==7_u);
    };
};
}
