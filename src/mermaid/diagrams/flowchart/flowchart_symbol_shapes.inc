// Mermaid 12.2.0 built-in symbol handlers: bang, text, stateStart/stateEnd,
// forkJoin, hourglass, lightningBolt, filledCircle and crossedCircle. MIT.
// Symbols that do not call labelHelper retain their fixed geometry even when
// the source carries text. The drawing, layout box and clipping rule stay
// separate: in particular forkJoin adds layout padding after measuring its bar.
bool symbol_flowchart_shape(std::u16string_view shape) {
    return shape==u"bang"||shape==u"text"||shape==u"sm-circ"||shape==u"fr-circ"||
        shape==u"fork"||shape==u"hourglass"||shape==u"bolt"||shape==u"f-circ"||shape==u"cross-circ";
}

bool symbol_has_label(std::u16string_view shape) {
    return shape!=u"sm-circ"&&shape!=u"fr-circ"&&shape!=u"fork"&&shape!=u"hourglass"&&
        shape!=u"bolt"&&shape!=u"f-circ"&&shape!=u"cross-circ";
}

std::string symbol_circle_path(double radius) {
    const auto arc="A"+number(radius)+","+number(radius)+" 0 1 0 ";
    return "M"+point({-radius,0})+arc+point({radius,0})+arc+point({-radius,0})+"Z";
}

FlowchartLocalShape bang_shape(double label_width,double label_height,double padding) {
    const double nominal_width=label_width+5*padding;
    const double width=std::max(nominal_width,label_width+20);
    const double height=std::max(label_height+4*padding,label_height+20);
    const double radius=.15*nominal_width;
    struct Arc {double radius;FlowchartPoint delta;};
    const Arc arcs[]={
        {radius,{.25*width,-.1*height}},{radius,{.25*width,0}},
        {radius,{.25*width,0}},{radius,{.25*width,.1*height}},
        {radius,{.15*width,.33*height}},{.8*radius,{0,.34*height}},
        {radius,{-.15*width,.33*height}},
        {radius,{-.25*width,.15*height}},{radius,{-.25*width,0}},
        {radius,{-.25*width,0}},{radius,{-.25*width,-.15*height}},
        {radius,{-.1*width,-.33*height}},{.8*radius,{0,-.34*height}},
        {radius,{.1*width,-.33*height}},
    };
    FlowchartLocalShape result;
    RoughBounds bounds{};
    FlowchartPoint from{};
    std::string path="M0 0";
    for(const auto& arc:arcs) {
        const FlowchartPoint to{from.x+arc.delta.x,from.y+arc.delta.y};
        // Keep the same arc in absolute coordinates for the shared RoughJS
        // path API, whose input contract is already normalized SVG commands.
        path+="A"+number(arc.radius)+","+number(arc.radius)+" 1 0,0 "+point(to);
        short_svg_arc_bounds(bounds,from,to,arc.radius,arc.radius,std::numbers::pi/180,false);
        from=to;
    }
    path+="H0V0Z";
    const FlowchartPoint translation{-width/2,-height/2};
    result.contours.push_back({std::move(path),translation});
    result.paint_bounds={bounds.left+translation.x,bounds.top+translation.y,
        bounds.right+translation.x,bounds.bottom+translation.y};
    result.box={result.paint_bounds.width(),result.paint_bounds.height()};
    return result;
}

FlowchartLocalShape measure_symbol_shape(std::u16string_view shape,double label_width,
    double label_height,double padding,std::u16string_view direction,double state_padding,
    const FlowchartPaint& paint,const FlowchartSvgTheme& theme) {
    if(shape==u"text") {
        const double width=label_width+padding,height=label_height+padding;
        FlowchartLocalShape result{{width,height},{-width/2,-height/2,width/2,height/2}};
        result.contours.push_back({rough_rounded_rect(-width/2,-height/2,width,height,0)});
        return result;
    }
    auto result=shape==u"bang"?bang_shape(label_width,label_height,padding):FlowchartLocalShape{};
    RoughPolygonOptions options{.seed=theme.hand_drawn_seed,
        .roughness=theme.hand_drawn?.7:0,
        .fill=theme.hand_drawn?RoughFill::hachure:RoughFill::solid};
    if(paint.get(u"fill",theme.fill)=="none") options.fill=RoughFill::none;
    options.stroke=paint.get(u"stroke",theme.border)!="none";
    if(shape==u"bang") {
        if(!theme.hand_drawn) return result;
        auto& contour=result.contours.front();
        contour.flat_drawing=sketch_svg_path(contour.path,options);
    } else if(shape==u"sm-circ"||shape==u"fr-circ"||shape==u"f-circ"||shape==u"cross-circ") {
        const double radius=shape==u"cross-circ"?30:7;
        result.attachment=FlowchartCircleAttachment{shape==u"sm-circ"||shape==u"fr-circ"?0:radius};
        result.contours.push_back({symbol_circle_path(radius)});
        if(shape==u"sm-circ"&&!theme.hand_drawn) {
            result.box={14,14};result.paint_bounds={-7,-7,7,7};return result;
        }
        if(shape==u"sm-circ") {
            options.hachure_angle=120;
            options.fill=RoughFill::hachure;options.stroke=true;
        } else if(shape==u"f-circ"&&options.fill!=RoughFill::none) options.fill=RoughFill::solid;
        result.contours.front().flat_drawing=sketch_ellipse({},2*radius,2*radius,options);
        if(shape==u"fr-circ") {
            auto& inner=result.contours.emplace_back(symbol_circle_path(2.5));
            options.fill=RoughFill::solid;options.stroke=true;
            inner.flat_drawing=sketch_ellipse({},5,5,options);
        } else if(shape==u"cross-circ") {
            const auto diagonal=radius/std::sqrt(2.0);
            const auto path="M"+point({-diagonal,diagonal})+"L"+point({diagonal,-diagonal})+
                "M"+point({diagonal,diagonal})+"L"+point({-diagonal,-diagonal});
            auto& cross=result.contours.emplace_back(path);
            cross.flat_drawing=sketch_svg_path(path,options);
        }
    } else if(shape==u"fork") {
        const bool horizontal=direction==u"LR"||direction==u"RL";
        const double width=horizontal?10:70,height=horizontal?70:10;
        auto& contour=result.contours.emplace_back(
            rough_rounded_rect(-width/2,-height/2,width,height,0));
        // userNodeOverrides is called with explicit fill/stroke overrides.
        options.fill=theme.hand_drawn?RoughFill::hachure:RoughFill::solid;
        options.stroke=true;
        contour.flat_drawing=sketch_rectangle(-width/2,-height/2,width,height,options);
    } else {
        std::vector<FlowchartPoint> points;
        FlowchartPoint translation;
        if(shape==u"hourglass") {
            points={{0,0},{30,0},{0,30},{30,30}};translation={-15,-15};
        } else {
            points={{35,0},{0,38.5},{21,38.5},{0,70},{35,31.5},{14,31.5}};
            translation={-17.5,-35};
        }
        result.attachment=points;
        auto& contour=result.contours.emplace_back(polygon_path(points),translation);
        contour.flat_drawing=sketch_svg_path(contour.path,options);
    }
    std::optional<RoughBounds> bounds;
    for(const auto& contour:result.contours) if(contour.flat_drawing&&contour.flat_drawing->bounds)
        include_outline_bounds(bounds,*contour.flat_drawing->bounds,contour.translation);
    if(bounds) {
        result.paint_bounds=*bounds;result.box={bounds->width(),bounds->height()};
    }
    if(shape==u"fork") {
        result.box.width+=state_padding/2;result.box.height+=state_padding/2;
    }
    return result;
}

std::string paint_symbol_shape(std::u16string_view name,const FlowchartLocalShape& shape,
    const FlowchartPaint& source_paint,const FlowchartSvgTheme& theme,std::string_view border,
    std::string_view stroke_width,std::string_view shadow) {
    auto paint=source_paint;
    std::string_view fill=theme.fill;
    if(name==u"sm-circ") paint={}; // stateStart does not compile node declarations.
    if(name==u"text") {fill="none";stroke_width="0";}
    if(name==u"f-circ") {
        const bool node_styles=std::ranges::any_of(paint.values,[](const auto& item) {
            return item.first==u"fill"||item.first==u"stroke"||item.first==u"stroke-width"||
                item.first==u"stroke-dasharray";
        });
        // The handler's inline fill is replaced by the complete style string,
        // rather than merged property by property, in the classic look.
        if(theme.hand_drawn||!node_styles) fill=theme.border;
        if(theme.hand_drawn) paint.values.erase(u"fill");
    }
    if(name==u"fork"&&theme.hand_drawn) {
        fill=theme.line;border=theme.line;
        paint.values.erase(u"fill");paint.values.erase(u"stroke");
    }
    // stateStart/stateEnd use `node default`, including in the hand-drawn look;
    // the normal Flowchart CSS therefore applies to both RoughJS paths.
    const bool rough=theme.hand_drawn&&name!=u"sm-circ"&&name!=u"fr-circ"&&name!=u"text";
    std::string svg="<g class=\"label-container\""+std::string(shadow)+">";
    for(const auto& contour:shape.contours) {
        if(rough&&contour.flat_drawing) {
            svg+=rough_flowchart_shape(*contour.flat_drawing,paint,fill,border,"1.5",
                contour.translation,{},"",name==u"f-circ");
            continue;
        }
        const auto append=[&](std::string_view path) {
            if(path.empty()) return;
            svg+="<path d=\""+std::string(path)+"\"";
            if(contour.translation.x||contour.translation.y)
                svg+=" transform=\"translate("+point(contour.translation)+")\"";
            svg+=paint.attributes(fill,border,stroke_width)+"/>";
        };
        if(contour.flat_drawing) {
            append(contour.flat_drawing->fill_path);append(contour.flat_drawing->stroke_path);
        } else append(contour.path);
    }
    return svg+"</g>";
}
