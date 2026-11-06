// MIT. Mermaid 12.2.0 rendering-elements/lineJump.ts and ELK lineHops.ts.
struct ElkHop {
    std::size_t segment;
    double fraction;
    FlowchartPoint point;
};
struct ElkHopRoute {
    std::string path;
    double length=0;
};
struct ElkHopIntersection {
    double a,b;
    FlowchartPoint point;
};
std::optional<ElkHopIntersection> elk_hop_intersection(
    FlowchartPoint a,FlowchartPoint b,FlowchartPoint c,FlowchartPoint d) {
    constexpr double epsilon=1e-6;
    const auto ax=b.x-a.x,ay=b.y-a.y,bx=d.x-c.x,by=d.y-c.y;
    const auto det=ax*by-ay*bx;
    if(det==0)return {};
    const auto dx=c.x-a.x,dy=c.y-a.y;
    const auto t=(dx*by-dy*bx)/det,u=(dx*ay-dy*ax)/det;
    if(t<=epsilon||t>=1-epsilon||u<=epsilon||u>=1-epsilon)return {};
    return ElkHopIntersection{t,u,{a.x+t*ax,a.y+t*ay}};
}
std::vector<std::optional<RoundedElkCorner>> elk_hop_corners(std::span<const FlowchartPoint> points) {
    std::vector<std::optional<RoundedElkCorner>> result(points.size());
    for(std::size_t i=1;i+1<points.size();++i)
        result[i]=rounded_elk_corner(points[i-1],points[i],points[i+1],5);
    return result;
}
bool elk_hop_in_corner(std::span<const FlowchartPoint> points,
    std::span<const std::optional<RoundedElkCorner>> corners,std::size_t segment,double fraction) {
    const auto length=edge_path_distance(points[segment],points[segment+1]);
    const auto& a=corners[segment];
    const auto& b=corners[segment+1];
    return (a&&fraction*length<a->cut)||(b&&length-fraction*length<b->cut);
}
bool elk_hop_horizontal(FlowchartPoint a,FlowchartPoint b) {
    return std::abs(b.x-a.x)>=std::abs(b.y-a.y);
}
double elk_hop_quantize(double x) {return std::floor(x*1000+.5)/1000;}
FlowchartPoint elk_hop_quantize(FlowchartPoint p) {
    return {elk_hop_quantize(p.x),elk_hop_quantize(p.y)};
}
ElkHopRoute rewrite_elk_hop_route(std::vector<FlowchartPoint> points,
    std::span<ElkHop> hops,const FlowchartEdge& edge,FlowchartElkLineHops style) {
    constexpr double jump_radius=6,clearance=2,min_radius=jump_radius*.6;
    const auto offset=[](FlowchartPoint& p,FlowchartPoint neighbor,std::u16string_view arrow) {
        if(arrow!=u"arrow_point")return;
        const auto angle=std::atan2(neighbor.y-p.y,neighbor.x-p.x);
        p.x+=4*std::cos(angle);p.y+=4*std::sin(angle);
    };
    auto first=points.front(),last=points.back();
    offset(first,points[1],edge.arrow_start);
    offset(last,points[points.size()-2],edge.arrow_end);
    points.front()=first;points.back()=last;
    const auto corners=elk_hop_corners(points);
    std::ranges::stable_sort(hops,{},[](const ElkHop& hop){return std::pair{hop.segment,hop.fraction};});
    ElkHopRoute result;
    EdgePath path(elk_hop_quantize(first),&result.length);
    std::size_t next=0;
    struct Jump { FlowchartPoint center; double distance,radius; };
    std::vector<Jump> jumps;
    for(std::size_t i=0;i+1<points.size();++i) {
        const auto a=points[i],b=points[i+1];
        const auto length=edge_path_distance(a,b);
        const auto start=corners[i]?corners[i]->cut:0;
        const auto stop=length-(corners[i+1]?corners[i+1]->cut:0);
        jumps.clear();
        while(next<hops.size()&&hops[next].segment==i) {
            const auto& hop=hops[next++];
            const auto distance=hop.fraction*length;
            const auto room=std::min(distance-start,stop-distance)-clearance;
            const auto radius=std::min(jump_radius,room);
            if(radius>=min_radius)jumps.push_back({hop.point,distance,radius});
        }
        for(std::size_t j=1;j<jumps.size();++j) {
            auto& prev=jumps[j-1];
            auto& curr=jumps[j];
            const auto gap=curr.distance-prev.distance;
            if(prev.radius+curr.radius>gap) {
                prev.radius=std::min(prev.radius,gap/2);
                curr.radius=std::min(curr.radius,gap/2);
            }
        }
        for(const auto& jump:jumps) {
            if(jump.radius<min_radius)continue;
            const auto dx=(b.x-a.x)*jump.radius/length,dy=(b.y-a.y)*jump.radius/length;
            path.line(elk_hop_quantize({jump.center.x-dx,jump.center.y-dy}));
            const auto end=elk_hop_quantize({jump.center.x+dx,jump.center.y+dy});
            if(style==FlowchartElkLineHops::arc) {
                const auto radius=elk_hop_quantize(jump.radius);
                const auto sweep=elk_hop_horizontal(a,b)?b.x>=a.x:b.y>=a.y;
                path.svg+="A"+number(radius)+","+number(radius)+" 0 0 "+(sweep?"1 ":"0 ")+point(end);
                result.length+=std::numbers::pi*radius;
            } else path.svg+="M"+point(end);
            path.last=end;
        }
        if(const auto& corner=corners[i+1]) {
            path.line(elk_hop_quantize(corner->entry));
            path.quadratic(elk_hop_quantize(b),elk_hop_quantize(corner->exit));
        } else path.line(elk_hop_quantize(b));
    }
    result.path=std::move(path.svg);
    return result;
}
std::vector<ElkHopRoute> paint_elk_line_hops(std::span<const FlowchartPlacedEdge> routes,
    std::span<const FlowchartEdge> edges,FlowchartElkLineHops style) {
    std::vector<std::vector<ElkHop>> hops(routes.size());
    std::vector<std::vector<std::optional<RoundedElkCorner>>> corners;
    corners.reserve(routes.size());
    for(const auto& route:routes)corners.push_back(elk_hop_corners(route.points));
    for(std::size_t i=0;i<routes.size();++i) {
        const auto& a=routes[i].points;
        for(std::size_t j=i+1;j<routes.size();++j) {
            const auto& b=routes[j].points;
            for(std::size_t ai=0;ai+1<a.size();++ai)for(std::size_t bi=0;bi+1<b.size();++bi) {
                const auto hit=elk_hop_intersection(a[ai],a[ai+1],b[bi],b[bi+1]);
                if(!hit||elk_hop_in_corner(a,corners[i],ai,hit->a)||
                         elk_hop_in_corner(b,corners[j],bi,hit->b))continue;
                const bool on_a=elk_hop_horizontal(a[ai],a[ai+1])&&
                                !elk_hop_horizontal(b[bi],b[bi+1]);
                if(on_a)hops[i].push_back({ai,hit->a,hit->point});
                else hops[j].push_back({bi,hit->b,hit->point});
            }
        }
    }
    std::vector<ElkHopRoute> result(routes.size());
    for(std::size_t i=0;i<routes.size();++i)
        if(!hops[i].empty())result[i]=rewrite_elk_hop_route(routes[i].points,hops[i],edges[i],style);
    return result;
}
