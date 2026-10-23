module folia.mermaid.flowchart_svg;
import std;
import folia.mermaid.svg_text;
import folia.mermaid.number;
import folia.mermaid.theme_color;

namespace folia::mermaid {
namespace {
std::string number(double v) {auto out=format_number(v);if(!out) throw std::runtime_error("Invalid Flowchart coordinate");return *out;}
std::string xml(std::u16string_view s) {
    auto out=escape_svg_text(s,u"Flowchart text");if(!out) throw std::runtime_error("Invalid Flowchart XML text");
    std::string escaped;for(auto c:*out) {if(c=='"') escaped+="&quot;";else if(c=='\'') escaped+="&apos;";else escaped+=c;}
    return escaped;
}
bool valid_box(const FlowchartTextBox& b) {
    return std::isfinite(b.width)&&std::isfinite(b.height)&&std::isfinite(b.baseline)&&b.width>=0&&b.height>=0&&b.baseline>=0&&b.baseline<=b.height;
}
void plain_label(std::u16string_view text,std::u16string_view kind) {
    if(kind!=u"text" || text.find_first_of(u"\n\r<>")!=text.npos)
        throw std::runtime_error("Flowchart rich or multiline labels require the label adapter");
}
std::string point(FlowchartPoint p) {return number(p.x)+","+number(p.y);}
FlowchartPoint mix(FlowchartPoint a,FlowchartPoint b,double wa,double wb,double divisor) {
    return {(wa*a.x+wb*b.x)/divisor,(wa*a.y+wb*b.y)/divisor};
}
// D3 3.2.0 curveBasis with its original endpoint handling; no JS runtime.
std::string curve_path(std::span<const FlowchartPoint> points,bool linear) {
    if(points.empty()) return {};
    std::string path="M"+point(points.front());
    if(linear || points.size()<3) {for(std::size_t i=1;i<points.size();++i) path+="L"+point(points[i]);return path;}
    path+="L"+point(mix(points[0],points[1],5,1,6));
    const auto segment=[&](FlowchartPoint a,FlowchartPoint b,FlowchartPoint c) {
        path+="C"+point(mix(a,b,2,1,3))+" "+point(mix(a,b,1,2,3))+" "+
            point({(a.x+4*b.x+c.x)/6,(a.y+4*b.y+c.y)/6});
    };
    for(std::size_t i=2;i<points.size();++i) segment(points[i-2],points[i-1],points[i]);
    segment(points[points.size()-2],points.back(),points.back());return path+"L"+point(points.back());
}
FlowchartPoint intersect(const FlowchartPlacedNode& n,std::u16string_view shape,FlowchartPoint p) {
    const double dx=p.x-n.x,dy=p.y-n.y;
    double factor;
    if(shape==u"circle") factor=(n.width/2)/std::hypot(dx,dy);
    else if(shape==u"diamond") factor=1/(std::abs(dx)/(n.width/2)+std::abs(dy)/(n.height/2));
    else factor=1/std::max(std::abs(dx)/(n.width/2),std::abs(dy)/(n.height/2));
    if(!std::isfinite(factor)) throw std::runtime_error("Invalid Flowchart shape intersection");
    return {n.x+dx*factor,n.y+dy*factor};
}
void shorten(FlowchartPoint& p,FlowchartPoint neighbor,double amount) {
    const auto dx=neighbor.x-p.x,dy=neighbor.y-p.y,length=std::hypot(dx,dy);
    if(length>0) {p.x+=amount*dx/length;p.y+=amount*dy/length;}
}
}

std::expected<FlowchartSvg,std::string> render_flowchart_svg(const FlowchartRenderInput& input,const FlowchartMeasurements& measured) {
    try {
        const auto& graph=input.document;const auto& theme=input.theme;
        if(measured.nodes.size()!=graph.nodes.size() || measured.edges.size()!=graph.edges.size())
            return std::unexpected("Flowchart needs one measured label per node and edge");
        for(auto v:{theme.font_size,theme.minimum_label_width,theme.wrapping_width,theme.padding,theme.radius})
            if(!std::isfinite(v)||v<0) return std::unexpected("Invalid Flowchart style dimension");
        if(theme.font_size==0||theme.font_family.empty()) return std::unexpected("Invalid Flowchart font");
        for(const auto& paint:{theme.fill,theme.border,theme.text,theme.line,theme.label_background})
            theme_detail::color_adjust(std::u16string(paint.begin(),paint.end()),{});
        if(!graph.title.empty()) return std::unexpected("Flowchart title placement requires the title adapter");
        std::vector<FlowchartNodeBox> boxes;std::vector<FlowchartEdgeLabelSize> labels;
        std::unordered_map<std::u16string,std::size_t> ids;
        const auto height=[&](const FlowchartTextBox& b){return theme.html_label_spacing?theme.font_size*1.5:b.height;};
        for(std::size_t i=0;i<graph.nodes.size();++i) {
            const auto& n=graph.nodes[i];const auto& b=measured.nodes[i];
            if(n.group||!n.parent.empty()) return std::unexpected("Flowchart SVG clusters require the Mermaid cluster adapter");
            if(!n.styles.empty()||!n.compiled_styles.empty()) return std::unexpected("Flowchart custom SVG styles require the style adapter");
            plain_label(n.label,n.label_type);
            if(!valid_box(b)) return std::unexpected("Invalid measured Flowchart label");
            if(b.width>theme.wrapping_width && theme.wrapping_width>0) return std::unexpected("Flowchart wrapped labels require the wrapping adapter");
            const double w=n.label.empty()?0:std::max(b.width,theme.minimum_label_width),h=n.label.empty()?0:height(b);
            FlowchartNodeBox box;
            if(n.shape==u"squareRect") box={w+4*n.padding,h+2*n.padding};
            else if(n.shape==u"roundedRect") box={w+2*n.padding,h+2*n.padding};
            else if(n.shape==u"diamond") {const double side=w+h+2*n.padding;box={side,side};}
            else if(n.shape==u"circle") {const double diameter=std::hypot(b.width,h)+n.padding;box={diameter,diameter};}
            else return std::unexpected("Flowchart SVG shape is not implemented yet");
            boxes.push_back(box);ids.emplace(n.id,i);
        }
        for(std::size_t i=0;i<graph.edges.size();++i) {
            const auto& e=graph.edges[i];const auto& b=measured.edges[i];
            plain_label(e.label,e.label_type);
            if(!valid_box(b)) return std::unexpected("Invalid measured Flowchart edge label");
            if(e.start==e.end) return std::unexpected("Flowchart SVG self loops require the Mermaid self-loop adapter");
            if(!e.styles.empty()||!e.compiled_styles.empty()) return std::unexpected("Flowchart custom edge styles require the style adapter");
            if(e.curve!=u"basis" && e.curve!=u"linear" && !e.curve.empty()) return std::unexpected("Flowchart SVG curve is not implemented yet");
            for(auto arrow:{e.arrow_start,e.arrow_end}) if(arrow!=u"none" && arrow!=u"arrow_point")
                return std::unexpected("Flowchart SVG marker is not implemented yet");
            labels.push_back(e.label.empty()?FlowchartEdgeLabelSize{}:FlowchartEdgeLabelSize{b.width,height(b)});
        }
        auto layout=layout_flowchart(graph,boxes,labels);if(!layout) return std::unexpected(layout.error());
        FlowchartSvg output;output.width=layout->width+2*theme.padding;output.height=layout->height+2*theme.padding;
        std::string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"flowchart\" width=\""+number(output.width)+
            "\" height=\""+number(output.height)+"\" viewBox=\"0 0 "+number(output.width)+" "+number(output.height)+"\">";
        for(auto [tag,value]:{std::pair{"title",std::u16string_view(graph.acc_title)},{"desc",std::u16string_view(graph.acc_description)}})
            if(!value.empty()) svg+="<"+std::string(tag)+">"+xml(value)+"</"+tag+">";
        svg+="<defs><marker id=\"flow-point-end\" viewBox=\"0 0 10 10\" refX=\"5\" refY=\"5\" markerUnits=\"userSpaceOnUse\" markerWidth=\"8\" markerHeight=\"8\" orient=\"auto\"><path d=\"M0 0L10 5L0 10Z\" fill=\""+theme.line+"\"/></marker>";
        svg+="<marker id=\"flow-point-start\" viewBox=\"0 0 10 10\" refX=\"4.5\" refY=\"5\" markerUnits=\"userSpaceOnUse\" markerWidth=\"8\" markerHeight=\"8\" orient=\"auto\"><path d=\"M0 5L10 10L10 0Z\" fill=\""+theme.line+"\"/></marker></defs>";
        svg+="<g font-family=\""+xml(theme.font_family)+"\" font-size=\""+number(theme.font_size)+"\" transform=\"translate("+number(theme.padding)+","+number(theme.padding)+")\">";
        const auto text=[&](std::u16string_view label,FlowchartPoint center,const FlowchartTextBox& b,std::string_view css) {
            return "<text class=\""+std::string(css)+"\" x=\""+number(center.x)+"\" y=\""+number(center.y+b.baseline-b.height/2)+
                "\" text-anchor=\"middle\" fill=\""+theme.text+"\">"+xml(label)+"</text>";
        };
        for(std::size_t i=0;i<graph.edges.size();++i) {
            const auto& e=graph.edges[i];auto points=layout->edges[i].points;
            const auto a=ids.at(e.start),b=ids.at(e.end);
            points.front()=intersect(layout->nodes[a],graph.nodes[a].shape,points[1]);
            points.back()=intersect(layout->nodes[b],graph.nodes[b].shape,points[points.size()-2]);
            if(e.arrow_start==u"arrow_point") shorten(points.front(),points[1],4);
            if(e.arrow_end==u"arrow_point") shorten(points.back(),points[points.size()-2],4);
            svg+="<path class=\"flowchart-link\" d=\""+curve_path(points,e.curve==u"linear")+"\" fill=\"none\" stroke=\""+theme.line+
                "\" stroke-width=\""+(e.stroke==u"thick"?std::string("3.5"):e.stroke==u"invisible"?std::string("0"):std::string("1"))+"\"";
            if(e.stroke==u"dotted") svg+=" stroke-dasharray=\"2\"";
            if(e.arrow_start==u"arrow_point") svg+=" marker-start=\"url(#flow-point-start)\"";
            if(e.arrow_end==u"arrow_point") svg+=" marker-end=\"url(#flow-point-end)\"";
            svg+="/>";
        }
        for(std::size_t i=0;i<graph.edges.size();++i) if(!graph.edges[i].label.empty() && layout->edges[i].label) {
            const auto p=*layout->edges[i].label;const auto b=labels[i];
            svg+="<g class=\"edgeLabel\"><rect x=\""+number(p.x-b.width/2)+"\" y=\""+number(p.y-b.height/2)+"\" width=\""+number(b.width)+"\" height=\""+number(b.height)+"\" fill=\""+theme.label_background+"\"/>";
            svg+=text(graph.edges[i].label,p,measured.edges[i],"edge-text")+"</g>";
        }
        for(std::size_t i=0;i<graph.nodes.size();++i) {
            const auto& n=layout->nodes[i];const auto& source=graph.nodes[i];
            svg+="<g class=\"node\" data-node=\""+xml(n.id)+"\" transform=\"translate("+number(n.x)+","+number(n.y)+")\">";
            const auto paint=" fill=\""+theme.fill+"\" stroke=\""+theme.border+"\" stroke-width=\"1\"";
            if(source.shape==u"circle") svg+="<circle class=\"label-container\" r=\""+number(n.width/2)+"\""+paint+"/>";
            else if(source.shape==u"diamond") svg+="<polygon class=\"label-container\" points=\"0,"+number(-n.height/2)+" "+number(n.width/2)+",0 0,"+number(n.height/2)+" "+number(-n.width/2)+",0\""+paint+"/>";
            else svg+="<rect class=\"label-container\" x=\""+number(-n.width/2)+"\" y=\""+number(-n.height/2)+"\" width=\""+number(n.width)+"\" height=\""+number(n.height)+"\" rx=\""+number(source.shape==u"roundedRect"?theme.radius:0)+"\""+paint+"/>";
            svg+=text(source.label,{0,0},measured.nodes[i],"nodeLabel")+"</g>";
        }
        output.svg=svg+"</g></svg>";return output;
    } catch(const std::exception& e) {return std::unexpected(e.what());}
}
}
