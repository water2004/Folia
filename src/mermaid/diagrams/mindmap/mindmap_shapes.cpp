module folia.mermaid.mindmap_shapes;
import folia.mermaid.number;
import std;

namespace folia::mermaid {
namespace {
std::string n(double value){return *format_number(value);}
struct ArcPath {
    std::string path="M0 0";
    double x=0,y=0,left=0,right=0,top=0,bottom=0;
    void point(double px,double py){left=std::min(left,px);right=std::max(right,px);top=std::min(top,py);bottom=std::max(bottom,py);}
    // Original cloud/bang paths use small arcs, mostly rotated by one degree.
    // Keep actual ellipse extrema rather than their control boxes.
    void arc(double rx,double ry,double rotation,bool sweep,double dx,double dy) {
        path+=" a"+n(rx)+","+n(ry)+" "+n(rotation)+" 0,"+(sweep?"1":"0")+" "+n(dx)+","+n(dy);
        const double phi=rotation*std::numbers::pi/180,cs=std::cos(phi),sn=std::sin(phi);
        const double ex=x+dx,ey=y+dy,px=(-cs*dx-sn*dy)/2,py=(sn*dx-cs*dy)/2;
        const double scale=px*px/(rx*rx)+py*py/(ry*ry);
        if(scale>1){rx*=std::sqrt(scale);ry*=std::sqrt(scale);}
        const double rx2=rx*rx,ry2=ry*ry;
        double factor=std::sqrt(std::max(0.0,(rx2*ry2-rx2*py*py-ry2*px*px)/(rx2*py*py+ry2*px*px)));
        if(!sweep)factor=-factor;
        const double local_x=factor*rx*py/ry,local_y=-factor*ry*px/rx;
        const double cx=(x+ex)/2+cs*local_x-sn*local_y,cy=(y+ey)/2+sn*local_x+cs*local_y;
        const double begin=std::atan2((py-local_y)/ry,(px-local_x)/rx),end=std::atan2((-py-local_y)/ry,(-px-local_x)/rx);
        constexpr double tau=2*std::numbers::pi;
        const auto positive=[&](double angle){return std::fmod(std::fmod(angle,tau)+tau,tau);};
        const double length=positive(sweep?end-begin:begin-end);
        const double x_extreme=std::atan2(-ry*sn,rx*cs),y_extreme=std::atan2(ry*cs,rx*sn);
        for(const double angle:{x_extreme,x_extreme+std::numbers::pi,y_extreme,y_extreme+std::numbers::pi})
            if(positive(sweep?angle-begin:begin-angle)<=length)point(cx+rx*cs*std::cos(angle)-ry*sn*std::sin(angle),cy+rx*sn*std::cos(angle)+ry*cs*std::sin(angle));
        point(ex,ey);x=ex;y=ey;
    }
};
std::string rect(double w,double h,double r) {
    const double x=-w/2,y=-h/2;r=std::min({r,w/2,h/2});
    return "M"+n(x+r)+" "+n(y)+" H"+n(x+w-r)+" Q"+n(x+w)+" "+n(y)+" "+n(x+w)+" "+n(y+r)+
        " V"+n(y+h-r)+" Q"+n(x+w)+" "+n(y+h)+" "+n(x+w-r)+" "+n(y+h)+" H"+n(x+r)+
        " Q"+n(x)+" "+n(y+h)+" "+n(x)+" "+n(y+h-r)+" V"+n(y+r)+" Q"+n(x)+" "+n(y)+" "+n(x+r)+" "+n(y)+" Z";
}
}
std::expected<MindmapShape,std::string> measure_mindmap_shape(const MindmapNode& node,MindmapLabelSize label,double radius) {
    if(!std::isfinite(label.width)||!std::isfinite(label.height)||label.width<0||label.height<0)
        return std::unexpected("Invalid Mindmap label measurement");
    MindmapShape shape;double w=0,h=0;
    if(node.type==0){w=label.width+4*node.padding;h=label.height+node.padding;shape.path=rect(w,h,5);}
    else if(node.type==1){w=label.width+30;h=label.height+30;shape.path=rect(w,h,radius);}
    else if(node.type==2){w=label.width+40;h=label.height+20;shape.path=rect(w,h,0);}
    else if(node.type==3){const double r=std::hypot(label.width,label.height)/2+node.padding;w=h=2*r;
        shape.path="M"+n(-r)+" 0 A"+n(r)+" "+n(r)+" 0 1 0 "+n(r)+" 0 A"+n(r)+" "+n(r)+" 0 1 0 "+n(-r)+" 0 Z";}
    else if(node.type==6){h=label.height+node.padding;const double m=h/4;w=label.width+2*m+node.padding;
        shape.path="M"+n(-w/2+m)+" "+n(h/2)+" L"+n(w/2-m)+" "+n(h/2)+" L"+n(w/2)+" 0 L"+n(w/2-m)+" "+n(-h/2)+" L"+n(-w/2+m)+" "+n(-h/2)+" L"+n(-w/2)+" 0 Z";}
    else if(node.type==4||node.type==5) {
        ArcPath arcs;
        if(node.type==4) {
            w=label.width+node.padding;h=label.height+node.padding;
            const double r1=.15*w,r2=.25*w,r3=.35*w,r4=.2*w;
            arcs.arc(r1,r1,false,true,.25*w,-.1*w);arcs.arc(r3,r3,true,true,.4*w,-.1*w);arcs.arc(r2,r2,true,true,.35*w,.2*w);
            arcs.arc(r1,r1,true,true,.15*w,.35*h);arcs.arc(r4,r4,true,true,-.15*w,.65*h);
            arcs.arc(r2,r1,true,true,-.25*w,.15*w);arcs.arc(r3,r3,true,true,-.5*w,0);arcs.arc(r1,r1,true,true,-.25*w,-.15*w);
            arcs.arc(r1,r1,true,true,-.1*w,-.35*h);arcs.arc(r4,r4,true,true,.1*w,-.65*h);
        } else {
            const double r=.15*(label.width+5*node.padding);
            w=std::max(label.width+5*node.padding,label.width+20);h=std::max(label.height+4*node.padding,label.height+20);
            arcs.arc(r,r,true,false,.25*w,-.1*h);arcs.arc(r,r,true,false,.25*w,0);arcs.arc(r,r,true,false,.25*w,0);arcs.arc(r,r,true,false,.25*w,.1*h);
            arcs.arc(r,r,true,false,.15*w,.33*h);arcs.arc(.8*r,.8*r,true,false,0,.34*h);arcs.arc(r,r,true,false,-.15*w,.33*h);
            arcs.arc(r,r,true,false,-.25*w,.15*h);arcs.arc(r,r,true,false,-.25*w,0);arcs.arc(r,r,true,false,-.25*w,0);arcs.arc(r,r,true,false,-.25*w,-.15*h);
            arcs.arc(r,r,true,false,-.1*w,-.33*h);arcs.arc(.8*r,.8*r,true,false,0,-.34*h);arcs.arc(r,r,true,false,.1*w,-.33*h);
        }
        shape.path=std::move(arcs.path)+" H0 V0 Z";shape.translate_x=-w/2;shape.translate_y=-h/2;
        shape.min_x=std::min(arcs.left-w/2,-label.width/2);shape.min_y=std::min(arcs.top-h/2,-label.height/2);
        shape.width=std::max(arcs.right-w/2,label.width/2)-shape.min_x;
        shape.height=std::max(arcs.bottom-h/2,label.height/2)-shape.min_y;
        return shape;
    } else return std::unexpected("Unknown Mindmap shape type");
    shape.min_x=-w/2;shape.min_y=-h/2;shape.width=w;shape.height=h;return shape;
}
}
