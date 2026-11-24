#include "support/folia_test.hpp"
import folia.mermaid.elk_compound_restore;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
double decimal(std::string_view text) {
    double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK compound restore fixture number");return value;
}
std::size_t number(std::string_view text){return static_cast<std::size_t>(decimal(text));}
void near(double a,double b,std::string_view name){expect(std::abs(a-b)<1e-9)<<name<<a<<b;}
void point(ElkRoutingPoint p,std::string_view text,std::string_view name){const auto v=split(text,',');near(p.x,decimal(v[0]),name);near(p.y,decimal(v[1]),name);}
suite elk_compound_restore=[] {
    "mermaid_elk_compound_paths_and_labels_match_original_coordinate_systems"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-compound-restore.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==6_u));const auto name=row[0];ElkCompoundInput graph;
            for(const auto p:split(row[1],',')){const auto n=decimal(p);graph.nodes.push_back({.parent=n<0?std::nullopt:std::optional{static_cast<std::size_t>(n)}});}
            for(const auto g:split(row[2],','))graph.nodes[number(g)].group=true;
            for(const auto item:split(row[3],',')){const auto v=split(item,':');graph.edges.push_back({number(v[0]),number(v[1])});}
            const auto prepared=elk_prepare_compound_graph(graph);expect(fatal(prepared.has_value()));const auto& g=*prepared;ElkCompoundGeometry geometry;
            for(std::size_t i=0;i<g.graphs.size();++i)geometry.graphs.push_back({.padding_left=10.0+i,.padding_top=20.0+i,.offset={i*2.0,-static_cast<double>(i)}});
            for(std::size_t i=0;i<g.nodes.size();++i)geometry.node_positions.push_back({i*30.0,i*20.0});
            for(std::size_t i=0;i<g.ports.size();++i)geometry.port_positions.push_back({i*3.0,i*2.0});
            geometry.segments.resize(g.segments.size());
            for(std::size_t i=0;i<g.segments.size();++i){const auto& s=g.segments[i];if(s.original_edge%2==0)geometry.segments[i].bends={{100.0+s.source_port,200.0+s.target_port},{120.0+s.source_port,220.0+s.target_port}};}
            for(std::size_t e=0;e<g.edges.size();++e){const auto& chain=g.edges[e];const std::array owners{chain.center_segment,chain.tail_segment,chain.head_segment};
                for(std::size_t j=0;j<3;++j){const auto id=e*3+j;geometry.segments[owners[j]].labels.push_back({id,e,7.0+id,11.0+id});}}
            const auto restored=elk_restore_compound_edges(g,geometry,number(row[4])!=0);const auto expected=split(row[5],';');expect(fatal(restored.size()==expected.size()));
            for(std::size_t e=0;e<restored.size();++e){const auto& route=restored[e];const auto v=split(expected[e],'|');expect(fatal(v.size()==8_u));
                expect(route.reference_graph==number(v[0])&&route.source_port==number(v[1])&&route.target_port==number(v[2]))<<name;
                point(route.start,v[3],name);point(route.end,v[4],name);point(route.target_offset,v[5],name);
                const auto bends=split(v[6],':');expect(fatal(bends.size()==route.bends.size()))<<name;for(std::size_t p=0;p<bends.size();++p)point(route.bends[p],bends[p],name);
                auto labels=route.labels;std::ranges::sort(labels,{},&ElkLabelPosition::id);const auto expected_labels=split(v[7],':');expect(fatal(labels.size()==expected_labels.size()));
                for(std::size_t i=0;i<labels.size();++i){const auto values=split(expected_labels[i],',');expect(labels[i].id==number(values[0]))<<name;near(labels[i].x,decimal(values[1]),name);near(labels[i].y,decimal(values[2]),name);}
            }
            ++count;
        }
        expect(count==4_u);
    };
};
}
