module folia.mermaid.mindmap_svg;
import folia.mermaid.number;
import folia.mermaid.svg_text;
import std;

namespace folia::mermaid {
namespace {
std::string num(double x){auto s=format_number(x);if(!s)throw std::runtime_error("Invalid Mindmap coordinate");return *s;}
std::string xml(std::u16string_view s){auto out=escape_svg_text(s,u"Mindmap SVG");if(!out)throw std::runtime_error("Invalid Mindmap text");std::string value;for(char c:*out)if(c=='"')value+="&quot;";else value+=c;return value;}
const ConfigValue& field(const ConfigValue& value,std::u16string_view key){const auto* p=value.find(key);if(!p)throw std::runtime_error("Missing Mindmap configuration");return *p;}
double size(const ConfigValue& v){if(v.kind==ConfigKind::number)return v.number;std::string s(v.string.begin(),v.string.end());double n=0;const auto result=std::from_chars(s.data(),s.data()+s.size(),n);if(result.ec!=std::errc{})throw std::runtime_error("Invalid Mindmap font size");return n;}
std::u16string key(std::u16string_view prefix,std::size_t index){const auto s=std::to_string(index);return std::u16string(prefix)+std::u16string(s.begin(),s.end());}
struct Point {double x,y;};
Point mix(Point a,Point b,double wa,double wb,double divisor){return {(wa*a.x+wb*b.x)/divisor,(wa*a.y+wb*b.y)/divisor};}
std::string point(Point p){return num(p.x)+","+num(p.y);}
// Original createCytoscapeInstance uses default 30px ellipse geometry for its
// edge endpoints; measured boxes override layoutDimensions only. Its cloned
// renderer nodes have no intersect callback. Do not clip these a second time.
std::string edge_path(Point a,Point b) {
    const double dx=b.x-a.x,dy=b.y-a.y,length=std::sqrt(dx*dx+dy*dy);
    const Point first{a.x+15*dx/length,a.y+15*dy/length},last{b.x-15*dx/length,b.y-15*dy/length},middle{(a.x+b.x)/2,(a.y+b.y)/2};
    std::string out="M"+point(first)+"L"+point(mix(first,middle,5,1,6));
    const auto segment=[&](Point p,Point q,Point r){out+="C"+point(mix(p,q,2,1,3))+" "+point(mix(p,q,1,2,3))+" "+point({(p.x+4*q.x+r.x)/6,(p.y+4*q.y+r.y)/6});};
    segment(first,middle,last);segment(middle,last,last);return out+"L"+point(last);
}
std::string label(const FlowchartTextBox& box,const FlowchartSvgTheme& theme,std::u16string_view color) {
    const auto font=box.font.value_or(FlowchartLabelFont{theme.font_family,theme.font_size});
    std::string svg="<text class=\"mindmap-label\" xml:space=\"preserve\" text-anchor=\"middle\" font-family=\""+xml(font.family)+"\" font-size=\""+num(font.size)+"\" fill=\""+xml(color)+"\">";
    for(std::size_t i=0;i<box.lines.size();++i) {
        svg+="<tspan x=\"0\" y=\""+num(-box.height/2+box.baseline+i*box.line_step)+"\">";
        if(box.runs.empty())svg+=xml(box.lines[i]);
        else for(const auto& run:box.runs.at(i))svg+="<tspan font-weight=\""+std::string(run.strong?"700":"400")+"\" font-style=\""+(run.emphasis?"italic":"normal")+"\">"+xml(run.text)+"</tspan>";
        svg+="</tspan>";
    }
    return svg+"</text>";
}
}
std::expected<MindmapRenderInput,std::string> prepare_mindmap_render(std::u16string_view source,const ConfigSession& session,FrontendLimits limits) {
    auto doc=prepare_mindmap(source,session,limits);if(!doc)return std::unexpected(doc.error());
    try {
        const auto& config=doc->effective_config;const auto& theme=field(config,u"themeVariables");
        if(field(config,u"look").string!=u"classic")return std::unexpected("Native Mindmap requires classic look");
        if(doc->layout!=u"cose-bilkent")return std::unexpected("Native Mindmap ELK layout is not connected yet");
        MindmapRenderInput result;
        result.theme.font_family=field(theme,u"fontFamily").string;
        result.theme.font_size=size(field(theme,u"fontSize"));
        result.theme.wrapping_width=field(field(config,u"flowchart"),u"wrappingWidth").number;
        if(const auto* radius=theme.find(u"radius"))result.theme.radius=size(*radius);
        const auto* html=config.find(u"htmlLabels");result.theme.html_label_spacing=!html||html->kind==ConfigKind::null||config_truthy(*html);
        result.root_fill=field(theme,u"git0").string;result.root_text=field(theme,u"gitBranchLabel0").string;
        for(std::size_t i=0;i<12;++i){result.fills.push_back(field(theme,key(u"cScale",i)).string);result.texts.push_back(field(theme,key(u"cScaleLabel",i)).string);result.underlines.push_back(field(theme,key(u"cScaleInv",i)).string);}
        result.document=std::move(*doc);return result;
    }catch(const std::exception& e){return std::unexpected(e.what());}
}
std::expected<MindmapSvg,std::string> render_mindmap_svg(const MindmapRenderInput& input,const MindmapMeasurements& measured) {
    try {
        const auto& doc=input.document;
        if(measured.nodes.size()!=doc.nodes.size())return std::unexpected("Mindmap needs one measured label per node");
        if(!measured.node_bounds.empty()&&measured.node_bounds.size()!=doc.nodes.size())return std::unexpected("Mindmap needs one whole-node bound per node");
        std::vector<MindmapShape> shapes;std::vector<CoseTreeNode> nodes;std::vector<CoseTreeEdge> edges;
        std::map<std::u16string,std::size_t> ids;
        for(std::size_t i=0;i<doc.nodes.size();++i) {
            const auto& n=doc.nodes[i];if(!n.icon.empty())return std::unexpected("Native Mindmap icon assets are not implemented");
            auto shape=measure_mindmap_shape(n,{measured.nodes[i].width,measured.nodes[i].height},input.theme.radius);
            if(!shape)return std::unexpected(shape.error());
            if(!measured.node_bounds.empty()) {
                const auto& b=measured.node_bounds[i];
                shape->min_x=b.x;shape->min_y=b.y;shape->width=b.width;shape->height=b.height;
            }
            ids.emplace(n.id,i);nodes.push_back({n.id,shape->width,shape->height});shapes.push_back(std::move(*shape));
        }
        for(const auto& e:doc.edges)edges.push_back({e.start,e.end});
        auto layout=layout_cose_tree(nodes,edges);if(!layout)return std::unexpected(layout.error());
        double left=std::numeric_limits<double>::infinity(),top=left,right=-left,bottom=-left;
        std::string svg="<g class=\"edgePaths\">";
        for(const auto& e:doc.edges) {
            const auto& a=layout->nodes.at(ids.at(e.start));const auto& b=layout->nodes.at(ids.at(e.end));
            const int depth=e.depth+1,section=e.section.value_or(-1)+1;
            const double width=depth<11&&17-3*(depth+1)>0?17-3*(depth+1):3;
            svg+="<path id=\""+xml(e.id)+"\" class=\""+xml(e.classes)+"\" fill=\"none\" stroke=\""+xml(input.fills.at(section))+"\" stroke-width=\""+num(width)+"\" d=\""+edge_path({a.x,a.y},{b.x,b.y})+"\"/>";
        }
        svg+="</g><g class=\"nodes\">";
        for(std::size_t i=0;i<doc.nodes.size();++i) {
            const auto& n=doc.nodes[i];const auto& shape=shapes[i];const auto& position=layout->nodes[i];
            const auto section=n.section.value_or(-1)+1;
            const auto& fill=n.parent?input.fills.at(section):input.root_fill;const auto& color=n.parent?input.texts.at(section):input.root_text;
            left=std::min(left,position.x+shape.min_x);top=std::min(top,position.y+shape.min_y);
            right=std::max(right,position.x+shape.min_x+shape.width);bottom=std::max(bottom,position.y+shape.min_y+shape.height);
            svg+="<g id=\"node_"+xml(n.id)+"\" class=\""+xml(n.classes)+"\" transform=\"translate("+num(position.x)+","+num(position.y)+")\"><path class=\"label-container\" fill=\""+xml(fill)+"\" stroke=\"none\" transform=\"translate("+num(shape.translate_x)+","+num(shape.translate_y)+")\" d=\""+shape.path+"\"/>";
            if(n.type==0)svg+="<line x1=\""+num(-shape.width/2)+"\" x2=\""+num(shape.width/2)+"\" y1=\""+num(shape.height/2)+"\" y2=\""+num(shape.height/2)+"\" stroke=\""+xml(input.underlines.at(section))+"\" stroke-width=\"3\"/>";
            svg+=label(measured.nodes[i],input.theme,color)+"</g>";
        }
        svg+="</g>";const double width=right-left+2*doc.padding,height=bottom-top+2*doc.padding;
        return MindmapSvg{"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""+num(width)+"\" height=\""+num(height)+"\" viewBox=\""+num(left-doc.padding)+" "+num(top-doc.padding)+" "+num(width)+" "+num(height)+"\">"+svg+"</svg>",width,height};
    }catch(const std::exception& e){return std::unexpected(e.what());}
}
}
