// Mermaid 12.2.0 storage shapes. MIT, see THIRD_PARTY_NOTICES.md.
// Measured dimensions, painted extents, and clipping are node-local.
bool storage_flowchart_shape(std::u16string_view shape) {
    return shape==u"datastore"||shape==u"folder"||shape==u"bucket"||shape==u"console"||
        shape==u"browser"||shape==u"st-rect"||shape==u"tag-rect"||shape==u"bow-rect";
}

std::vector<FlowchartPoint> storage_bucket_arc(double rx,double cy,double ry,bool lower) {
    constexpr std::size_t segments=12;
    std::vector<FlowchartPoint> points;
    points.reserve(segments+1);
    for(std::size_t i=0;i<=segments;++i) {
        const double theta=std::numbers::pi*static_cast<double>(lower?i:segments-i)/segments;
        points.push_back({rx*std::cos(theta),cy+(lower?1:-1)*ry*std::sin(theta)});
    }
    return points;
}

std::vector<FlowchartPoint> storage_bow_arc(FlowchartPoint first,FlowchartPoint last,
    double rx,double ry,bool clockwise) {
    constexpr std::size_t segments=20;
    const auto dx=(last.x-first.x)/2,dy=(last.y-first.y)/2;
    const auto angle=std::atan2(dy,dx);
    const auto distance=std::sqrt(std::pow(dx/rx,2)+std::pow(dy/ry,2));
    const auto factor=std::sqrt(std::max(0.0,1-distance*distance))*(clockwise?-1:1);
    const auto cx=(first.x+last.x)/2+factor*ry*std::sin(angle);
    const auto cy=(first.y+last.y)/2-factor*rx*std::cos(angle);
    const auto start=std::atan2((first.y-cy)/ry,(first.x-cx)/rx);
    const auto end=std::atan2((last.y-cy)/ry,(last.x-cx)/rx);
    auto span=end-start;
    if(clockwise&&span<0) span+=2*std::numbers::pi;
    if(!clockwise&&span>0) span-=2*std::numbers::pi;
    std::vector<FlowchartPoint> points;
    points.reserve(segments);
    for(std::size_t i=0;i<segments;++i) {
        const auto angle=start+span*static_cast<double>(i)/(segments-1);
        points.push_back({cx+rx*std::cos(angle),cy+ry*std::sin(angle)});
    }
    return points;
}

FlowchartLocalShape measure_storage_flowchart_shape(std::u16string_view shape,double lw,double lh,
    double padding,bool neo) {
    FlowchartLocalShape local;
    const auto set_box=[&](double width,double height) {
        local.box={width,height};
        local.paint_bounds={-width/2,-height/2,width/2,height/2};
    };
    const auto add_outline=[&](std::vector<FlowchartPoint> points,FlowchartPoint shift={}) {
        local.contours.push_back({polygon_path(points),shift});
        local.attachment=std::move(points);
    };
    const auto add_accent=[&](std::string path,bool fill=false,bool translucent=false) {
        local.contours.push_back({.path=std::move(path),.paint=fill?FlowchartContourPaint::accent_fill:
            FlowchartContourPaint::accent_stroke,.measured=false,.opacity=translucent?"0.6":""});
    };
    if(shape==u"datastore") {
        set_box(lw+4*padding,lh+2*padding);
        const auto w=local.box.width,h=local.box.height;
        const std::array<FlowchartPoint,4> corners{{{-w/2,-h/2},{w/2,-h/2},{w/2,h/2},{-w/2,h/2}}};
        local.contours.push_back({.path=polygon_path(corners),.dash=number(w)+" "+number(h)});
    } else if(shape==u"folder") {
        const auto w=std::max(lw+2*padding,90.0),content=lh+2*padding;
        const auto tab=std::clamp(content*.16,8.0,14.0),h=content+tab;
        const auto left=-w/2,top=-h/2,tab_right=left+std::max(w*.38,28.0);
        set_box(w,h);
        add_outline({{left,top},{tab_right,top},{tab_right,top+tab},{-left,top+tab},{-left,-top},{left,-top}});
        local.label_offset.y=tab/2;
    } else if(shape==u"bucket") {
        const auto w=std::max(lw+2*padding,80.0),rx=w/2,ry=std::clamp(w*.08,5.0,12.0);
        const auto h=lh+2*padding+ry,top=-h/2+ry,bottom=h/2,foot=rx*.72;
        set_box(w,h+ry);
        local.paint_bounds={-rx,-h/2,rx,bottom+ry};
        const auto rim=" A"+point({rx,ry})+" 0 0 0 ";
        const auto base=" A"+point({foot,ry})+" 0 0 0 ";
        const auto left=point({-rx,top}),right=point({rx,top});
        local.contours.push_back({"M"+left+" L"+point({-foot,bottom})+base+point({foot,bottom})+
            " L"+right+rim+left+" Z"});
        add_accent("M"+left+rim+right+rim+left);
        auto points=storage_bucket_arc(rx,top,ry,false);
        auto lower=storage_bucket_arc(foot,bottom,ry,true);
        points.insert(points.end(),lower.begin(),lower.end());
        local.attachment=std::move(points);
        local.label_offset.y=ry/2;
    } else if(shape==u"console"||shape==u"browser") {
        const bool browser=shape==u"browser";
        const auto band=browser?18.0:20.0;
        const auto w=std::max(lw+2*padding,90.0),h=lh+2*padding+band;
        const auto x=-w/2,y=-h/2;
        set_box(w,h);
        local.contours.push_back({rough_rounded_rect(x,y,w,h,12)});
        local.label_offset.y=band/2;
        if(browser) {
            add_accent("M"+point({x,y+band})+" L"+point({-x,y+band}));
            for(int i=0;i<3;++i) {
                const auto cx=x+12+i*9,cy=y+band/2;
                add_accent("M"+point({cx-2.5,cy})+" a2.5,2.5 0 1 0 5,0 a2.5,2.5 0 1 0 -5,0 Z",true);
            }
            add_accent(rough_rounded_rect(x+44,y+4,std::max(w-56,10.0),band-8,3),false,true);
        } else local.console_glyph=FlowchartPoint{x+12,y+16};
    } else {
        const auto px=neo?16:padding,py=neo?12:padding;
        local.flat_rough_paths=true;
        if(shape==u"st-rect") {
            const auto offset=neo?10.0:5.0,w=lw+2*px,h=lh+2*py;
            const auto x=-w/2,y=-h/2;
            set_box(w+2*offset,h+2*offset);
            add_outline({{x-offset,y+offset},{x-offset,y+h+offset},{x+w-offset,y+h+offset},
                {x+w-offset,y+h},{x+w,y+h},{x+w,y+h-offset},{x+w+offset,y+h-offset},
                {x+w+offset,y-offset},{x+offset,y-offset},{x+offset,y},{x,y},{x,y+offset}});
            const std::array<FlowchartPoint,6> inner{{{x,y+offset},{x+w-offset,y+offset},
                {x+w-offset,y+h},{x+w,y+h},{x+w,y},{x,y}}};
            local.contours.push_back({polygon_path(inner)});
            for(auto& contour:local.contours) contour.merge_flat_drawing=true;
            local.label_offset={-offset,offset};
        } else if(shape==u"tag-rect") {
            const auto w=lw+2*px,h=lh+2*py,tag=h*.2,x=-w/2,y=-h/2;
            set_box(w+tag,h);
            add_outline({{x-tag/2,y},{x+w+tag/2,y},{x+w+tag/2,-y},{x-tag/2,-y}});
            const std::array<FlowchartPoint,3> triangle{{{x+w-tag/2,-y},{x+w+tag/2,-y},
                {x+w+tag/2,-y-tag}}};
            local.contours.push_back({polygon_path(triangle),{},FlowchartContourPaint::shape,RoughFill::solid});
        } else {
            const auto w=lw+2*px,h=lh+py,ry=h/2,rx=ry/(2.5+h/50);
            const auto x=-w/2,y=-h/2;
            std::vector<FlowchartPoint> points{{-x,y},{x,y}};
            if(ry>0) {
                auto left=storage_bow_arc({x,y},{x,-y},rx,ry,false);
                points.insert(points.end(),left.begin(),left.end());
                points.push_back({-x,-y});
                auto right=storage_bow_arc({-x,-y},{-x,y},rx,ry,true);
                points.insert(points.end(),right.begin(),right.end());
            }
            const auto bounds=outline_point_bounds(points);
            local.box={bounds.width(),bounds.height()};
            local.paint_bounds={bounds.left+rx/2,bounds.top,bounds.right+rx/2,bounds.bottom};
            add_outline(std::move(points),{rx/2,0});
        }
    }
    return local;
}

void measure_storage_flat_paths(FlowchartLocalShape& local,const FlowchartPaint& paint,
    const FlowchartSvgTheme& theme) {
    std::optional<RoughBounds> measured,painted;
    for(auto& contour:local.contours) {
        contour.flat_drawing=sketch_svg_path(contour.path,{.seed=theme.hand_drawn_seed,.roughness=0,
            .fill=paint.get(u"fill",theme.fill)=="none"?RoughFill::none:RoughFill::solid,
            .stroke=paint.get(u"stroke",theme.border)!="none"});
        if(const auto& bounds=contour.flat_drawing->bounds) {
            include_outline_bounds(measured,*bounds);
            include_outline_bounds(painted,*bounds,contour.translation);
        }
    }
    if(measured) local.box={measured->width(),measured->height()};
    if(painted) local.paint_bounds=*painted;
}

RoughFlowchartNode measure_rough_storage_shape(std::u16string_view shape,FlowchartLocalShape& local,
    const FlowchartPaint& paint,const FlowchartSvgTheme& theme) {
    local.flat_rough_paths=false;
    if(shape==u"datastore") {
        auto rough=rough_flowchart_node(u"squareRect",local.box,paint,theme);
        if(const auto& box=rough.drawing.bounds) {
            const auto bounds=*box;
            local.box={bounds.width(),bounds.height()};
            rough.drawing.stroke_path.clear();
            if(paint.get(u"stroke",theme.border)!="none") {
                const RoughPolygonOptions options{.seed=theme.hand_drawn_seed,.fill=RoughFill::none};
                for(const auto y:{bounds.top,bounds.bottom}) {
                    const std::array line{RoughCommand{RoughVerb::move,{bounds.left,y}},
                        RoughCommand{RoughVerb::line,{bounds.right,y}}};
                    rough.overlays.push_back({sketch_path(line,options)});
                }
            }
        }
        return rough;
    }
    // Accent paths are ordinary SVG overlays even when the body is rough.
    auto body=local;
    std::erase_if(body.contours,flowchart_accent);
    auto result=rough_flowchart_contours(body,paint,theme);
    local.box=body.box;
    local.paint_bounds=body.paint_bounds;
    if(shape==u"browser"||shape==u"bucket"||shape==u"console") result.grouped=true;
    return result;
}
