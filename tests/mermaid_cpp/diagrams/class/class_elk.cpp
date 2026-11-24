#include "support/folia_test.hpp"
import folia.mermaid.class_elk;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
std::vector<std::string_view> fields(std::string_view s,char delimiter){std::vector<std::string_view> out;for(;;){const auto p=s.find(delimiter);out.push_back(s.substr(0,p));if(p==s.npos)break;s.remove_prefix(p+1);}return out;}
double value(std::string_view s){double v{};std::from_chars(s.data(),s.data()+s.size(),v);return v;}
std::u16string ascii(std::string_view s){return {s.begin(),s.end()};}
std::u16string decode(std::string_view s){std::u16string out;for(std::size_t i=0;i<s.size();i+=4){unsigned n{};std::from_chars(s.data()+i,s.data()+i+4,n,16);out.push_back(static_cast<char16_t>(n));}return out;}
void near(double actual,double expected,std::string_view context){expect(std::abs(actual-expected)<1e-4)<<context<<actual<<expected;}
void point(FlowchartPoint actual,std::string_view expected,std::string_view context){const auto xy=fields(expected,',');near(actual.x,value(xy[0]),context);near(actual.y,value(xy[1]),context);}
suite class_elk=[] {
 "mermaid_class_elk_five_original_measured_scenes"_test=[] {
  const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream file{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"class-elk.tsv"};expect(fatal(file.is_open()));
  std::string line;std::size_t count=0;
  while(std::getline(file,line)){
   if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;
   const auto row=fields(line,'\t');expect(fatal(row.size()==9_u));auto session=ConfigSession::create();expect(fatal(session.has_value()));
   ConfigValue site;site.kind=ConfigKind::mapping;site.mapping.emplace(u"theme",ConfigValue{.kind=ConfigKind::string,.string=u"default"});
   expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
   const auto doc=prepare_class(decode(row[1]),*session);expect(fatal(doc.has_value()))<<row[0];FlowchartInput graph;graph.direction=doc->direction;
   for(const auto& n:doc->namespaces)graph.nodes.push_back({.id=n.id,.label=n.label,.parent=n.parent,.group=true,.padding=16});
   for(const auto& n:doc->classes)graph.nodes.push_back({.id=n.id,.parent=n.parent});
   for(const auto& e:doc->relations)graph.edges.push_back({.start=e.from,.end=e.to,.label=e.title});
   std::vector<FlowchartNodeBox> boxes(graph.nodes.size());FlowchartMeasurements groups;groups.nodes.resize(boxes.size());
   const auto nodeRows=fields(row[2],';');
   for(auto item:nodeRows){const auto n=fields(item,',');const auto node=std::ranges::find(graph.nodes,ascii(n[0]),&FlowchartNode::id);expect(fatal(node!=graph.nodes.end()));const auto i=node-graph.nodes.begin();boxes[i]={value(n[3]),value(n[4])};groups.nodes[i].width=value(n[5]);groups.nodes[i].height=value(n[6]);}
   std::vector<ClassEdgeMeasurements> edges;
   for(auto item:fields(row[3],';')){const auto n=fields(item,',');ClassEdgeMeasurements m;m.label.width=value(n[0]);m.label.height=value(n[1]);m.start.width=value(n[2]);m.start.height=value(n[3]);m.end.width=value(n[4]);m.end.height=value(n[5]);edges.push_back(m);}
   auto scene=layout_measured_elk_class(graph,boxes,doc->relations,edges,groups);expect(fatal(scene.has_value()))<<row[0]<<(scene?"":scene.error());
   for(auto item:nodeRows){const auto n=fields(item,',');const auto node=std::ranges::find(scene->layout.nodes,ascii(n[0]),&FlowchartPlacedNode::id);expect(fatal(node!=scene->layout.nodes.end()));near(node->x,value(n[7]),row[0]);near(node->y,value(n[8]),row[0]);near(node->width,value(n[9]),row[0]);near(node->height,value(n[10]),row[0]);}
   const auto routes=fields(row[4],';'),centers=fields(row[5],';'),terminals=fields(row[6],';');expect(fatal(routes.size()==scene->layout.edges.size()));
   for(std::size_t i=0;i<routes.size();++i){const auto points=fields(routes[i],'|');const auto& edge=scene->layout.edges[i];expect(fatal(points.size()==edge.points.size()))<<row[0]<<i;
    for(std::size_t p=0;p<points.size();++p)point(edge.points[p],points[p],row[0]);
    if(centers[i]!="-"){expect(fatal(edge.label.has_value()));point(*edge.label,centers[i],row[0]);}
    for(const auto head:{false,true}){const auto& actual=head?scene->terminals[i].end:scene->terminals[i].start;const auto expected=terminals[2*i+head];expect(actual.has_value()==(expected!="-"));if(expected!="-"){expect(fatal(actual.has_value()));point(*actual,expected,row[0]);}}
   }
   std::vector<std::pair<FlowchartPoint,FlowchartPoint>> endpoints;
   for(const auto& edge:scene->layout.edges)endpoints.emplace_back(edge.points.front(),edge.points.back());
   finish_class_elk_scene(*scene,graph,edges,endpoints);
   const auto finalTerminals=fields(row[7],';'),finalCenters=fields(row[8],';');
   for(std::size_t i=0;i<scene->layout.edges.size();++i){
    for(bool head:{false,true}){const auto& actual=head?scene->terminals[i].end:scene->terminals[i].start;const auto expected=finalTerminals[2*i+head];if(expected!="-"){expect(fatal(actual.has_value()));point(*actual,expected,row[0]);}}
    if(finalCenters[i]!="-"){expect(fatal(scene->layout.edges[i].label.has_value()));point(*scene->layout.edges[i].label,finalCenters[i],row[0]);}
   }++count;
  }expect(count==5_u);
 };
};
}
