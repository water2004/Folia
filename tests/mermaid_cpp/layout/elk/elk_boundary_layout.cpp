#include "support/folia_test.hpp"
import folia.mermaid.elk_boundary_layout;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
double decimal(std::string_view text) {
    double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK boundary layout fixture number");return value;
}
std::size_t number(std::string_view text){return static_cast<std::size_t>(decimal(text));}
void near(double a,double b,std::string_view name){expect(std::abs(a-b)<1e-8)<<name<<a<<b;}
void point(ElkRoutingPoint p,std::string_view text,std::string_view name){const auto v=split(text,',');near(p.x,decimal(v[0]),name);near(p.y,decimal(v[1]),name);}
suite elk_boundary_layout=[] {
    "mermaid_elk_local_boundary_layout_matches_original_geometry"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-boundary-layout.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==8_u));const auto name=row[0];
            ElkCycleGraph graph;std::vector<ElkExternalLayerRole> roles;std::vector<ElkMeasuredNode> measured;
            for(const auto item:split(row[1],',')){const auto n=roles.size();roles.push_back(static_cast<ElkExternalLayerRole>(number(item)));measured.push_back({50.0+n*5,38.0+n*2});}graph.node_count=roles.size();
            for(const auto item:split(row[2],',')){const auto v=split(item,':');const auto p=graph.ports.size();graph.ports.push_back({number(v[0])});graph.ports.push_back({number(v[1])});graph.edges.push_back({p,p+1});}
            const auto result=elk_layout_boundary_graph(graph,roles,measured);expect(fatal(result.has_value()))<<name;
            const auto layers=split(row[3],';');expect(fatal(layers.size()==result->graph.layers.size()))<<name;
            for(std::size_t l=0;l<layers.size();++l){const auto nodes=split(layers[l],',');expect(fatal(nodes.size()==result->graph.layers[l].size()));for(std::size_t n=0;n<nodes.size();++n)expect(result->graph.layers[l][n]==number(nodes[n]))<<name;}
            const auto positions=split(row[4],';');expect(fatal(positions.size()==result->node_y.size()));
            for(std::size_t n=0;n<positions.size();++n)point({result->routing.node_x[n],result->node_y[n]},positions[n],name);
            const auto size=split(row[5],',');near(result->content.width,decimal(size[0]),name);near(result->content.height,decimal(size[1]),name);near(result->content.offset.y,decimal(size[2]),name);
            const auto ports=split(row[6],';');expect(fatal(ports.size()==result->boundary_ports.size()));
            for(std::size_t n=0;n<ports.size();++n){expect(result->boundary_ports[n].has_value()==(ports[n]!="-"))<<name;if(ports[n]!="-"&&result->boundary_ports[n])point(*result->boundary_ports[n],ports[n],name);}
            const auto paths=split(row[7],';');expect(fatal(paths.size()==result->routes.size()));
            for(std::size_t e=0;e<paths.size();++e){const auto v=split(paths[e],'|');const auto& route=result->routes[e];point(route.start,v[0],name);point(route.end,v[1],name);const auto bends=split(v[2],':');expect(fatal(bends.size()==route.bends.size()))<<name;for(std::size_t p=0;p<bends.size();++p)point(route.bends[p],bends[p],name);}
            ++count;
        }
        expect(count==4_u);
    };
};
}
