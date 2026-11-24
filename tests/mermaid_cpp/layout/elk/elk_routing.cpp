#include "support/folia_test.hpp"
import folia.mermaid.elk_routing;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> fields(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;
    if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}
    return result;
}
std::size_t number(std::string_view text) {
    std::size_t value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK routing fixture number");return value;
}
double decimal(std::string_view text) {
    double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK routing fixture decimal");return value;
}
suite elk_routing = [] {
    "mermaid_elk_orthogonal_routing_matches_original_slots_bends_and_layer_widths"_test = [] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-routing.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.empty()||line.front()=='#')continue;
            const auto row=fields(line,'\t');expect(fatal(row.size()==11_u));
            ElkLayeredGraph graph;std::vector<double> positions;std::vector<ElkHorizontalMargin> margins;
            const auto assignments=fields(row[1],',');graph.nodes.resize(assignments.size());
            for(std::size_t n=0;n<assignments.size();++n) {
                auto& node=graph.nodes[n];node.layer=number(assignments[n]);node.width=40+n*7;node.height=24;
                if(graph.layers.size()<=node.layer)graph.layers.resize(node.layer+1);graph.layers[node.layer].push_back(n);
                margins.push_back({static_cast<double>(n%3),static_cast<double>((n+1)%4)});
            }
            for(const auto value:fields(row[2],','))positions.push_back(decimal(value));
            for(const auto value:fields(row[4],',')){auto& node=graph.nodes[number(value)];node.kind=ElkLayerNodeKind::long_edge;node.width=0;node.height=1;}
            for(const auto endpoints:fields(row[3],',')) {
                const auto values=fields(endpoints,':');const auto s=number(values[0]),t=number(values[1]);
                const auto e=graph.edges.size(),p=graph.ports.size();
                graph.ports.push_back({.node=s,.side=ElkPortSide::east,.y=decimal(values[2]),.outgoing={e}});
                graph.ports.push_back({.node=t,.side=ElkPortSide::west,.y=decimal(values[3]),.incoming={e}});
                graph.nodes[s].ports.push_back(p);graph.nodes[t].ports.push_back(p+1);
                graph.edges.push_back({.source_port=p,.target_port=p+1});
            }
            const auto result=elk_route_flat_edges(graph,positions,margins,{},number(row[5]));
            expect(fatal(result.has_value()))<<row[0];
            const auto xs=fields(row[6],','),widths=fields(row[7],','),slots=fields(row[8],',');
            expect(fatal(xs.size()==result->node_x.size()&&widths.size()==result->layer_width.size()&&slots.size()==result->gap_slots.size()));
            for(std::size_t n=0;n<xs.size();++n)expect(std::abs(result->node_x[n]-decimal(xs[n]))<1e-9)<<row[0]<<n<<result->node_x[n];
            for(std::size_t l=0;l<widths.size();++l)expect(std::abs(result->layer_width[l]-decimal(widths[l]))<1e-9)<<row[0]<<l;
            for(std::size_t l=0;l<slots.size();++l)expect(result->gap_slots[l]==number(slots[l]))<<row[0]<<l;
            expect(std::abs(result->width-decimal(row[9]))<1e-9)<<row[0];
            // A single straight edge is represented by the '-' empty bend list.
            auto edges=fields(row[10],';');if(edges.empty()&&graph.edges.size()==1)edges.push_back("-");
            expect(fatal(edges.size()==result->bends.size()));
            for(std::size_t e=0;e<edges.size();++e) {
                const auto bends=fields(edges[e],':');expect(fatal(bends.size()==result->bends[e].size()))<<row[0]<<e;
                for(std::size_t p=0;p<bends.size();++p) {
                    const auto xy=fields(bends[p],',');const auto point=result->bends[e][p];
                    expect(std::abs(point.x-decimal(xy[0]))<1e-9&&std::abs(point.y-decimal(xy[1]))<1e-9)<<row[0]<<e<<p<<point.x<<point.y;
                }
            }
            ++count;
        }
        expect(count==7_u);
    };
};
}
