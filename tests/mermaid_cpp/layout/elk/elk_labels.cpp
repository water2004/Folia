#include "support/folia_test.hpp"
import folia.mermaid.elk_labels;
import folia.mermaid.elk_ports;
import folia.mermaid.elk_placement;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
double decimal(std::string_view text) {
    double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK label fixture number");return value;
}
std::size_t number(std::string_view text){return static_cast<std::size_t>(decimal(text));}
void near(double actual,double expected,std::string_view name){expect(std::abs(actual-expected)<1e-9)<<name<<actual<<expected;}
void layers(const ElkLayeredGraph& g,std::string_view expected,std::string_view name) {
    const auto ls=split(expected,';');expect(fatal(ls.size()==g.layers.size()))<<name;
    for(std::size_t l=0;l<ls.size();++l){const auto ns=split(ls[l],',');expect(fatal(ns.size()==g.layers[l].size()))<<name;
        for(std::size_t n=0;n<ns.size();++n)expect(g.layers[l][n]==number(ns[n]))<<name<<l<<n;}
}
void point(ElkRoutingPoint p,std::string_view expected,std::string_view name) {
    const auto xy=split(expected,',');near(p.x,decimal(xy[0]),name);near(p.y,decimal(xy[1]),name);
}
suite elk_labels=[] {
    "mermaid_elk_inline_center_label_processors_match_original_geometry"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-labels.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==11_u));const auto name=row[0];
            ElkCycleGraph source{.node_count=number(row[1])};
            for(const auto pair:split(row[2],',')) {
                const auto ends=split(pair,':');const auto p=source.ports.size();
                source.ports.push_back({number(ends[0])});source.ports.push_back({number(ends[1])});source.edges.push_back({p,p+1});
            }
            std::vector<ElkInlineCenterLabel> labels;
            for(const auto item:split(row[3],',')) {
                const auto v=split(item,':');labels.push_back({labels.size(),number(v[0]),decimal(v[1]),decimal(v[2])});
            }
            const auto direction=static_cast<ElkLabelDirection>(number(row[4]));
            auto prepared=elk_prepare_inline_labels(source,labels,direction,2,3);expect(fatal(prepared.has_value()))<<name;
            auto& g=prepared->graph;const auto inserted=split(row[5],';');expect(fatal(inserted.size()==prepared->dummies.size()));
            for(std::size_t i=0;i<inserted.size();++i) {
                const auto v=split(inserted[i],',');const auto n=prepared->dummies[i].node;expect(n==number(v[0]))<<name;
                near(g.nodes[n].width,decimal(v[1]),name);near(g.nodes[n].height,decimal(v[2]),name);
            }
            layers(g,row[6],name);elk_assign_flat_port_sides(g);elk_split_long_edges(g);elk_switch_label_dummies(g);layers(g,row[7],name);
            elk_place_inline_label_ports(g,prepared->dummies);
            for(std::size_t n=0;n<g.nodes.size();++n) {
                auto& node=g.nodes[n];if(n<source.node_count){node.width=40+n*5;node.height=24+n;}
                for(const auto p:node.ports){auto& port=g.ports[p];port.x=port.side==ElkPortSide::east?node.width:0;if(n<source.node_count)port.y=node.height/2;}
            }
            const auto placed=elk_place_flat_nodes(g,{},false);expect(fatal(placed.has_value()))<<name;
            const auto geometry=split(row[8],';');expect(fatal(geometry.size()==g.nodes.size()));
            ElkOrthogonalRoutes routed;routed.node_x.resize(g.nodes.size());routed.bends.resize(g.edges.size());
            for(std::size_t n=0;n<g.nodes.size();++n) {
                const auto v=split(geometry[n],',');const auto& node=g.nodes[n];
                expect(static_cast<std::size_t>(node.kind)==number(v[0]))<<name;
                near(node.width,decimal(v[1]),name);near(node.height,decimal(v[2]),name);near((*placed)[n],decimal(v[3]),name);
                near(g.ports[node.ports.front()].y,decimal(v[4]),name);routed.node_x[n]=node.layer*100;
            }
            const auto restored_labels=elk_restore_inline_labels(g,prepared->dummies,routed.node_x,*placed,direction,3);
            const auto expected_labels=split(row[9],';');expect(fatal(restored_labels.size()==expected_labels.size()));
            for(std::size_t i=0;i<restored_labels.size();++i) {
                const auto& label=restored_labels[i];expect(label.id==i&&label.original_edge==labels[i].original_edge)<<name;
                point({label.x,label.y},expected_labels[i],name);
            }
            const auto anchor=[&](std::size_t p){const auto& port=g.ports[p];return ElkRoutingPoint{routed.node_x[port.node]+port.x,(*placed)[port.node]+port.y};};
            for(std::size_t e=0;e<g.edges.size();++e){const auto a=anchor(g.edges[e].source_port),b=anchor(g.edges[e].target_port);const auto x=(a.x+b.x)/2;routed.bends[e]={{x,a.y},{x,b.y}};}
            const auto routes=elk_restore_flat_routes(g,routed,*placed);expect(fatal(routes.has_value()))<<name;
            const auto expected_routes=split(row[10],';');expect(fatal(routes->size()==expected_routes.size()));
            for(std::size_t e=0;e<routes->size();++e) {
                const auto v=split(expected_routes[e],'|');const auto& route=(*routes)[e];
                expect(route.source_node==number(v[0])&&route.target_node==number(v[1]))<<name;
                point(route.start,v[2],name);point(route.end,v[3],name);const auto bends=split(v[4],':');expect(fatal(bends.size()==route.bends.size()));
                for(std::size_t p=0;p<bends.size();++p)point(route.bends[p],bends[p],name);
            }
            ++count;
        }
        expect(count==8_u);
    };
};
}

