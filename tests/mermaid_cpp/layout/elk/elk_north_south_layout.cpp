#include "support/folia_test.hpp"
import folia.mermaid.elk_order;
import folia.mermaid.elk_north_south_layout;

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
double decimal(std::string_view text){double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid NS geometry value");return value;}
void near(double a,double b,std::string_view name){expect(std::abs(a-b)<1e-8)<<name<<a<<b;}
void point(ElkRoutingPoint p,std::string_view text,std::string_view name){const auto v=split(text,',');near(p.x,decimal(v[0]),name);near(p.y,decimal(v[1]),name);}
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
suite elk_north_south_layout=[] {
    "mermaid_elk_north_south_placement_and_routing_match_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-north-south-layout.tsv"};
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
            expect(fatal(elk_sort_north_south_ports(g,*prepared).has_value()))<<name;
            ElkRandom random(1);const auto ordered=elk_order_north_south_graph(g,*prepared,random);expect(fatal(ordered.has_value()))<<name;
            const auto originals=split(row[1],',').size();
            for(std::size_t n=0;n<originals;++n){g.nodes[n].width=50+5*n;g.nodes[n].height=38+2*n;g.nodes[n].fixed_port_positions=true;}
            for(std::size_t p=0;p<g.edges.size()*2;++p) {
                auto& port=g.ports[p];const auto& node=g.nodes[port.node];
                port.x=port.side==ElkPortSide::east?node.width:port.side==ElkPortSide::west?0:10+3*p;
                port.y=port.side==ElkPortSide::north?-5:port.side==ElkPortSide::south?node.height+5:node.height/2;
            }
            const auto result=elk_layout_north_south_graph(g,*prepared,random);expect(fatal(result.has_value()))<<name;
            const auto positions=split(row[4],';');expect(fatal(positions.size()==g.nodes.size()));
            for(std::size_t n=0;n<positions.size();++n)point({result->routing.node_x[n],result->node_y[n]},positions[n],name);
            const auto bounds=split(row[5],',');near(result->routing.width,decimal(bounds[0]),name);near(result->height,decimal(bounds[1]),name);near(result->offset_y,decimal(bounds[2]),name);
            const auto routes=split(row[6],';');expect(fatal(routes.size()==result->routes.size()));
            for(std::size_t e=0;e<routes.size();++e){const auto parts=split(routes[e],'|');const auto& route=result->routes[e];point(route.start,parts[0],name);point(route.end,parts[1],name);const auto bends=split(parts[2],':');expect(fatal(bends.size()==route.bends.size()))<<name;for(std::size_t p=0;p<bends.size();++p)point(route.bends[p],bends[p],name);}
            near(random.next_double(),decimal(row[7]),name);
            ++count;
        }
        expect(count==4_u);
    };
};
}
