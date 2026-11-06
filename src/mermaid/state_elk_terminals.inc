/* Terminal route cleanup from Mermaid 12.2.0 elk/render.ts. */
struct ElkShiftedRun {
 FlowchartPoint old_start,old_end,new_start,new_end;
};
enum class ElkRunAxis { none,horizontal,vertical };
ElkRunAxis elk_run_axis(FlowchartPoint a,FlowchartPoint b) {
 const auto dx=std::abs(a.x-b.x),dy=std::abs(a.y-b.y);
 if(dx<=.01&&dy>.01)return ElkRunAxis::vertical;
 if(dy<=.01&&dx>.01)return ElkRunAxis::horizontal;
 return ElkRunAxis::none;
}
std::optional<ElkShiftedRun> straighten_elk_front(std::vector<FlowchartPoint>& points) {
 if(points.size()<5)return std::nullopt;
 const auto a=points[0],b=points[1],c=points[2],d=points[3];
 const auto axis=elk_run_axis(a,b);
 const auto horizontal=axis==ElkRunAxis::horizontal;
 if(axis==ElkRunAxis::none||elk_run_axis(c,d)!=axis||
    elk_run_axis(b,c)!=(horizontal?ElkRunAxis::vertical:ElkRunAxis::horizontal))
  return std::nullopt;
 if(std::hypot(b.x-a.x,b.y-a.y)>30)return std::nullopt;
 const auto jog=horizontal?std::abs(c.y-b.y):std::abs(c.x-b.x);
 if(jog<.01||jog>16)return std::nullopt;
 if(horizontal?(b.x>a.x)!=(d.x>c.x):(b.y>a.y)!=(d.y>c.y))return std::nullopt;
 std::size_t last=3;
 while(last+1<points.size()&&elk_run_axis(points[last],points[last+1])==axis)++last;
 if(last+1==points.size())return std::nullopt;
 const auto shift=[&](FlowchartPoint p){
  return horizontal?FlowchartPoint{p.x,a.y}:FlowchartPoint{a.x,p.y};
 };
 const ElkShiftedRun run{c,points[last],shift(c),shift(points[last])};
 for(std::size_t i=2;i<=last;++i)points[i]=shift(points[i]);
 points.erase(points.begin()+1,points.begin()+3);
 return run;
}
std::size_t elk_route_crossings(std::span<const FlowchartPoint> a,
                                std::span<const FlowchartPoint> b) {
 const auto side=[](auto o,auto p,auto q){
  return (p.x-o.x)*(q.y-o.y)-(p.y-o.y)*(q.x-o.x);
 };
 const auto opposite=[](double p,double q){return (p>0&&q<0)||(p<0&&q>0);};
 std::size_t count=0;
 for(std::size_t i=1;i<a.size();++i)for(std::size_t j=1;j<b.size();++j){
  const auto p=a[i-1],q=a[i],r=b[j-1],s=b[j];
  count+=opposite(side(r,s,p),side(r,s,q))&&opposite(side(p,q,r),side(p,q,s));
 }
 return count;
}
FlowchartPoint project_elk_shifted_label(FlowchartPoint label,std::span<const ElkShiftedRun> runs) {
 auto nearest=std::numeric_limits<double>::infinity();
 FlowchartPoint result=label;
 for(const auto& run:runs){
  const auto dx=run.old_end.x-run.old_start.x,dy=run.old_end.y-run.old_start.y;
  const auto x=label.x-run.old_start.x,y=label.y-run.old_start.y;
  const auto square_length=dx*dx+dy*dy;
  const auto t=square_length==0?0:std::clamp((x*dx+y*dy)/square_length,0.,1.);
  const auto distance=(x-t*dx)*(x-t*dx)+(y-t*dy)*(y-t*dy);
  if(distance>=nearest)continue;
  nearest=distance;
  result={run.new_start.x+t*(run.new_end.x-run.new_start.x),
          run.new_start.y+t*(run.new_end.y-run.new_start.y)};
 }
 return result;
}
bool elk_route_covers_label(std::span<const FlowchartPoint> route,
 const std::optional<FlowchartPoint>& label,FlowchartEdgeLabelSize size) {
 if(!label||size.width==0||size.height==0)return false;
 for(std::size_t i=1;i<route.size();++i){
  const auto a=route[i-1],b=route[i];
  const auto dx=b.x-a.x,dy=b.y-a.y;
  const auto x=a.x-label->x,y=a.y-label->y;
  const auto half_width=size.width/2,half_height=size.height/2;
  const std::array boundaries{
   std::pair{-dx,x+half_width},std::pair{dx,half_width-x},
   std::pair{-dy,y+half_height},std::pair{dy,half_height-y}};
  double begin=0,end=1;
  for(const auto [p,q]:boundaries){
   if(p==0){
    if(q<0){end=0;break;}
   }else if(p<0)begin=std::max(begin,q/p);
   else end=std::min(end,q/p);
  }
  if(begin<end)return true;
 }
 return false;
}
void straighten_state_elk_terminals(std::span<FlowchartPlacedEdge> edges,
                                    std::span<const FlowchartEdgeLabelSize> sizes) {
 for(std::size_t i=0;i<edges.size();++i){
  auto& edge=edges[i];
  if(edge.points.size()<5)continue;
  auto points=edge.points;
  std::array<ElkShiftedRun,2> runs;
  std::size_t run_count=0;
  if(const auto front=straighten_elk_front(points))runs[run_count++]=*front;
  std::ranges::reverse(points);
  if(const auto back=straighten_elk_front(points))runs[run_count++]=*back;
  if(run_count==0)continue;
  std::ranges::reverse(points);
  auto label=edge.label;
  if(label)label=project_elk_shifted_label(*label,std::span(runs).first(run_count));
  std::size_t before=0,after=0,covered_before=0,covered_after=0;
  for(std::size_t j=0;j<edges.size();++j){
   const auto& other=edges[j];
   if(j==i||other.points.size()<2)continue;
   before+=elk_route_crossings(edge.points,other.points);
   after+=elk_route_crossings(points,other.points);
   covered_before+=elk_route_covers_label(other.points,edge.label,sizes[i]);
   covered_before+=elk_route_covers_label(edge.points,other.label,sizes[j]);
   covered_after+=elk_route_covers_label(other.points,label,sizes[i]);
   covered_after+=elk_route_covers_label(points,other.label,sizes[j]);
  }
  if(after>before||covered_after>covered_before)continue;
  edge.points=std::move(points);
  edge.label=label;
 }
}
