#include "support/folia_test.hpp"
import folia.mermaid.flowchart_elk;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
std::vector<std::string_view> fields(std::string_view s,char delimiter){std::vector<std::string_view> out;for(;;){auto p=s.find(delimiter);out.push_back(s.substr(0,p));if(p==s.npos)break;s.remove_prefix(p+1);}return out;}
double value(std::string_view s){double v{};std::from_chars(s.data(),s.data()+s.size(),v);return v;}
std::u16string ascii(std::string_view s){return {s.begin(),s.end()};}
std::u16string decode(std::string_view s){std::u16string out;for(std::size_t i=0;i<s.size();i+=4){unsigned n{};std::from_chars(s.data()+i,s.data()+i+4,n,16);out.push_back(static_cast<char16_t>(n));}return out;}
void near(double actual,double expected,std::string_view context){expect(std::abs(actual-expected)<1e-4)<<context<<actual<<expected;}
suite flowchart_elk=[] {
 "mermaid_flowchart_elk_measured_six_original_scenes"_test=[] {
  const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream file{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"flowchart-elk.tsv"};expect(fatal(file.is_open()));
  std::string line;std::size_t count=0;
  while(std::getline(file,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;
   const auto row=fields(line,'\t');expect(fatal(row.size()==6_u));auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;site.mapping.emplace(u"theme",ConfigValue{.kind=ConfigKind::string,.string=u"default"});expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
   const auto graph=prepare_flowchart(decode(row[1]),*session);expect(fatal(graph.has_value()))<<row[0];
   std::vector<FlowchartNodeBox> boxes(graph->nodes.size());std::vector<FlowchartEdgeLabelSize> labels;FlowchartMeasurements measured;measured.nodes.resize(boxes.size());
   const auto nodeRows=fields(row[2],';');
   for(auto item:nodeRows){const auto n=fields(item,',');const auto node=std::ranges::find(graph->nodes,ascii(n[0]),&FlowchartNode::id);expect(fatal(node!=graph->nodes.end()));const auto i=node-graph->nodes.begin();boxes[i]={value(n[1]),value(n[2])};measured.nodes[i].width=value(n[3]);measured.nodes[i].height=value(n[4]);}
   for(auto item:fields(row[3],';')){const auto n=fields(item,',');labels.push_back({value(n[0]),value(n[1])});}
   const auto scene=layout_measured_elk_flowchart(*graph,boxes,labels,measured,{});expect(fatal(scene.has_value()))<<row[0]<<(scene?"":scene.error());
   for(auto item:nodeRows){const auto n=fields(item,',');const auto node=std::ranges::find(scene->layout.nodes,ascii(n[0]),&FlowchartPlacedNode::id);expect(fatal(node!=scene->layout.nodes.end()));near(node->x,value(n[5]),row[0]);near(node->y,value(n[6]),row[0]);near(node->width,value(n[7]),row[0]);near(node->height,value(n[8]),row[0]);}
   const auto routes=fields(row[4],';'),centers=fields(row[5],';');expect(fatal(routes.size()==scene->layout.edges.size()));
   for(std::size_t i=0;i<routes.size();++i){const auto points=fields(routes[i],'|');const auto& edge=scene->layout.edges[i];expect(fatal(points.size()==edge.points.size()))<<row[0]<<i;
    for(std::size_t p=0;p<points.size();++p){const auto xy=fields(points[p],',');near(edge.points[p].x,value(xy[0]),row[0]);near(edge.points[p].y,value(xy[1]),row[0]);}
    if(centers[i]!="-"){expect(fatal(edge.label.has_value()));const auto xy=fields(centers[i],',');near(edge.label->x,value(xy[0]),row[0]);near(edge.label->y,value(xy[1]),row[0]);}
   }++count;
  }expect(count==6_u);
 };
};
}
