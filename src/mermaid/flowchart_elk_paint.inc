// Mermaid rendering-elements/edges.js generateRoundedPath. MIT, ../LICENSE.
// Included in flowchart_svg's private namespace; ELK supplies the clipped route.
// Accumulate the actual path length needed by Neo's absolute SVG dash gaps.
FlowchartPoint edge_path_mix(FlowchartPoint a,FlowchartPoint b,double wa,double wb,double divisor) {
    return {(wa*a.x+wb*b.x)/divisor,(wa*a.y+wb*b.y)/divisor};
}
double edge_path_distance(FlowchartPoint a,FlowchartPoint b) {return std::hypot(b.x-a.x,b.y-a.y);}

// The Neo dash gaps are in absolute SVG units, measured on the painted path.
double edge_path_cubic_length(FlowchartPoint a,FlowchartPoint b,FlowchartPoint c,FlowchartPoint d,int depth=0) {
    const double chord=edge_path_distance(a,d),polygon=edge_path_distance(a,b)+edge_path_distance(b,c)+edge_path_distance(c,d);
    if(polygon-chord<.001 || depth==10) return (chord+polygon)/2;
    const auto ab=edge_path_mix(a,b,1,1,2),bc=edge_path_mix(b,c,1,1,2),cd=edge_path_mix(c,d,1,1,2);
    const auto left=edge_path_mix(ab,bc,1,1,2),right=edge_path_mix(bc,cd,1,1,2),mid=edge_path_mix(left,right,1,1,2);
    return edge_path_cubic_length(a,ab,left,mid,depth+1)+edge_path_cubic_length(mid,right,cd,d,depth+1);
}
struct EdgePath {
    std::string svg;
    FlowchartPoint last;
    double* length;
    EdgePath(FlowchartPoint start,double* length):svg("M"+point(start)),last(start),length(length) {}
    void line(FlowchartPoint end) {
        svg+="L"+point(end);
        if(length) *length+=edge_path_distance(last,end);
        last=end;
    }
    void cubic(FlowchartPoint a,FlowchartPoint b,FlowchartPoint end) {
        svg+="C"+point(a)+" "+point(b)+" "+point(end);
        if(length) *length+=edge_path_cubic_length(last,a,b,end);
        last=end;
    }
    void quadratic(FlowchartPoint control,FlowchartPoint end) {
        svg+="Q"+point(control)+" "+point(end);
        if(length) *length+=edge_path_cubic_length(last,edge_path_mix(last,control,1,2,3),edge_path_mix(control,end,2,1,3),end);
        last=end;
    }
};
std::string rounded_elk_path(std::span<const FlowchartPoint> points,double radius=5,double* length=nullptr) {
    if(points.size()<2)return {};
    EdgePath path(points.front(),length);
    constexpr double epsilon=1e-5;
    for(std::size_t i=1;i+1<points.size();++i) {
        const auto a=points[i-1],b=points[i],c=points[i+1];
        const double dx1=b.x-a.x,dy1=b.y-a.y,dx2=c.x-b.x,dy2=c.y-b.y;
        const double len1=std::hypot(dx1,dy1),len2=std::hypot(dx2,dy2);
        if(len1<epsilon||len2<epsilon){path.line(b);continue;}
        const double nx1=dx1/len1,ny1=dy1/len1,nx2=dx2/len2,ny2=dy2/len2;
        const double angle=std::acos(std::clamp(nx1*nx2+ny1*ny2,-1.0,1.0));
        if(angle<epsilon||std::abs(std::numbers::pi-angle)<epsilon){path.line(b);continue;}
        const double cut=std::min({radius/std::sin(angle/2),len1/2,len2/2});
        path.line({b.x-nx1*cut,b.y-ny1*cut});
        path.quadratic(b,{b.x+nx2*cut,b.y+ny2*cut});
    }
    path.line(points.back());
    return std::move(path.svg);
}

// geometry.ts outlineAttachPoint: preserve the orthogonal departure axis when
// ELK's bounding-box port lies outside a diamond or a circular outline.
template<class Outline>
std::optional<FlowchartPoint> elk_outline_attachment(const FlowchartPlacedNode& node,
    std::u16string_view shape,FlowchartPoint port,FlowchartPoint next,const Outline& outline,
    bool exact_rectangles=false) {
    const double dx=next.x-port.x,dy=next.y-port.y;
    if((dx==0&&dy==0)||(std::abs(dx)>1e-6&&std::abs(dy)>1e-6))return {};
    const bool horizontal=std::abs(dx)>std::abs(dy);
    // The hand-drawn rectangle's intersection still uses the measured
    // rectangular bounds. Resolve that axis analytically to avoid leaving a
    // microscopic bisection segment that consumes extra RoughJS random draws.
    if(exact_rectangles&&(shape==u"squareRect"||shape==u"roundedRect")) {
        const auto left=node.x-node.width/2,right=node.x+node.width/2;
        const auto top=node.y-node.height/2,bottom=node.y+node.height/2;
        if(horizontal&&port.y>=top&&port.y<=bottom)
            return FlowchartPoint{std::clamp(port.x,left,right),port.y};
        if(!horizontal&&port.x>=left&&port.x<=right)
            return FlowchartPoint{port.x,std::clamp(port.y,top,bottom)};
    }
    const auto along=[&](double t){return horizontal?FlowchartPoint{t,port.y}:FlowchartPoint{port.x,t};};
    const auto inside=[&](FlowchartPoint p) {
        const auto distance=std::hypot(p.x-node.x,p.y-node.y);
        if(distance==0)return true;
        const auto crossing=outline(node,shape,p);
        return distance<=std::hypot(crossing.x-node.x,crossing.y-node.y)+1e-9;
    };
    double inner=horizontal?node.x:node.y,outer=horizontal?port.x:port.y;
    if(!inside(along(inner)))return {};
    if(inside(along(outer)))return port;
    for(int step=0;step<20;++step){const double mid=(inner+outer)/2;if(inside(along(mid)))inner=mid;else outer=mid;}
    return along(inner);
}

// Mermaid 12.2.0 ELK sanitizeElkEdgePoints: preserve an existing frame
// anchor, or clip the actual route where it leaves a tightened frame.
bool clip_elk_group_endpoint(std::vector<FlowchartPoint>& points,
    const FlowchartPlacedNode& node,bool first) {
    if(points.size()<2)return false;
    const auto at=[&](std::size_t i){return first?i:points.size()-1-i;};
    const auto on_border=[&](FlowchartPoint p) {
        constexpr double tolerance=.5;
        const auto x=std::abs(p.x-node.x),y=std::abs(p.y-node.y);
        const auto w=node.width/2,h=node.height/2;
        return (std::abs(x-w)<=tolerance&&y<=h+tolerance)||
               (std::abs(y-h)<=tolerance&&x<=w+tolerance);
    };
    const auto replace_prefix=[&](std::size_t count,FlowchartPoint crossing) {
        if(first){points.erase(points.begin(),points.begin()+count);points.insert(points.begin(),crossing);}
        else{points.resize(points.size()-count);points.push_back(crossing);}
    };
    const auto raw=points[at(1)];
    // The ordinary endpoint pass prepends/appends one synthetic node centre.
    if(on_border(raw)){replace_prefix(2,raw);return true;}
    for(std::size_t i=2;i<points.size();++i){
        const auto out=points[at(i)],in=points[at(i-1)];
        if(std::abs(out.x-node.x)<node.width/2&&std::abs(out.y-node.y)<node.height/2)continue;
        // An already exterior raw anchor is handled by ordinary intersection.
        if(std::abs(in.x-node.x)>=node.width/2||std::abs(in.y-node.y)>=node.height/2)return false;
        const auto dx=out.x-in.x,dy=out.y-in.y;
        const auto inf=std::numeric_limits<double>::infinity();
        const auto tx=dx==0?inf:(node.x+std::copysign(node.width/2,dx)-in.x)/dx;
        const auto ty=dy==0?inf:(node.y+std::copysign(node.height/2,dy)-in.y)/dy;
        const auto t=std::min(tx,ty);
        replace_prefix(i,{in.x+t*dx,in.y+t*dy});
        return true;
    }
    return false;
}
struct ClassicElkOutline {
    FlowchartPoint operator()(const FlowchartPlacedNode& node,std::u16string_view shape,FlowchartPoint p) const {
        return intersect(node,shape,p);
    }
};

template<class Outline=ClassicElkOutline>
void clip_elk_node_endpoints(std::vector<FlowchartPoint>& points,
    const FlowchartPlacedNode& start,const FlowchartPlacedNode& end,
    std::u16string_view start_shape,std::u16string_view end_shape,Outline outline={},bool exact_rectangles=false,
    bool start_group=false,bool end_group=false) {
    points.insert(points.begin(),{start.x,start.y});points.push_back({end.x,end.y});
    const bool skip_start=start_group&&clip_elk_group_endpoint(points,start,true);
    const bool skip_end=end_group&&clip_elk_group_endpoint(points,end,false);
    const auto outside=[](const FlowchartPlacedNode& n,FlowchartPoint p) {
        constexpr double rounding_slack=1e-9;
        return std::abs(p.x-n.x)+rounding_slack>=n.width/2||
            std::abs(p.y-n.y)+rounding_slack>=n.height/2;
    };
    const auto replacement=[&](const FlowchartPlacedNode& n,std::u16string_view shape,std::size_t i,int step) {
        const auto next=static_cast<std::ptrdiff_t>(i)+step;
        if(next>=0&&next<static_cast<std::ptrdiff_t>(points.size()))
            if(auto attached=elk_outline_attachment(n,shape,points[i],points[static_cast<std::size_t>(next)],outline,exact_rectangles))return *attached;
        auto p=points[i];const double dx=p.x-n.x,dy=p.y-n.y,length=std::hypot(dx,dy);
        if(length>0&&(std::abs(std::abs(dx)-n.width/2)<1||std::abs(std::abs(dy)-n.height/2)<1))
            p={n.x+dx/length*(length+10),n.y+dy/length*(length+10)};
        return outline(n,shape,p);
    };
    const auto replace=[&](FlowchartPoint p,bool first) {
        const auto old=first?points.front():points.back();
        if(std::abs(old.x-p.x)<.1&&std::abs(old.y-p.y)<.1) {
            if(first)points.erase(points.begin());else points.pop_back();
        }else if(first)points.front()=p;else points.back()=p;
    };
    if(!skip_start)for(std::size_t i=0;i<points.size();++i)
        if(outside(start,points[i])){replace(replacement(start,start_shape,i,1),true);break;}
    if(!skip_end){
        std::size_t last=points.size()-2;
        for(std::size_t i=points.size();i-->0;)if(outside(end,points[i])){last=i;break;}
        replace(replacement(end,end_shape,last,-1),false);
    }
    if(!skip_end&&points.size()>1&&std::hypot(points.back().x-points[points.size()-2].x,points.back().y-points[points.size()-2].y)<2)points.pop_back();
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
