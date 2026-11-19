// Shared Mermaid 12.2.0 point/circle/cross markers. MIT.
std::string_view marker_name(std::u16string_view arrow) {
    if(arrow==u"none") return {};
    if(arrow==u"arrow_point") return "point";
    if(arrow==u"arrow_circle") return "circle";
    if(arrow==u"arrow_cross") return "cross";
    throw std::runtime_error("Flowchart SVG marker is not implemented yet");
}
// Original markers.js classic geometry. Circle/cross use their refX positions;
// lineWithOffset shortens only point arrows in this look, not the neo offsets.
std::string marker_definitions(std::string_view color,std::string_view prefix="flow",
    std::string_view stroke={}) {
    if(stroke.empty()) stroke=color;
    std::string out="<defs>";
    for(auto type:{"point","circle","cross"}) for(bool start:{false,true}) {
        const std::string_view name=type;
        const bool point=name=="point",cross=name=="cross";
        out+="<marker id=\""+std::string(prefix)+"-"+std::string(name)+(start?"-start":"-end")+
            "\" viewBox=\""+(cross?"0 0 11 11":"0 0 10 10")+
            "\" refX=\""+(point?(start?"4.5":"5"):(start?"-1":cross?"12":"11"))+
            "\" refY=\""+(cross?"5.2":"5")+"\" markerUnits=\"userSpaceOnUse\" markerWidth=\""+
            (point?"8":"11")+"\" markerHeight=\""+(point?"8":"11")+"\" orient=\"auto\">";
        if(name=="circle") out+="<circle cx=\"5\" cy=\"5\" r=\"5\"";
        else out+="<path d=\""+std::string(cross?"M 1,1 l 9,9 M 10,1 l -9,9":
            start?"M 0 5 L 10 10 L 10 0 z":"M 0 0 L 10 5 L 0 10 z")+"\"";
        out+=" fill=\""+std::string(point?stroke:color)+"\" stroke=\""+std::string(stroke)+
            "\" stroke-width=\""+(cross?"2":"1")+"\" stroke-dasharray=\"1,0\"/></marker>";
    }
    return out+"</defs>";
}
