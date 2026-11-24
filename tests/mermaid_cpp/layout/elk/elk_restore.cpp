#include "support/folia_test.hpp"
import folia.mermaid.elk_restore;
import folia.mermaid.elk_ports;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> fields(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}
    return result;
}
std::size_t number(std::string_view text) {
    std::size_t value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK restore fixture number");return value;
}
double decimal(std::string_view text) {
    double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK restore fixture decimal");return value;
}
void check_point(ElkRoutingPoint point,std::string_view expected,std::string_view name) {
    const auto xy=fields(expected,',');expect(fatal(xy.size()==2_u));
    expect(std::abs(point.x-decimal(xy[0]))<1e-9&&std::abs(point.y-decimal(xy[1]))<1e-9)<<name<<point.x<<point.y;
}
suite elk_restore = [] {
    "mermaid_elk_restores_original_joined_and_feedback_edge_paths"_test = [] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-restore.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=fields(line,'\t');expect(fatal(row.size()==5_u));
            const auto assignments=fields(row[1],',');ElkCycleGraph source{.node_count=assignments.size()};ElkLayeringResult layering;
            for(std::size_t n=0;n<assignments.size();++n) {
                const auto l=number(assignments[n]);layering.layers.push_back(static_cast<int>(l));
                if(layering.layer_nodes.size()<=l)layering.layer_nodes.resize(l+1);layering.layer_nodes[l].push_back(n);
            }
            for(const auto pair:fields(row[2],',')) {
                const auto ends=fields(pair,':');const auto p=source.ports.size();
                source.ports.push_back({number(ends[0])});source.ports.push_back({number(ends[1])});source.edges.push_back({p,p+1});
            }
            for(const auto e:fields(row[3],','))layering.reversed_edges.push_back(number(e));
            auto made=make_elk_layered_graph(source,layering);expect(fatal(made.has_value()));auto graph=std::move(*made);
            for(std::size_t n=0;n<graph.nodes.size();++n){graph.nodes[n].width=40+n*5;graph.nodes[n].height=24+n;}
            elk_assign_flat_port_sides(graph);elk_split_long_edges(graph);
            ElkOrthogonalRoutes routes;routes.node_x.resize(graph.nodes.size());routes.bends.resize(graph.edges.size());std::vector<double> y(graph.nodes.size());
            for(std::size_t n=0;n<graph.nodes.size();++n) {
                const auto& node=graph.nodes[n];routes.node_x[n]=node.layer*100;y[n]=n*30;
                for(const auto p:node.ports){auto& port=graph.ports[p];port.x=port.side==ElkPortSide::east?node.width:0;port.y=node.height/2;}
            }
            const auto anchor=[&](std::size_t p){const auto& port=graph.ports[p];return ElkRoutingPoint{routes.node_x[port.node]+port.x,y[port.node]+port.y};};
            for(std::size_t e=0;e<graph.edges.size();++e) {
                const auto a=anchor(graph.edges[e].source_port),b=anchor(graph.edges[e].target_port);const auto x=(a.x+b.x)/2;
                routes.bends[e]={{x,a.y},{x,b.y}};
            }
            const auto result=elk_restore_flat_routes(graph,routes,y);expect(fatal(result.has_value()));
            const auto expected=fields(row[4],';');expect(fatal(expected.size()==result->size()));
            for(std::size_t e=0;e<expected.size();++e) {
                const auto values=fields(expected[e],'|');expect(fatal(values.size()==7_u));const auto& route=(*result)[e];
                expect(route.original_edge==e&&route.source_node==number(values[0])&&route.target_node==number(values[1]))<<row[0]<<e;
                expect(route.source_port==number(values[2])&&route.target_port==number(values[3]))<<row[0]<<e;
                check_point(route.start,values[4],row[0]);check_point(route.end,values[5],row[0]);
                const auto bends=fields(values[6],':');expect(fatal(bends.size()==route.bends.size()));
                for(std::size_t p=0;p<bends.size();++p)check_point(route.bends[p],bends[p],row[0]);
            }
            ++count;
        }
        expect(count==4_u);
    };
};
}
