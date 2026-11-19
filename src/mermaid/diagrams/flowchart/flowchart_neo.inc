// Mermaid 12.2.0 insertLookDefs, marker margin geometry and edge dash cutouts. MIT.
struct NeoShadow {
    std::string dx,dy,deviation,color,opacity="1";
    bool preset=false;
    std::string attribute() const {
        return color.empty()?std::string{}:" filter=\"url(#flow-drop-shadow)\"";
    }
};

NeoShadow neo_shadow(const FlowchartSvgTheme& theme) {
    if(theme.drop_shadow.empty() || theme.drop_shadow=="none") return {};
    if(theme.drop_shadow=="url(#drop-shadow)")
        return {"4","4","0",theme.dark_shadow?"#FFFFFF":"#000000","0.06",true};
    if(theme.drop_shadow.size()>4096)
        throw std::runtime_error("Flowchart Neo drop shadow exceeds the style limit");
    static const std::regex shadow(R"(\s*drop-shadow\(\s*(-?(?:[0-9]+(?:\.[0-9]+)?|\.[0-9]+))px\s+(-?(?:[0-9]+(?:\.[0-9]+)?|\.[0-9]+))px\s+((?:[0-9]+(?:\.[0-9]+)?|\.[0-9]+))px\s+(.+?)\s*\)\s*;?\s*)");
    std::smatch match;
    if(!std::regex_match(theme.drop_shadow,match,shadow))
        throw std::runtime_error("Unsupported Flowchart Neo drop shadow");
    const auto color=match[4].str();
    theme_detail::color_adjust(std::u16string(color.begin(),color.end()),{});
    return {match[1].str(),match[2].str(),match[3].str(),color};
}

std::string neo_look_definitions(const FlowchartSvgTheme& theme,const NeoShadow& shadow) {
    std::string definitions;
    if(theme.use_gradient) {
        for(const auto& paint:{theme.gradient_start,theme.gradient_stop})
            theme_detail::color_adjust(std::u16string(paint.begin(),paint.end()),{});
        definitions+="<linearGradient id=\"flow-gradient\" gradientUnits=\"objectBoundingBox\" x1=\"0%\" y1=\"0%\" x2=\"100%\" y2=\"0%\">"
            "<stop offset=\"0%\" stop-color=\""+theme.gradient_start+"\" stop-opacity=\"1\"/>"
            "<stop offset=\"100%\" stop-color=\""+theme.gradient_stop+"\" stop-opacity=\"1\"/></linearGradient>";
    }
    if(!shadow.color.empty()) {
        const auto bounds=shadow.preset?" width=\"130%\" height=\"130%\"":" x=\"-50%\" y=\"-50%\" width=\"200%\" height=\"200%\"";
        definitions+="<filter id=\"flow-drop-shadow\""+std::string(bounds)+"><feDropShadow dx=\""+shadow.dx+
            "\" dy=\""+shadow.dy+"\" stdDeviation=\""+shadow.deviation+"\" flood-color=\""+shadow.color+
            "\" flood-opacity=\""+shadow.opacity+"\"/></filter>";
    }
    return definitions.empty()?std::string{}:"<defs>"+definitions+"</defs>";
}

std::string neo_marker_definitions(std::string_view color,std::string_view prefix="flow",
    std::string_view custom_stroke={}) {
    struct Marker {
        std::string_view id,view,ref_x,ref_y,width,height,shape,stroke_width,dash;
    };
    constexpr Marker markers[]={
        {"point-start","0 0 11.5 14","1","7","11.5","14","<polygon points=\"0,7 11.5,14 11.5,0\"","0","1,0"},
        {"point-end","0 0 11.5 14","11.5","7","10.5","14","<path d=\"M 0 0 L 11.5 7 L 0 14 z\"","0","1,0"},
        {"circle-start","0 0 10 10","-2","5","14","14","<circle cx=\"5\" cy=\"5\" r=\"5\"","0","1,0"},
        {"circle-end","0 0 10 10","12.25","5","14","14","<circle cx=\"5\" cy=\"5\" r=\"5\"","0","1,0"},
        {"cross-start","0 0 15 15","-3.5","7.5","12","12","<path d=\"M 1,1 L 14,14 M 1,14 L 14,1\"","2.5","1,0"},
        {"cross-end","0 0 15 15","17.7","7.5","12","12","<path d=\"M 1,1 L 14,14 M 1,14 L 14,1\"","2.5","none"},
    };
    std::string out="<defs>";
    for(const auto& marker:markers) {
        // The original colored-marker clone targets path/circle/line, so its
        // start-point polygon inherits the ordinary marker color.
        const auto stroke=custom_stroke.empty() || marker.id=="point-start"?color:custom_stroke;
        const auto fill=marker.id=="point-end"?stroke:color;
        out+="<marker id=\"";out+=prefix;out+="-";out+=marker.id;
        out+="\" viewBox=\"";out+=marker.view;out+="\" refX=\"";out+=marker.ref_x;
        out+="\" refY=\"";out+=marker.ref_y;out+="\" markerUnits=\"userSpaceOnUse\" markerWidth=\"";out+=marker.width;
        out+="\" markerHeight=\"";out+=marker.height;out+="\" orient=\"auto\">";out+=marker.shape;
        out+=" fill=\"";out+=fill;out+="\" stroke=\"";out+=stroke;out+="\" stroke-width=\"";out+=marker.stroke_width;
        out+="\" stroke-dasharray=\"";out+=marker.dash;out+="\"/></marker>";
    }
    return out+"</defs>";
}

std::string neo_edge_dash(const FlowchartEdge& edge,double length) {
    const auto offset=[](std::u16string_view arrow) {
        if(arrow==u"arrow_point") return 4.0;
        if(arrow==u"arrow_cross" || arrow==u"arrow_circle") return 12.5;
        return 0.0;
    };
    const auto start=offset(edge.arrow_start),end=offset(edge.arrow_end);
    const auto middle=length-start-end;
    if(middle<0 && edge.stroke!=u"dotted") return "0";
    std::string dash="0 "+number(start);
    if(edge.stroke==u"dotted") {
        const auto pairs=std::floor(std::max(0.0,middle)/4);
        if(pairs>65536) throw std::runtime_error("Flowchart Neo dash array limit exceeded");
        for(std::size_t i=0;i<static_cast<std::size_t>(pairs);++i) dash+=" 2 2";
    } else dash+=" "+number(middle);
    return dash+" "+number(end);
}
