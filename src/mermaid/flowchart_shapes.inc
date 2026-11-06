// Mermaid 12.2.0 classic shape geometry and polygon intersections. MIT.
// Included in flowchart_svg's private namespace; dimensions are measured SVG units.
constexpr double stadium_arc_step=std::numbers::pi/49;

double stadium_width_loss(double height) {
    return height*(1-std::cos(stadium_arc_step/2));
}

FlowchartNodeBox stadium_box(double label_width,double label_height,double padding) {
    const double height=label_height+padding;
    const double inscribed=height*std::cos(stadium_arc_step/2);
    const double cap=std::sqrt(std::max(0.0,inscribed*inscribed-label_height*label_height));
    const double width=std::max({label_width+height/4+padding,
        1.5*height+(height-inscribed),label_width+padding+height-cap});
    // updateNodeBounds reads the sampled path, whose arcs miss the x extrema.
    return {width-stadium_width_loss(height),height};
}

std::vector<FlowchartPoint> stadium_points(double bounds_width,double height) {
    const double width=bounds_width+stadium_width_loss(height),radius=height/2;
    std::vector<FlowchartPoint> points={{-width/2+radius,-height/2},{width/2-radius,-height/2}};
    const auto arc=[&](double center,double start) {
        for(int i=0;i<50;++i) {
            const double angle=start+i*stadium_arc_step;
            points.push_back({-(center+radius*std::cos(angle)),-radius*std::sin(angle)});
        }
    };
    arc(-width/2+radius,std::numbers::pi/2);
    points.push_back({width/2-radius,height/2});
    arc(width/2-radius,3*std::numbers::pi/2);
    return points;
}

std::vector<FlowchartPoint> subroutine_points(double width,double height) {
    const double left=-width/2,right=width/2,top=-height/2,bottom=height/2;
    return {{left+8,bottom},{right-8,bottom},{right-8,top},{left+8,top},
        {left+8,bottom},{left,bottom},{right,bottom},{right,top},{left,top},{left,bottom}};
}

bool classic_polygon(std::u16string_view shape) {
    return shape==u"hexagon" || shape==u"lean_right" || shape==u"lean_left" ||
        shape==u"trapezoid" || shape==u"inv_trapezoid" || shape==u"odd";
}

// Center the original points after insertPolygonShape/rectLeftInvArrow's
// translation. Width is the painted bounds, including slants and the notch.
std::vector<FlowchartPoint> classic_polygon_points(std::u16string_view shape,double width,double height) {
    const double left=-width/2,right=width/2,top=-height/2,bottom=height/2;
    const double slant=height/2;
    if(shape==u"hexagon") return {{left+height/4,bottom},{right-height/4,bottom},
        {right,0},{right-height/4,top},{left+height/4,top},{left,0}};
    if(shape==u"lean_right") return {{left,bottom},{right-slant,bottom},{right,top},{left+slant,top}};
    if(shape==u"lean_left") return {{left+slant,bottom},{right,bottom},{right-slant,top},{left,top}};
    if(shape==u"trapezoid") return {{left,bottom},{right,bottom},{right-slant,top},{left+slant,top}};
    if(shape==u"inv_trapezoid") return {{left+slant,bottom},{right-slant,bottom},{right,top},{left,top}};
    return {{left,top},{left+height/4,0},{left,bottom},{right,bottom},{right,top}};
}

// Preserve the pinned intersect-line predicate, including its near-zero test.
std::optional<FlowchartPoint> shape_line_intersection(
    FlowchartPoint p1,FlowchartPoint p2,FlowchartPoint q1,FlowchartPoint q2) {
    const double a1=p2.y-p1.y,b1=p1.x-p2.x,c1=p2.x*p1.y-p1.x*p2.y;
    const double r3=a1*q1.x+b1*q1.y+c1,r4=a1*q2.x+b1*q2.y+c1;
    if(r3!=0 && r4!=0 && r3*r4>0) return {};
    const double a2=q2.y-q1.y,b2=q1.x-q2.x,c2=q2.x*q1.y-q1.x*q2.y;
    const double r1=a2*p1.x+b2*p1.y+c2,r2=a2*p2.x+b2*p2.y+c2;
    if(std::abs(r1)<1e-6 && std::abs(r2)<1e-6 && r1*r2>0) return {};
    const double denominator=a1*b2-a2*b1;
    if(denominator==0) return {};
    return FlowchartPoint{(b1*c2-b2*c1)/denominator,(a2*c1-a1*c2)/denominator};
}

FlowchartPoint polygon_intersection(const FlowchartPlacedNode& node,
    std::span<const FlowchartPoint> points,FlowchartPoint target) {
    double min_x=std::numeric_limits<double>::infinity(),min_y=min_x;
    for(auto p:points) {min_x=std::min(min_x,p.x);min_y=std::min(min_y,p.y);}
    const double left=node.x-node.width/2-min_x,top=node.y-node.height/2-min_y;
    FlowchartPoint result{node.x,node.y};double distance=std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i<points.size();++i) {
        const auto a=points[i],b=points[(i+1)%points.size()];
        if(auto p=shape_line_intersection({node.x,node.y},target,{left+a.x,top+a.y},{left+b.x,top+b.y})) {
            const double candidate=std::hypot(p->x-target.x,p->y-target.y);
            if(candidate<distance) {result=*p;distance=candidate;}
        }
    }
    return result;
}

double cylinder_radius_y(double width) {return (width/2)/(2.5+width/50);}

std::string polygon_path(std::span<const FlowchartPoint> points) {
    std::string path;
    for(auto p:points) path+=(path.empty()?"M":"L")+point(p);
    return path+"Z";
}

std::string cylinder_path(double width,double height) {
    const double ry=cylinder_radius_y(width),body=height-2*ry;
    const auto arc="a"+number(width/2)+","+number(ry)+" 0,0,0 ";
    // Same path as createCylinderPathD, translated into node-local coordinates.
    return "M"+point({-width/2,-body/2})+arc+number(width)+",0"+arc+number(-width)+",0"+
        "l0,"+number(body)+arc+number(width)+",0l0,"+number(-body);
}
