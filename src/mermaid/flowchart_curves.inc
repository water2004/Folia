// D3 Shape 3.2.0 Flowchart line curves, and Mermaid edges.js corner handling.
// MIT/ISC; see THIRD_PARTY_NOTICES.md. EdgePath owns SVG output and length.
FlowchartPoint mix(FlowchartPoint a,FlowchartPoint b,double wa,double wb,double divisor) {
    return {(wa*a.x+wb*b.x)/divisor,(wa*a.y+wb*b.y)/divisor};
}

void basis_curve(EdgePath& path,std::span<const FlowchartPoint> points) {
    path.line(mix(points[0],points[1],5,1,6));
    const auto segment=[&](FlowchartPoint a,FlowchartPoint b,FlowchartPoint c) {
        path.cubic(mix(a,b,2,1,3),mix(a,b,1,2,3),{(a.x+4*b.x+c.x)/6,(a.y+4*b.y+c.y)/6});
    };
    for(std::size_t i=2;i<points.size();++i) segment(points[i-2],points[i-1],points[i]);
    segment(points[points.size()-2],points.back(),points.back());
    path.line(points.back());
}

void cardinal_curve(EdgePath& path,std::span<const FlowchartPoint> points) {
    for(std::size_t i=1;i<points.size();++i) {
        const auto p=points[i-1],q=points[i];
        const auto a=i>1?points[i-2]:q,b=i+1<points.size()?points[i+1]:p;
        path.cubic({p.x+(q.x-a.x)/6,p.y+(q.y-a.y)/6},
            {q.x+(p.x-b.x)/6,q.y+(p.y-b.y)/6},q);
    }
}

void catmull_rom_curve(EdgePath& path,std::span<const FlowchartPoint> points) {
    auto chord=[](FlowchartPoint a,FlowchartPoint b) {
        const auto dx=a.x-b.x,dy=a.y-b.y;
        return std::sqrt(dx*dx+dy*dy);
    };
    double previous=0,current=chord(points[0],points[1]);
    for(std::size_t i=1;i<points.size();++i) {
        const auto p=points[i-1],q=points[i];
        const auto a=i>1?points[i-2]:p,b=i+1<points.size()?points[i+1]:q;
        const auto next=chord(q,b);
        const auto l=std::sqrt(previous),m=std::sqrt(current),r=std::sqrt(next);
        auto c1=p,c2=q;
        if(l>1e-12) {
            const auto A=2*previous+3*l*m+current,N=3*l*(l+m);
            c1={(p.x*A-a.x*current+q.x*previous)/N,(p.y*A-a.y*current+q.y*previous)/N};
        }
        if(r>1e-12) {
            const auto B=2*next+3*r*m+current,N=3*r*(r+m);
            c2={(q.x*B+p.x*next-b.x*current)/N,(q.y*B+p.y*next-b.y*current)/N};
        }
        path.cubic(c1,c2,q);
        previous=current;current=next;
    }
}

void monotone_curve(EdgePath& path,std::span<const FlowchartPoint> points,bool reflect) {
    const auto map=[reflect](FlowchartPoint p) {return reflect?FlowchartPoint{p.y,p.x}:p;};
    std::vector<FlowchartPoint> distinct;
    for(auto raw:points) {
        auto p=map(raw);
        if(distinct.empty()||distinct.back().x!=p.x||distinct.back().y!=p.y) distinct.push_back(p);
    }
    if(distinct.size()<3) {
        if(distinct.size()==2) path.line(map(distinct.back()));
        else path.svg+='Z';
        return;
    }
    const auto slope3=[](FlowchartPoint a,FlowchartPoint b,FlowchartPoint c) {
        const auto h0=b.x-a.x,h1=c.x-b.x;
        const auto s0=(b.y-a.y)/(h0!=0?h0:h1<0?-0.0:0.0);
        const auto s1=(c.y-b.y)/(h1!=0?h1:h0<0?-0.0:0.0);
        const auto p=(s0*h1+s1*h0)/(h0+h1);
        if(std::isnan(s0)||std::isnan(s1)||std::isnan(p)) return 0.0;
        const auto slope=((s0<0?-1:1)+(s1<0?-1:1))*std::min({std::abs(s0),std::abs(s1),.5*std::abs(p)});
        return std::isnan(slope)?0.0:slope;
    };
    const auto slope2=[](FlowchartPoint a,FlowchartPoint b,double t) {
        const auto h=b.x-a.x;
        return h!=0?(3*(b.y-a.y)/h-t)/2:t;
    };
    auto next=slope3(distinct[0],distinct[1],distinct[2]);
    auto previous=slope2(distinct[0],distinct[1],next);
    for(std::size_t i=1;i<distinct.size();++i) {
        const auto p=distinct[i-1],q=distinct[i];
        const auto dx=(q.x-p.x)/3;
        path.cubic(map({p.x+dx,p.y+dx*previous}),map({q.x-dx,q.y-dx*next}),map(q));
        previous=next;
        if(i+2<distinct.size()) next=slope3(q,distinct[i+1],distinct[i+2]);
        else if(i+1<distinct.size()) next=slope2(q,distinct[i+1],previous);
    }
}

auto natural_controls(std::span<const FlowchartPoint> points,double FlowchartPoint::*coordinate) {
    const auto n=points.size()-1;
    std::vector<double> a(n,1),b(n,4),r(n);
    const auto x=[&](auto i) {return points[i].*coordinate;};
    b[0]=2;r[0]=x(0)+2*x(1);
    for(std::size_t i=1;i+1<n;++i) r[i]=4*x(i)+2*x(i+1);
    a.back()=2;b.back()=7;r.back()=8*x(n-1)+x(n);
    for(std::size_t i=1;i<n;++i) {
        const auto m=a[i]/b[i-1];
        b[i]-=m;r[i]-=m*r[i-1];
    }
    a.back()=r.back()/b.back();
    for(std::size_t i=n-1;i-->0;) a[i]=(r[i]-a[i+1])/b[i];
    b.back()=(x(n)+a.back())/2;
    for(std::size_t i=0;i+1<n;++i) b[i]=2*x(i+1)-a[i+1];
    return std::pair{std::move(a),std::move(b)};
}

std::string curve_path(std::span<const FlowchartPoint> points,std::u16string_view curve,double* length=nullptr) {
    if(points.empty()) return {};
    if(curve==u"rounded") return rounded_elk_path(points,5,length);
    EdgePath path(points.front(),length);
    if(points.size()==1) return path.svg+'Z';
    if(curve==u"step"||curve==u"stepBefore"||curve==u"stepAfter") {
        const auto t=curve==u"stepBefore"?0.0:curve==u"stepAfter"?1.0:.5;
        for(std::size_t i=1;i<points.size();++i) {
            const auto a=points[i-1],b=points[i];
            if(t==0) {
                path.line({a.x,b.y});path.line(b);
            } else {
                const auto x=a.x*(1-t)+b.x*t;
                path.line({x,a.y});path.line({x,b.y});
            }
        }
        if(t==.5) path.line(points.back());
    } else if(curve==u"bumpX"||curve==u"bumpY") {
        for(std::size_t i=1;i<points.size();++i) {
            const auto a=points[i-1],b=points[i];
            const auto x=(a.x+b.x)/2,y=(a.y+b.y)/2;
            if(curve==u"bumpX") path.cubic({x,a.y},{x,b.y},b);
            else path.cubic({a.x,y},{b.x,y},b);
        }
    } else if(curve==u"linear"||points.size()==2) {
        for(auto p:points|std::views::drop(1)) path.line(p);
    } else if(curve==u"cardinal") {
        cardinal_curve(path,points);
    } else if(curve==u"catmullRom") {
        catmull_rom_curve(path,points);
    } else if(curve==u"monotoneX"||curve==u"monotoneY") {
        monotone_curve(path,points,curve==u"monotoneY");
    } else if(curve==u"natural") {
        const auto [x0,x1]=natural_controls(points,&FlowchartPoint::x);
        const auto [y0,y1]=natural_controls(points,&FlowchartPoint::y);
        for(std::size_t i=0;i<x0.size();++i) path.cubic({x0[i],y0[i]},{x1[i],y1[i]},points[i+1]);
    } else {
        basis_curve(path,points);
    }
    return std::move(path.svg);
}

std::vector<FlowchartPoint> fix_curve_corners(std::span<const FlowchartPoint> points) {
    std::vector<FlowchartPoint> result;
    if(points.size()<3) return {points.begin(),points.end()};
    result.push_back(points.front());
    for(std::size_t i=1;i+1<points.size();++i) {
        const auto a=points[i-1],b=points[i],c=points[i+1];
        const bool vertical=a.x==b.x&&b.y==c.y&&std::abs(b.y-a.y)>5&&std::abs(b.x-c.x)>5;
        const bool horizontal=a.y==b.y&&b.x==c.x&&std::abs(b.x-a.x)>5&&std::abs(b.y-c.y)>5;
        if(!vertical&&!horizontal) {result.push_back(b);continue;}
        auto before=b,after=b;
        shorten(before,a,5);shorten(after,c,5);
        auto corner=b;
        if(std::abs(c.x-a.x)>10&&std::abs(c.y-a.y)>=10) {
            const auto offset=2*std::numbers::sqrt2;
            const auto dx=after.x<before.x?-1:1,dy=after.y<before.y?-1:1;
            corner={before.x+dx*(vertical?5-offset:offset),
                before.y+dy*(vertical?offset:5-offset)};
        }
        result.insert(result.end(),{before,corner,after});
    }
    result.push_back(points.back());
    return result;
}

std::string flowchart_edge_path(std::vector<FlowchartPoint> points,std::u16string_view curve,
    const FlowchartEdge& edge,double* length=nullptr) {
    if(points.size()<2) return curve_path(points,curve,length);
    if(curve!=u"rounded") points=fix_curve_corners(points);
    if(edge.arrow_start==u"arrow_point") shorten(points.front(),points[1],4);
    if(edge.arrow_end==u"arrow_point") shorten(points.back(),points[points.size()-2],4);
    return curve_path(points,curve,length);
}
