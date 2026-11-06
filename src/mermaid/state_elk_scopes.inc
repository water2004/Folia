// Mermaid 12.2.0 elk/render.ts hierarchy policy and elkjs 0.9.3
// ComponentsProcessor / SimpleRowGraphPlacer (EPL-2.0).
// A group with its own direction starts a new layout scope unless an edge
// crossing the hierarchy forces the affected ancestor chain back into ELK.
std::expected<FlowchartScene,std::string> layout_state_elk_scopes(const FlowchartInput& graph,
 std::span<const FlowchartNodeBox> boxes,std::span<const FlowchartEdgeLabelSize> edge_boxes,
 const FlowchartMeasurements& labels,const std::unordered_map<std::u16string,std::size_t>& by_id) {
 const auto root=graph.nodes.size();
 std::vector<std::size_t> parents(root,root);
 std::vector<bool> separate(root);
 for(std::size_t i=0;i<root;++i){
  const auto& n=graph.nodes[i];
  if(!n.parent.empty())parents[i]=by_id.at(n.parent);
  separate[i]=n.group&&!n.direction.empty();
 }
 for(const auto& edge:graph.edges){
  const auto source=by_id.at(edge.start),target=by_id.at(edge.end);
  if(parents[source]==parents[target])continue;
  std::unordered_set<std::size_t> ancestors;
  for(auto i=source;i!=root;i=parents[i])ancestors.insert(i);
  auto common=target;
  while(common!=root&&!ancestors.contains(common))common=parents[common];
  for(const auto endpoint:{source,target})for(auto i=endpoint;i!=root;i=parents[i]){
   separate[i]=false;
   if(i==common)break;
  }
 }
 std::vector<std::vector<std::size_t>> nodes(root+1),edges(root+1);
 std::vector<std::size_t> scope_of(root,root);
 for(std::size_t i=0;i<root;++i){
  auto scope=parents[i];
  while(scope!=root&&!separate[scope])scope=parents[scope];
  nodes[scope].push_back(i);scope_of[i]=scope;
 }
 for(std::size_t i=0;i<graph.edges.size();++i)edges[scope_of[by_id.at(graph.edges[i].start)]].push_back(i);

 FlowchartScene scene;
 scene.layout.nodes.resize(root);scene.layout.edges.resize(graph.edges.size());
 struct ScopeFrame { double width=0,height=0,title=0; };
 std::vector<ScopeFrame> frames(root);
 const auto shift=[&](auto&& self,std::size_t scope,double dx,double dy)->void{
  for(const auto i:nodes[scope]){
   auto& node=scene.layout.nodes[i];node.x+=dx;node.y+=dy;
   if(separate[i])self(self,i,dx,dy);
  }
  for(const auto i:edges[scope]){
   auto& edge=scene.layout.edges[i];
   for(auto& p:edge.points){p.x+=dx;p.y+=dy;}
   if(edge.label){edge.label->x+=dx;edge.label->y+=dy;}
  }
 };
 const auto solve=[&](auto&& self,std::size_t scope)->std::expected<ScopeFrame,std::string>{
  if(nodes[scope].empty())return ScopeFrame{};
  FlowchartInput local;
  local.direction=scope==root?graph.direction:graph.nodes[scope].direction;
  std::vector<FlowchartNodeBox> local_boxes;
  std::vector<FlowchartEdgeLabelSize> local_edge_boxes;
  FlowchartMeasurements local_labels;
  for(const auto i:nodes[scope]){
   auto n=graph.nodes[i];
   auto box=boxes[i];
   if(parents[i]==scope)n.parent.clear();
   if(separate[i]){
    auto child=self(self,i);
    if(!child)return std::unexpected(child.error());
    frames[i]=*child;
    n.group=false;
    box={child->width,child->height};
   }
   local.nodes.push_back(std::move(n));local_boxes.push_back(box);
   local_labels.nodes.push_back(labels.nodes[i]);
  }
  for(const auto i:edges[scope]){
   local.edges.push_back(graph.edges[i]);local_edge_boxes.push_back(edge_boxes[i]);
  }
  struct Component {
   std::vector<std::size_t> nodes,edges;
   FlowchartScene scene;
  };
  std::vector<Component> components;
  // Included groups are solved together. Separate layered scopes use ELK's
  // ordinary weak-component pass, then SimpleRowGraphPlacer.
  if(scope==root||std::ranges::any_of(local.nodes,&FlowchartNode::group)){
   auto& component=components.emplace_back();
   for(std::size_t i=0;i<local.nodes.size();++i)component.nodes.push_back(i);
   for(std::size_t i=0;i<local.edges.size();++i)component.edges.push_back(i);
  }else{
   std::unordered_map<std::u16string,std::size_t> local_ids;
   std::vector<std::size_t> links(local.nodes.size());
   for(std::size_t i=0;i<local.nodes.size();++i){local_ids.emplace(local.nodes[i].id,i);links[i]=i;}
   const auto representative=[&](std::size_t i){
    while(i!=links[i]){links[i]=links[links[i]];i=links[i];}
    return i;
   };
   for(const auto& edge:local.edges)
    links[representative(local_ids.at(edge.start))]=representative(local_ids.at(edge.end));
   std::unordered_map<std::size_t,std::size_t> component_by_root;
   std::vector<std::size_t> component_by_node(local.nodes.size());
   for(std::size_t i=0;i<local.nodes.size();++i){
    const auto [component,inserted]=component_by_root.try_emplace(representative(i),components.size());
    if(inserted)components.emplace_back();
    component_by_node[i]=component->second;
    components[component->second].nodes.push_back(i);
   }
   for(std::size_t i=0;i<local.edges.size();++i)
    components[component_by_node[local_ids.at(local.edges[i].start)]].edges.push_back(i);
  }

  for(auto& component:components){
   if(components.size()==1){
    auto placed=scope==root?layout_measured_elk_scene(local,local_boxes,local_edge_boxes,local_labels,{}):
     layout_measured_elk_subgraph(local,local_boxes,local_edge_boxes,local_labels);
    if(!placed)return std::unexpected(placed.error());
    component.scene=std::move(*placed);
    break;
   }
   FlowchartInput subgraph{.direction=local.direction};
   std::vector<FlowchartNodeBox> sub_boxes;
   std::vector<FlowchartEdgeLabelSize> sub_edge_boxes;
   FlowchartMeasurements sub_labels;
   for(const auto i:component.nodes){
    subgraph.nodes.push_back(local.nodes[i]);sub_boxes.push_back(local_boxes[i]);
    sub_labels.nodes.push_back(local_labels.nodes[i]);
   }
   for(const auto i:component.edges){
    subgraph.edges.push_back(local.edges[i]);sub_edge_boxes.push_back(local_edge_boxes[i]);
   }
   auto placed=layout_measured_elk_subgraph(subgraph,sub_boxes,sub_edge_boxes,sub_labels);
   if(!placed)return std::unexpected(placed.error());
   component.scene=std::move(*placed);
  }

  constexpr double padding=24,component_spacing=24;
  const auto width=[](const Component& c){return c.scene.layout.width-2*padding;};
  const auto height=[](const Component& c){return c.scene.layout.height-2*padding;};
  const auto area=[&](const Component& c){return width(c)*height(c);};
  double max_row_width=0,total_area=0;
  if(components.size()>1){
   // With no model order override or explicit priorities, ELK uses stable
   // ascending area. The threshold uses its default aspect ratio of 1.6.
   std::ranges::stable_sort(components,std::ranges::less{},area);
   for(const auto& c:components){max_row_width=std::max(max_row_width,width(c));total_area+=area(c);}
   max_row_width=std::max(max_row_width,std::sqrt(total_area)*static_cast<double>(1.6f));
  }
  double x=0,y=0,row_height=0,broadest=component_spacing;
  for(auto& component:components){
   if(components.size()>1&&x+width(component)>max_row_width){
    x=0;y+=row_height+component_spacing;row_height=0;
   }
   for(std::size_t n=0;n<component.nodes.size();++n){
    const auto i=nodes[scope][component.nodes[n]];
    auto& node=scene.layout.nodes[i];
    node=std::move(component.scene.layout.nodes[n]);node.x+=x;node.y+=y;
    if(separate[i])shift(shift,i,node.x-node.width/2,node.y-node.height/2+frames[i].title);
   }
   for(std::size_t e=0;e<component.edges.size();++e){
    auto& edge=scene.layout.edges[edges[scope][component.edges[e]]];
    edge=std::move(component.scene.layout.edges[e]);
    for(auto& point:edge.points){point.x+=x;point.y+=y;}
    if(edge.label){edge.label->x+=x;edge.label->y+=y;}
   }
   broadest=std::max(broadest,x+width(component));row_height=std::max(row_height,height(component));
   x+=width(component)+component_spacing;
  }
  ScopeFrame frame;
  if(components.size()==1){
   frame.width=components.front().scene.layout.width;frame.height=components.front().scene.layout.height;
  }else{
   frame.width=broadest+2*padding;frame.height=y+row_height+2*padding;
  }
  if(scope!=root){
   const auto& group=graph.nodes[scope];
   if(!group.label.empty()){
    frame.title=5+std::max(0.,labels.nodes[scope].height-2);
    frame.width=std::max(frame.width,labels.nodes[scope].width+group.padding);
    frame.height+=frame.title;
   }
  }
  return frame;
 };
 auto frame=solve(solve,root);
 if(!frame)return std::unexpected(frame.error());
 scene.layout.width=frame->width;scene.layout.height=frame->height;
 return scene;
}
