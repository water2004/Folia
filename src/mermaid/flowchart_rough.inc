// Mermaid 12.2.0 rendering-elements/shapes/{drawRect,question,roundedRectPath}.
// The seeded group is generated before layout so Dagre/ELK get the path bounds.
struct RoughFlowchartLayer {
    RoughPolygonResult drawing;
    FlowchartPoint offset{};
    std::string_view stroke_width;
    bool solid_fill = false;
    bool invisible_stroke = false;
};
struct RoughFlowchartNode : RoughFlowchartLayer {
    std::vector<RoughFlowchartLayer> overlays;
    FlowchartPoint label_offset{};
    bool grouped = false;
};

RoughFlowchartNode rough_flowchart_contours(FlowchartLocalShape& shape,
    const FlowchartPaint& paint,const FlowchartSvgTheme& theme) {
    const auto fill=paint.get(u"fill",theme.fill);
    const auto stroke=paint.get(u"stroke",theme.border);
    RoughFlowchartNode result;
    result.grouped=shape.contours.size()>1&&std::ranges::all_of(shape.contours,&FlowchartContour::measured);
    std::optional<RoughBounds> measured,painted;
    for(std::size_t i=0;i<shape.contours.size();++i) {
        const auto& contour=shape.contours[i];
        const auto mode=fill=="none"||fill=="transparent"?RoughFill::none:contour.sketch_fill;
        auto drawing=sketch_svg_path(contour.path,{.seed=theme.hand_drawn_seed,.fill=mode,.stroke=stroke!="none"});
        if(drawing.bounds) {
            if(contour.measured) include_outline_bounds(measured,*drawing.bounds,contour.translation);
            if(!contour.invisible_stroke||!drawing.fill_path.empty())
                include_outline_bounds(painted,*drawing.bounds,contour.translation);
        }
        RoughFlowchartLayer layer{std::move(drawing),contour.translation,{},
                                  mode==RoughFill::solid,contour.invisible_stroke};
        if(i==0) static_cast<RoughFlowchartLayer&>(result)=std::move(layer);
        else result.overlays.push_back(std::move(layer));
    }
    if(measured) shape.box={measured->width(),measured->height()};
    if(painted) shape.paint_bounds=*painted;
    return result;
}

std::string rough_rounded_rect(double x,double y,double width,double height,double radius) {
    const auto corner=" A"+number(radius)+" "+number(radius)+" 0 0 1 ";
    return "M"+point({x+radius,y})+"H"+number(x+width-radius)+corner+
        point({x+width,y+radius})+"V"+number(y+height-radius)+corner+
        point({x+width-radius,y+height})+"H"+number(x+radius)+corner+
        point({x,y+height-radius})+"V"+number(y+radius)+corner+point({x+radius,y})+"Z";
}

RoughPolygonResult rough_cluster_frame(const FlowchartPlacedNode& n,const FlowchartSvgTheme& theme) {
    return sketch_svg_path(rough_rounded_rect(n.x-n.width/2,n.y-n.height/2,n.width,n.height,0),
        {.seed=theme.hand_drawn_seed});
}

RoughFlowchartNode rough_flowchart_node(std::u16string_view shape, FlowchartNodeBox box,
    const FlowchartPaint& paint,const FlowchartSvgTheme& theme) {
    const auto fill=paint.get(u"fill",theme.fill);
    const auto stroke=paint.get(u"stroke",theme.border);
    RoughPolygonOptions options{.seed=theme.hand_drawn_seed,.stroke=stroke!="none"};
    const auto x=-box.width/2,y=-box.height/2;
    if(shape==u"squareRect"||(shape==u"roundedRect"&&theme.radius==0))
        return {{sketch_rectangle(x,y,box.width,box.height,options)}};
    if(shape==u"circle") return {{sketch_ellipse({},box.width,box.height,options)}};
    if(shape==u"doublecircle") {
        options.roughness=.2;
        RoughFlowchartNode node{{sketch_ellipse({},box.width,box.height,options),{},"2.5"}};
        node.overlays.push_back({sketch_ellipse({},box.width-10,box.height-10,options),{},"1.5"});
        return node;
    }
    if(shape==u"subroutine") {
        RoughFlowchartNode node{{sketch_rectangle(x,y,box.width,box.height,options)}};
        options.fill=RoughFill::none;
        for(const auto line_x:{-x-8,x+8}) {
            const std::array path{RoughCommand{RoughVerb::move,{line_x,y}},
                RoughCommand{RoughVerb::line,{line_x,-y}}};
            node.overlays.push_back({sketch_path(path,options)});
        }
        return node;
    }
    if(fill=="none"||fill=="transparent") options.fill=RoughFill::none;
    if(shape==u"roundedRect") return {{sketch_svg_path(
        rough_rounded_rect(x,y,box.width,box.height,theme.radius),options)}};
    if(shape==u"cylinder") {
        const auto rx=box.width/2,ry=cylinder_radius_y(box.width),h=box.height-2*ry;
        const auto arc=" A"+point({rx,ry})+" 0,0,0 ";
        const auto outer="M"+point({0,ry})+"M"+point({box.width,ry})+arc+point({0,ry})+
            "V"+number(ry+h)+arc+point({box.width,ry+h})+"V"+number(ry);
        RoughFlowchartNode node{{sketch_svg_path(outer,options),{-rx,-h/2-ry}}};
        options.fill=RoughFill::none;
        node.overlays.push_back({sketch_svg_path("M"+point({-rx,-h/2})+arc+point({rx,-h/2}),options)});
        return node;
    }
    if(shape==u"diamond") {
        const double side=box.width;
        const std::array path{
            RoughCommand{RoughVerb::move,{side/2,0}},RoughCommand{RoughVerb::line,{side,-side/2}},
            RoughCommand{RoughVerb::line,{side/2,-side}},RoughCommand{RoughVerb::line,{0,-side/2}},
            RoughCommand{RoughVerb::close}};
        return {{sketch_path(path,options),{-side/2+0.5,side/2}}};
    }
    if(shape==u"stadium") return {{sketch_svg_path(
        polygon_path(stadium_points(box.width,box.height)),options)}};
    if(extra_flowchart_shape(shape)) {
        const auto w=box.width,h=box.height;
        if(shape==u"lin-rect"||shape==u"div-rect") {
            const auto l=-w/2,r=w/2,t=-h/2,b=h/2;
            std::array<RoughPoint,7> points;
            if(shape==u"lin-rect") {
                const auto x=l+8;
                points={{{x,t},{r,t},{r,b},{l,b},{l,t},{x,t},{x,b}}};
            } else {
                const auto y=-h/3;
                points={{{l,y},{r,y},{r,b},{l,b},{l,t},{r,t},{r,y}}};
            }
            return {{sketch_polygon(points,options)}};
        }
        if(shape==u"win-pane") {
            const auto l=-w/2-5,r=w/2-5,t=-h/2-5,b=h/2-5;
            const auto path="M"+point({l,t})+"L"+point({r,t})+"L"+point({r,b})+
                "L"+point({l,b})+"L"+point({l,t})+"M"+point({l,t+10})+"L"+point({r,t+10})+
                "M"+point({l+10,t})+"L"+point({l+10,b});
            return {{sketch_svg_path(path,options),{5,5}}};
        }
        auto points=extra_shape_outline(shape,w,h);
        FlowchartPoint offset;
        if(shape==u"notch-rect"||shape==u"tri"||shape==u"flip-tri") offset={-w/2,h/2};
        else if(shape==u"sl-rect") offset={0,h/6};
        if(shape==u"notch-rect") points.push_back(points.front());
        for(auto& p:points) {p.x-=offset.x;p.y-=offset.y;}
        return {{sketch_svg_path(polygon_path(points),options),offset}};
    }
    if(shape==u"odd") {
        const auto notch=box.height/4,w=box.width-notch;
        const std::array points{FlowchartPoint{-w/2-notch,y},FlowchartPoint{-w/2,0},
            FlowchartPoint{-w/2-notch,-y},FlowchartPoint{w/2,-y},FlowchartPoint{w/2,y}};
        return {{sketch_svg_path(polygon_path(points),options),{notch/2,0}},{},{notch/2,0}};
    }
    if(classic_polygon(shape)) {
        auto points=classic_polygon_points(shape,box.width,box.height);
        const auto base_width=shape==u"hexagon"?box.width:box.width-box.height;
        for(auto& p:points) {
            p.x+=base_width/2;
            p.y-=box.height/2;
        }
        return {{sketch_svg_path(polygon_path(points),options),{-base_width/2,box.height/2}}};
    }
    throw std::runtime_error("Flowchart hand-drawn node shape is not implemented yet");
}

std::string rough_stroke_dash(const FlowchartPaint& paint) {
    auto dash=paint.get(u"stroke-dasharray","");
    std::string result;
    std::size_t tokens=0;
    std::istringstream pieces(dash);
    for(std::string word;tokens<2 && pieces>>word;++tokens) {
        double value=0;
        const auto parsed=std::from_chars(word.data(),word.data()+word.size(),value);
        if(parsed.ec!=std::errc{}||parsed.ptr!=word.data()+word.size()) value=0;
        if(tokens) result+=' ';
        result+=number(value);
    }
    return tokens==0?"0 0":tokens==1?result+" "+result:result;
}

std::string rough_flowchart_shape(const RoughPolygonResult& paths,const FlowchartPaint& paint,
    std::string_view fill,std::string_view stroke,std::string_view hatch_width,FlowchartPoint offset={},
    std::string_view stroke_width={},std::string_view css="label-container",
    bool solid_fill=false,bool invisible_stroke=false) {
    auto width=stroke_width.empty()?paint.get(u"stroke-width","1.3"):std::string(stroke_width);
    if(width.ends_with("px")) width.resize(width.size()-2);
    std::string out="<g";
    if(!css.empty()) out+=" class=\""+std::string(css)+"\"";
    if(offset.x!=0||offset.y!=0) out+=" transform=\"translate("+point(offset)+")\"";
    out+=">";
    if(!paths.fill_path.empty()) {
        out+="<path d=\""+paths.fill_path+"\"";
        if(solid_fill) out+=" fill=\""+paint.get(u"fill",fill)+"\" stroke=\"none\" stroke-width=\"0\"/>";
        else out+=" fill=\"none\" stroke=\""+paint.get(u"fill",fill)+"\" stroke-width=\""+
            std::string(hatch_width)+"\" stroke-dasharray=\"0 0\"/>";
    }
    if(!paths.stroke_path.empty()) out+="<path d=\""+paths.stroke_path+"\" fill=\"none\" stroke=\""+
        paint.get(u"stroke",stroke)+"\" stroke-width=\""+width+"\" stroke-dasharray=\""+rough_stroke_dash(paint)+"\""+
        (invisible_stroke?" stroke-opacity=\"0\"":"")+"/>";
    return out+"</g>";
}
