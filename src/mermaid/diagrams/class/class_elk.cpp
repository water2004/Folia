module folia.mermaid.class_elk;
import std;
namespace folia::mermaid {
std::expected<ClassElkScene,std::string> layout_measured_elk_class(
    const FlowchartInput& graph,std::span<const FlowchartNodeBox> boxes,
    std::span<const ClassRelation> relations,std::span<const ClassEdgeMeasurements> measured,
    const FlowchartMeasurements& groups) {
    if(relations.size()!=graph.edges.size()||measured.size()!=relations.size())
        return std::unexpected("Class ELK needs measured relationships in graph edge order");
    std::vector<FlowchartEdgeLabelSize> centers;std::vector<ElkEndLabel> terminals;
    for(std::size_t i=0;i<relations.size();++i){
        const auto& edge=relations[i];const auto& sizes=measured[i];centers.push_back({sizes.label.width,sizes.label.height});
        const auto add=[&](bool head,const std::u16string& text,const ClassTextBox& size,ClassMarker marker){
            if(text.empty()||text==u"none")return;
            const double padding=marker==ClassMarker::none?0:6;
            terminals.push_back({2*i+static_cast<std::size_t>(head),i,head,size.width+2*padding,size.height+2*padding});
        };
        add(false,edge.start_label,sizes.start,edge.start);add(true,edge.end_label,sizes.end,edge.end);
    }
    auto placed=layout_measured_elk_scene(graph,boxes,centers,groups,terminals);
    if(!placed)return std::unexpected(placed.error());
    ClassElkScene scene;static_cast<FlowchartScene&>(scene)=std::move(static_cast<FlowchartScene&>(*placed));scene.terminals.resize(relations.size());
    for(const auto& label:placed->end_labels){const FlowchartPoint center{label.x+label.width/2,label.y+label.height/2};
        if(label.id%2)scene.terminals[label.original_edge].end=center;else scene.terminals[label.original_edge].start=center;
    }
    return scene;
}
namespace {
struct Box {double x1,y1,x2,y2;};
Box label_box(FlowchartPoint p,double w,double h){return {p.x-w/2,p.y-h/2,p.x+w/2,p.y+h/2};}
bool overlaps(Box a,Box b){return a.x1<b.x2&&b.x1<a.x2&&a.y1<b.y2&&b.y1<a.y2;}
bool segment_hits(FlowchartPoint a,FlowchartPoint b,Box box){
 double t0=0,t1=1;const double dx=b.x-a.x,dy=b.y-a.y;
 for(const auto [p,q]:{std::pair{-dx,a.x-box.x1},std::pair{dx,box.x2-a.x},std::pair{-dy,a.y-box.y1},std::pair{dy,box.y2-a.y}}){
  if(p==0){if(q<0)return false;}else if(p<0)t0=std::max(t0,q/p);else t1=std::min(t1,q/p);
 }return t0<t1;
}
}
void finish_class_elk_scene(ClassElkScene& scene,const FlowchartInput& graph,
 std::span<const ClassEdgeMeasurements> measured,
 std::span<const std::pair<FlowchartPoint,FlowchartPoint>> raw_endpoints){
 auto& edges=scene.layout.edges;
 for(std::size_t i=0;i<edges.size();++i){if(edges[i].points.empty())continue;
  for(bool head:{false,true}){auto& p=head?scene.terminals[i].end:scene.terminals[i].start;if(!p)continue;
   const auto now=head?edges[i].points.back():edges[i].points.front(),old=head?raw_endpoints[i].second:raw_endpoints[i].first;
   p->x+=now.x-old.x;p->y+=now.y-old.y;
  }
 }
 std::vector<Box> node_boxes,center_boxes;std::vector<std::pair<FlowchartPoint,FlowchartPoint>> frames;
 for(std::size_t i=0;i<scene.layout.nodes.size();++i){const auto& n=scene.layout.nodes[i];const auto b=label_box({n.x,n.y},n.width,n.height);
  if(!graph.nodes[i].group)node_boxes.push_back(b);else {frames.push_back({{b.x1,b.y1},{b.x2,b.y1}});frames.push_back({{b.x2,b.y1},{b.x2,b.y2}});frames.push_back({{b.x2,b.y2},{b.x1,b.y2}});frames.push_back({{b.x1,b.y2},{b.x1,b.y1}});}
 }
 for(std::size_t i=0;i<edges.size();++i)if(edges[i].label&&!graph.edges[i].label.empty()&&measured[i].label.width&&measured[i].label.height)center_boxes.push_back(label_box(*edges[i].label,measured[i].label.width,measured[i].label.height));
 const auto blocked=[&](Box box,std::size_t edge,bool head,std::size_t segment){
  for(auto b:node_boxes)if(overlaps(box,b))return true;for(auto b:center_boxes)if(overlaps(box,b))return true;
  for(auto [a,b]:frames)if(segment_hits(a,b,box))return true;
  for(std::size_t j=0;j<edges.size();++j){const auto& points=edges[j].points;
   for(std::size_t p=0;p+1<points.size();++p)if(!(j==edge&&p==segment)&&segment_hits(points[p],points[p+1],box))return true;
   for(bool other_head:{false,true}){if(j==edge&&head==other_head)continue;const auto& other=other_head?scene.terminals[j].end:scene.terminals[j].start;const auto& size=other_head?measured[j].end:measured[j].start;if(other&&overlaps(box,label_box(*other,size.width,size.height)))return true;}
  }return false;
 };
 // Original putTerminalLabelsOnTheirSide. ClassDB supplies startRight/endLeft.
 for(std::size_t i=0;i<edges.size();++i)for(bool head:{false,true}){
  auto& center=head?scene.terminals[i].end:scene.terminals[i].start;const auto& size=head?measured[i].end:measured[i].start;
  const auto& points=edges[i].points;if(!center||points.size()<2)continue;const auto segment=head?points.size()-2:0;
  const auto from=points[segment],to=points[segment+1];const double length=std::hypot(to.x-from.x,to.y-from.y);if(!length)continue;
  const FlowchartPoint normal{-(to.y-from.y)/length,(to.x-from.x)/length};const double offset=(center->x-from.x)*normal.x+(center->y-from.y)*normal.y;
  if(offset==0||(offset>0)==!head)continue;
  const FlowchartPoint mirrored{center->x-2*offset*normal.x,center->y-2*offset*normal.y};
  if(!blocked(label_box(mirrored,size.width,size.height),i,head,segment))center=mirrored;
 }
 // Original separateOppositeEdgeLabels, after terminal collision decisions.
 std::map<std::pair<std::u16string,std::u16string>,std::vector<std::size_t>> pairs;
 for(std::size_t i=0;i<edges.size();++i)if(edges[i].label&&!graph.edges[i].label.empty()&&measured[i].label.width){auto a=graph.edges[i].start,b=graph.edges[i].end;if(a>b)std::swap(a,b);pairs[{a,b}].push_back(i);}
 for(const auto& [key,pair]:pairs)if(pair.size()==2){const auto i=pair[0],j=pair[1];auto& a=*edges[i].label;auto& b=*edges[j].label;
  if(std::abs(a.y-b.y)>std::max(measured[i].label.height,measured[j].label.height))continue;
  const double delta=b.x-a.x,deficit=(measured[i].label.width+measured[j].label.width)/2+4-std::abs(delta);if(deficit<=0)continue;
  const double sign=delta<0?-1:1;a.x-=sign*deficit/2;b.x+=sign*deficit/2;
 }
 scene.min_x=scene.min_y=std::numeric_limits<double>::infinity();scene.max_x=scene.max_y=-scene.min_x;
 const auto include=[&](Box b){scene.min_x=std::min(scene.min_x,b.x1);scene.min_y=std::min(scene.min_y,b.y1);scene.max_x=std::max(scene.max_x,b.x2);scene.max_y=std::max(scene.max_y,b.y2);};
 for(const auto& n:scene.layout.nodes)include(label_box({n.x,n.y},n.width,n.height));
 for(std::size_t i=0;i<edges.size();++i){for(auto p:edges[i].points)include(label_box(p,0,0));if(edges[i].label)include(label_box(*edges[i].label,measured[i].label.width,measured[i].label.height));
  if(scene.terminals[i].start)include(label_box(*scene.terminals[i].start,measured[i].start.width,measured[i].start.height));if(scene.terminals[i].end)include(label_box(*scene.terminals[i].end,measured[i].end.width,measured[i].end.height));
 }
}
}
