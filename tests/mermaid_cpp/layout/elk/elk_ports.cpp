#include "support/folia_test.hpp"
import folia.mermaid.elk_ports;
import folia.mermaid.elk_order;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> fields(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;
    if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}
    return result;
}
double decimal(std::string_view text) {
    double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK ports fixture number");return value;
}
suite elk_ports = [] {
    "mermaid_elk_point_ports_match_original_side_order_and_geometry"_test = [] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-ports.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.empty()||line.front()=='#')continue;
            const auto row=fields(line,'\t');expect(fatal(row.size()==7_u));
            const auto directions=row[3]=="-"?std::string_view{}:row[3];
            ElkLayeredGraph graph;graph.nodes.resize(1+directions.size());graph.nodes[0].layer=1;
            graph.nodes[0].width=decimal(row[1]);graph.nodes[0].height=decimal(row[2]);graph.layers.resize(3);graph.layers[1]={0};
            for(std::size_t i=0;i<directions.size();++i) {
                const bool output=directions[i]=='o';const auto p=graph.ports.size();
                graph.nodes[0].ports.push_back(p);graph.nodes[i+1].ports={p+1};graph.nodes[i+1].layer=output?2:0;
                graph.layers[output?2:0].push_back(i+1);
                graph.ports.push_back({.node=0,.incoming=output?std::vector<std::size_t>{}:std::vector<std::size_t>{i},.outgoing=output?std::vector<std::size_t>{i}:std::vector<std::size_t>{}});
                graph.ports.push_back({.node=i+1,.incoming=output?std::vector<std::size_t>{i}:std::vector<std::size_t>{},.outgoing=output?std::vector<std::size_t>{}:std::vector<std::size_t>{i}});
                graph.edges.push_back({.source_port=output?p:p+1,.target_port=output?p+1:p});
            }
            elk_assign_flat_port_sides(graph);expect(fatal(elk_sort_flat_ports(graph).has_value()));
            std::vector<ElkPointPortOptions> options(graph.nodes.size());options[0].center_alignment=row[4]=="1";
            expect(fatal(elk_place_flat_point_ports(graph,options).has_value()));
            const auto order=fields(row[5],','),ports=fields(row[6],';');
            expect(fatal(order.size()==directions.size()&&ports.size()==directions.size()));
            for(std::size_t i=0;i<directions.size();++i) {
                expect(graph.nodes[0].ports[i]/2==static_cast<std::size_t>(decimal(order[i])))<<row[0]<<i;
                const auto values=fields(ports[i],',');const auto& p=graph.ports[2*i];
                expect(static_cast<int>(p.side)==static_cast<int>(decimal(values[0])))<<row[0]<<i;
                expect(std::abs(p.x-decimal(values[1]))<1e-9&&std::abs(p.y-decimal(values[2]))<1e-9)<<row[0]<<i<<p.x<<p.y;
            }
            ++count;
        }
        expect(count==8_u);
    };
};
}
