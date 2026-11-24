#include "support/folia_test.hpp"
import folia.mermaid.elk_ports;
import folia.mermaid.elk_order;
import folia.mermaid.elk_placement;
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
std::uint64_t number(std::string_view text) {
    std::uint64_t value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK pipeline fixture number");return value;
}
double decimal(std::string_view text) {
    double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK pipeline fixture decimal");return value;
}
suite elk_pipeline = [] {
    "mermaid_elk_flat_phases_preserve_original_rng_and_geometry"_test = [] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-pipeline.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.empty()||line.front()=='#')continue;
            const auto row=fields(line,'\t');expect(fatal(row.size()==11_u));
            const auto assignments=fields(row[1],',');
            ElkCycleGraph source{.node_count=assignments.size()};ElkLayeringResult layering;
            for(std::size_t n=0;n<assignments.size();++n) {
                const auto l=number(assignments[n]);layering.layers.push_back(static_cast<int>(l));
                if(layering.layer_nodes.size()<=l)layering.layer_nodes.resize(l+1);layering.layer_nodes[l].push_back(n);
            }
            for(const auto pair:fields(row[2],',')) {
                const auto endpoints=fields(pair,':');const auto p=source.ports.size();
                source.ports.push_back({number(endpoints[0])});source.ports.push_back({number(endpoints[1])});
                source.edges.push_back({p,p+1});
            }
            auto made=make_elk_layered_graph(source,layering);expect(fatal(made.has_value()));auto graph=std::move(*made);
            for(std::size_t n=0;n<graph.nodes.size();++n){graph.nodes[n].width=50+n*7;graph.nodes[n].height=38+n*2;}
            elk_assign_flat_port_sides(graph);elk_split_long_edges(graph);
            expect(fatal(elk_sort_flat_ports(graph).has_value()));
            ElkRandom random(number(row[3]));
            expect(fatal(elk_order_flat_graph(graph,random).has_value()));
            expect(random.state==number(row[4]))<<row[0]<<" after order "<<random.state;
            expect(fatal(elk_place_flat_point_ports(graph).has_value()));
            const auto placed=elk_place_flat_nodes(graph,{},false);expect(fatal(placed.has_value()));
            const std::vector<ElkHorizontalMargin> margins(graph.nodes.size());
            const auto routes=elk_route_flat_edges(graph,*placed,margins,random);expect(fatal(routes.has_value()));
            expect(random.state==number(row[5]))<<row[0]<<" after routing "<<random.state;
            const auto layers=fields(row[6],';');expect(fatal(layers.size()==graph.layers.size()));
            for(std::size_t l=0;l<layers.size();++l) {
                const auto nodes=fields(layers[l],',');expect(fatal(nodes.size()==graph.layers[l].size()));
                for(std::size_t n=0;n<nodes.size();++n)expect(graph.layers[l][n]==number(nodes[n]))<<row[0]<<l<<n;
            }
            const auto xs=fields(row[7],','),ys=fields(row[8],',');
            expect(fatal(xs.size()==graph.nodes.size()&&ys.size()==graph.nodes.size()));
            for(std::size_t n=0;n<xs.size();++n) {
                expect(std::abs(routes->node_x[n]-decimal(xs[n]))<1e-8)<<row[0]<<n<<routes->node_x[n];
                expect(std::abs((*placed)[n]-decimal(ys[n]))<1e-8)<<row[0]<<n<<(*placed)[n];
            }
            expect(std::abs(routes->width-decimal(row[9]))<1e-8)<<row[0]<<routes->width;
            const auto edges=fields(row[10],';');expect(fatal(edges.size()==routes->bends.size()));
            for(std::size_t e=0;e<edges.size();++e) {
                const auto bends=fields(edges[e],':');expect(fatal(bends.size()==routes->bends[e].size()))<<row[0]<<e;
                for(std::size_t p=0;p<bends.size();++p) {
                    const auto xy=fields(bends[p],',');const auto point=routes->bends[e][p];
                    expect(std::abs(point.x-decimal(xy[0]))<1e-8&&std::abs(point.y-decimal(xy[1]))<1e-8)<<row[0]<<e<<p<<point.x<<point.y;
                }
            }
            ++count;
        }
        expect(count==4_u);
    };
};
}
