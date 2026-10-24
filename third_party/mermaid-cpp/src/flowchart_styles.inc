// Common classic paints from handDrawnShapeStyles::{compileStyles,styles2String}.
// Keep source declarations owned and separate from SVG presentation attributes.
struct FlowchartPaint {
    std::map<std::u16string,std::u16string> values;
    std::u16string marker_stroke;

    FlowchartLabelFont font(const FlowchartSvgTheme& theme) const {
        FlowchartLabelFont result{theme.font_family,theme.font_size};
        if(auto it=values.find(u"font-family");it!=values.end()) result.family=it->second;
        if(auto it=values.find(u"font-size");it!=values.end()) result.size=std::stod(xml(it->second));
        if(auto it=values.find(u"font-weight");it!=values.end())
            result.weight=it->second==u"bold"?700:it->second==u"normal"?400:std::stoul(xml(it->second));
        if(auto it=values.find(u"font-style");it!=values.end()) result.italic=it->second==u"italic";
        return result;
    }

    std::string get(std::u16string_view key,std::string_view fallback) const {
        auto it=values.find(std::u16string(key));
        return it==values.end()?std::string(fallback):xml(it->second);
    }
    std::string attributes(std::string_view fill,std::string_view stroke,
        std::string_view width="1",std::string_view dash="none") const {
        return " fill=\""+get(u"fill",fill)+"\" stroke=\""+get(u"stroke",stroke)+
            "\" stroke-width=\""+get(u"stroke-width",width)+"\" stroke-dasharray=\""+
            get(u"stroke-dasharray",dash)+"\"";
    }
};
std::u16string trim_style(std::u16string_view text) {
    const auto first=text.find_first_not_of(u" \t\r\n");
    if(first==text.npos) return {};
    return std::u16string(text.substr(first,text.find_last_not_of(u" \t\r\n")-first+1));
}
FlowchartPaint compile_paint(const std::vector<std::u16string>& classes,
    const std::vector<std::u16string>& direct) {
    FlowchartPaint result;
    for(const auto* list:{&classes,&direct}) for(const auto& declaration:*list) {
        const auto colon=declaration.find(u':');
        if(colon==declaration.npos) throw std::runtime_error("Invalid Flowchart style declaration");
        const auto key=trim_style(std::u16string_view(declaration).substr(0,colon));
        const auto value=trim_style(std::u16string_view(declaration).substr(colon+1));
        if(key==u"fill"||key==u"stroke"||key==u"color") {
            if(value!=u"none") theme_detail::color_adjust(value,{});
        } else if(key==u"stroke-width"||key==u"stroke-dasharray") {
            static const std::regex length(R"((?:\d+(?:\.\d*)?|\.\d+)(?:px)?)");
            static const std::regex dash(R"((?:\d+(?:\.\d*)?|\.\d+)(?:px)?(?:[ ,]+(?:\d+(?:\.\d*)?|\.\d+)(?:px)?)*)");
            const auto encoded=xml(value);
            if(!(key==u"stroke-dasharray" && value==u"none") &&
                !std::regex_match(encoded,key==u"stroke-width"?length:dash))
                throw std::runtime_error("Unsupported Flowchart stroke dimension");
        } else if(key==u"font-size") {
            static const std::regex size(R"((?:\d+(?:\.\d*)?|\.\d+)px)");
            if(!std::regex_match(xml(value),size)||std::stod(xml(value))<=0)
                throw std::runtime_error("Flowchart font-size requires a positive pixel size");
        } else if(key==u"font-weight") {
            if(value!=u"normal" && value!=u"bold" &&
                !(value.size()==3 && value[0]>=u'1' && value[0]<=u'9' && value.substr(1)==u"00"))
                throw std::runtime_error("Unsupported Flowchart font weight");
        } else if(key==u"font-style") {
            if(value!=u"normal" && value!=u"italic") throw std::runtime_error("Unsupported Flowchart font style");
        } else if(key==u"font-family") {
            if(value.empty()) throw std::runtime_error("Empty Flowchart font family");
        } else throw std::runtime_error("Flowchart SVG style property is not implemented yet: "+xml(key));
        result.values[key]=value;
        // edges.js chooses the first stroke declaration for marker cloning,
        // while the painted path uses the last declaration's CSS precedence.
        if(key==u"stroke" && result.marker_stroke.empty()) result.marker_stroke=value;
    }
    return result;
}
