// Mermaid rendering-elements/edges.js generateRoundedPath. MIT, ../LICENSE.
// Included in flowchart_svg's private namespace; ELK supplies the clipped route.
std::string rounded_elk_path(std::span<const FlowchartPoint> points,double radius=5) {
    if(points.size()<2)return {};
    std::string path="M"+point(points.front());
    constexpr double epsilon=1e-5;
    for(std::size_t i=1;i+1<points.size();++i) {
        const auto a=points[i-1],b=points[i],c=points[i+1];
        const double dx1=b.x-a.x,dy1=b.y-a.y,dx2=c.x-b.x,dy2=c.y-b.y;
        const double len1=std::hypot(dx1,dy1),len2=std::hypot(dx2,dy2);
        if(len1<epsilon||len2<epsilon){path+="L"+point(b);continue;}
        const double nx1=dx1/len1,ny1=dy1/len1,nx2=dx2/len2,ny2=dy2/len2;
        const double angle=std::acos(std::clamp(nx1*nx2+ny1*ny2,-1.0,1.0));
        if(angle<epsilon||std::abs(std::numbers::pi-angle)<epsilon){path+="L"+point(b);continue;}
        const double cut=std::min({radius/std::sin(angle/2),len1/2,len2/2});
        path+="L"+point({b.x-nx1*cut,b.y-ny1*cut});
        path+="Q"+point(b)+" "+point({b.x+nx2*cut,b.y+ny2*cut});
    }
    return path+"L"+point(points.back());
}

// geometry.ts outlineAttachPoint: preserve the orthogonal departure axis when
// ELK's bounding-box port lies outside a diamond or a circular outline.
std::optional<FlowchartPoint> elk_outline_attachment(const FlowchartPlacedNode& node,
    std::u16string_view shape,FlowchartPoint port,FlowchartPoint next) {
    const double dx=next.x-port.x,dy=next.y-port.y;
    if((dx==0&&dy==0)||(std::abs(dx)>1e-6&&std::abs(dy)>1e-6))return {};
    const bool horizontal=std::abs(dx)>std::abs(dy);
    const auto along=[&](double t){return horizontal?FlowchartPoint{t,port.y}:FlowchartPoint{port.x,t};};
    const auto inside=[&](FlowchartPoint p) {
        const auto distance=std::hypot(p.x-node.x,p.y-node.y);
        if(distance==0)return true;
        const auto crossing=intersect(node,shape,p);
        return distance<=std::hypot(crossing.x-node.x,crossing.y-node.y)+1e-9;
    };
    double inner=horizontal?node.x:node.y,outer=horizontal?port.x:port.y;
    if(!inside(along(inner)))return {};
    if(inside(along(outer)))return port;
    for(int step=0;step<20;++step){const double mid=(inner+outer)/2;if(inside(along(mid)))inner=mid;else outer=mid;}
    return along(inner);
}

// render.ts cutter2/sanitizeElkEdgePoints for ordinary node endpoints. Container
// endpoints are handled by the layout adapter's frame clipping boundary.
void clip_elk_node_endpoints(std::vector<FlowchartPoint>& points,
    const FlowchartPlacedNode& start,const FlowchartPlacedNode& end,
    std::u16string_view start_shape,std::u16string_view end_shape) {
    points.insert(points.begin(),{start.x,start.y});points.push_back({end.x,end.y});
    const auto outside=[](const FlowchartPlacedNode& n,FlowchartPoint p) {
        return std::abs(p.x-n.x)>=n.width/2||std::abs(p.y-n.y)>=n.height/2;
    };
    const auto replacement=[&](const FlowchartPlacedNode& n,std::u16string_view shape,std::size_t i,int step) {
        const auto next=static_cast<std::ptrdiff_t>(i)+step;
        if(next>=0&&next<static_cast<std::ptrdiff_t>(points.size()))
            if(auto attached=elk_outline_attachment(n,shape,points[i],points[static_cast<std::size_t>(next)]))return *attached;
        auto p=points[i];const double dx=p.x-n.x,dy=p.y-n.y,length=std::hypot(dx,dy);
        if(length>0&&(std::abs(std::abs(dx)-n.width/2)<1||std::abs(std::abs(dy)-n.height/2)<1))
            p={n.x+dx/length*(length+10),n.y+dy/length*(length+10)};
        return intersect(n,shape,p);
    };
    const auto replace=[&](FlowchartPoint p,bool first) {
        const auto old=first?points.front():points.back();
        if(std::abs(old.x-p.x)<.1&&std::abs(old.y-p.y)<.1) {
            if(first)points.erase(points.begin());else points.pop_back();
        }else if(first)points.front()=p;else points.back()=p;
    };
    for(std::size_t i=0;i<points.size();++i)if(outside(start,points[i])){replace(replacement(start,start_shape,i,1),true);break;}
    std::size_t last=points.size()-2;
    for(std::size_t i=points.size();i-->0;)if(outside(end,points[i])){last=i;break;}
    replace(replacement(end,end_shape,last,-1),false);
    if(points.size()>1&&std::hypot(points.back().x-points[points.size()-2].x,points.back().y-points[points.size()-2].y)<2)points.pop_back();
    std::vector<FlowchartPoint> deduped;
    for(std::size_t i=0;i<points.size();++i)
        if(i==0||std::abs(points[i].x-points[i-1].x)>1e-6||std::abs(points[i].y-points[i-1].y)>1e-6)deduped.push_back(points[i]);
    points=std::move(deduped);
}

void elk_remove_short_marker_segment(std::vector<FlowchartPoint>& points,
    const FlowchartPlacedNode& node,bool first) {
    if(points.size()<3)return;
    const auto end=first?points.front():points.back();
    const auto adjacent=first?points[1]:points[points.size()-2];
    if(std::hypot(end.x-adjacent.x,end.y-adjacent.y)>=8)return;
    const double dx=std::abs(adjacent.x-node.x),dy=std::abs(adjacent.y-node.y);
    if((std::abs(dx-node.width/2)<=1&&dy<=node.height/2+1)||
       (std::abs(dy-node.height/2)<=1&&dx<=node.width/2+1))
        points.erase(first?points.begin()+1:points.end()-2);
}
