#include "support/folia_test.hpp"
import folia.mermaid.elk_layout;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> split(std::string_view text,char delimiter) {
    std::vector<std::string_view> result;if(text=="-")return result;
    for(;;){const auto end=text.find(delimiter);result.push_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return result;
}
double decimal(std::string_view text) {
    double value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{})throw std::runtime_error("Invalid ELK layout fixture number");return value;
}
std::size_t number(std::string_view text){return static_cast<std::size_t>(decimal(text));}
void near(double a,double b,std::string_view name){expect(std::abs(a-b)<1e-8)<<name<<a<<b;}
void point(ElkRoutingPoint p,std::string_view text,std::string_view name){const auto v=split(text,',');near(p.x,decimal(v[0]),name);near(p.y,decimal(v[1]),name);}
suite elk_layout=[] {
    "mermaid_elk_complete_labelled_flat_layout_matches_original_adapter"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"elk-layout.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto row=split(line,'\t');expect(fatal(row.size()==11_u));const auto name=row[0];ElkFlatLayoutInput graph;
            for(const auto item:split(row[1],',')){const auto v=split(item,':');graph.nodes.push_back({decimal(v[0]),decimal(v[1])});}
            for(const auto item:split(row[2],',')){const auto v=split(item,':');graph.edges.push_back({number(v[0]),number(v[1])});}
            for(const auto item:split(row[3],',')){const auto v=split(item,':');graph.center_labels.push_back({graph.center_labels.size(),number(v[0]),decimal(v[1]),decimal(v[2])});}
            for(const auto item:split(row[4],',')){const auto v=split(item,':');graph.end_labels.push_back({graph.end_labels.size(),number(v[0]),number(v[1])!=0,decimal(v[2]),decimal(v[3])});}
            ElkFlatLayoutOptions options;options.direction=static_cast<ElkLayoutDirection>(number(row[5]));
            const auto result=elk_layout_flat(graph,options);expect(fatal(result.has_value()))<<name;
            const auto bounds=split(row[6],',');near(result->width,decimal(bounds[0]),name);near(result->height,decimal(bounds[1]),name);
            const auto nodes=split(row[7],';');expect(fatal(nodes.size()==result->nodes.size()));
            for(std::size_t n=0;n<nodes.size();++n){const auto v=split(nodes[n],',');const auto& box=result->nodes[n];near(box.x,decimal(v[0]),name);near(box.y,decimal(v[1]),name);near(box.width,decimal(v[2]),name);near(box.height,decimal(v[3]),name);}
            for(const auto center:{true,false}) {
                const auto& labels=center?result->center_labels:result->end_labels;const auto expected=split(row[center?8:9],';');expect(fatal(labels.size()==expected.size()));
                for(std::size_t i=0;i<labels.size();++i)point({labels[i].x,labels[i].y},expected[i],name);
            }
            const auto routes=split(row[10],';');expect(fatal(routes.size()==result->routes.size()));
            for(std::size_t e=0;e<routes.size();++e){const auto v=split(routes[e],'|');const auto& route=result->routes[e];
                expect(route.original_edge==e&&route.source_node==graph.edges[e].source&&route.target_node==graph.edges[e].target)<<name;
                point(route.start,v[0],name);point(route.end,v[1],name);const auto bends=split(v[2],':');expect(fatal(bends.size()==route.bends.size()))<<name;
                for(std::size_t p=0;p<bends.size();++p)point(route.bends[p],bends[p],name);}
            ++count;
        }
        expect(count==8_u);
    };
};
}
