module folia.mermaid.flowchart_svg;
import std;
import folia.mermaid.svg_text;
import folia.mermaid.number;
import folia.mermaid.theme_color;
import folia.mermaid.flowchart_graph;

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
#include "flowchart_shapes.inc"

FlowchartPoint intersect(const FlowchartPlacedNode& n,std::u16string_view shape,FlowchartPoint p) {
    if(shape==u"stadium") return polygon_intersection(n,stadium_points(n.width,n.height),p);
    if(shape==u"subroutine") return polygon_intersection(n,subroutine_points(n.width,n.height),p);
    if(classic_polygon(shape)) return polygon_intersection(n,classic_polygon_points(shape,n.width,n.height),p);
    const double dx=p.x-n.x,dy=p.y-n.y;
    double factor;
    if(shape==u"circle" || shape==u"doublecircle") factor=(n.width/2)/std::hypot(dx,dy);
    else if(shape==u"diamond") factor=1/(std::abs(dx)/(n.width/2)+std::abs(dy)/(n.height/2));
    else factor=1/std::max(std::abs(dx)/(n.width/2),std::abs(dy)/(n.height/2));
    if(!std::isfinite(factor)) throw std::runtime_error("Invalid Flowchart shape intersection");
    FlowchartPoint result{n.x+dx*factor,n.y+dy*factor};
    if(shape==u"cylinder") {
        const double rx=n.width/2,ry=cylinder_radius_y(n.width),x=result.x-n.x;
        if(rx!=0 && (std::abs(x)<rx || (std::abs(x)==rx && std::abs(result.y-n.y)>n.height/2-ry))) {
            double y=ry*ry*(1-x*x/(rx*rx));
            if(y>0) y=std::sqrt(y);
            y=ry-y;if(dy>0) y=-y;
            result.y+=y;
        }
    }
    return result;
}
void shorten(FlowchartPoint& p,FlowchartPoint neighbor,double amount) {
    const auto dx=neighbor.x-p.x,dy=neighbor.y-p.y,length=std::hypot(dx,dy);
    if(length>0) {p.x+=amount*dx/length;p.y+=amount*dy/length;}
}
// utils.calcLabelPosition/calculatePoint: midpoint by polyline distance, with
// the original five-decimal interpolation. This is not the curve's midpoint.
FlowchartPoint label_midpoint(std::span<const FlowchartPoint> points) {
    double distance=0;
    for(std::size_t i=1;i<points.size();++i) distance+=std::hypot(points[i].x-points[i-1].x,points[i].y-points[i-1].y);
    distance/=2;
    for(std::size_t i=1;i<points.size();++i) {
        const auto a=points[i-1],b=points[i];const double length=std::hypot(b.x-a.x,b.y-a.y);
        if(length==0) return a;
        if(length<distance) {distance-=length;continue;}
        const double ratio=distance/length;
        if(ratio<=0) return a;if(ratio>=1) return b;
        return {std::floor(((1-ratio)*a.x+ratio*b.x)*1e5+0.5)/1e5,
            std::floor(((1-ratio)*a.y+ratio*b.y)*1e5+0.5)/1e5};
    }
    return points.back();
}
// Preserve utils.isLabelCoordinateInPath's literal rounded-coordinate lookup.
// It controls whether resolveEdgeLabelPosition applies the clipping delta.
bool label_coordinate_in_path(FlowchartPoint point,const std::string& path) {
    static const std::regex decimal(R"(\d+\.\d+)");
    std::string sanitized;std::size_t cursor=0;
    for(auto it=std::sregex_iterator(path.begin(),path.end(),decimal);it!=std::sregex_iterator();++it) {
        sanitized+=path.substr(cursor,it->position()-cursor);
        sanitized+=number(std::floor(std::stod(it->str())+0.5));
        cursor=it->position()+it->length();
    }
    sanitized+=path.substr(cursor);
    return sanitized.find(number(std::floor(point.x+0.5)))!=std::string::npos ||
        sanitized.find(number(std::floor(point.y+0.5)))!=std::string::npos;
}
#include "flowchart_clusters.inc"

}

std::expected<FlowchartSvg,std::string> render_flowchart_svg(const FlowchartRenderInput& input,const FlowchartMeasurements& measured) {
    try {
        const auto& graph=input.document;const auto& theme=input.theme;
        if(measured.nodes.size()!=graph.nodes.size() || measured.edges.size()!=graph.edges.size())
            return std::unexpected("Flowchart needs one measured label per node and edge");
        for(auto v:{theme.font_size,theme.minimum_label_width,theme.wrapping_width,theme.padding,theme.radius,theme.group_title_top,theme.group_title_bottom})
            if(!std::isfinite(v)||v<0) return std::unexpected("Invalid Flowchart style dimension");
        if(theme.font_size==0||theme.font_family.empty()) return std::unexpected("Invalid Flowchart font");
        for(const auto& paint:{theme.fill,theme.border,theme.text,theme.line,theme.label_background,theme.cluster_fill,theme.cluster_border})
            theme_detail::color_adjust(std::u16string(paint.begin(),paint.end()),{});
        if(!graph.title.empty()) return std::unexpected("Flowchart title placement requires the title adapter");
        std::vector<FlowchartNodeBox> boxes;std::vector<FlowchartEdgeLabelSize> labels;
        std::unordered_map<std::u16string,std::size_t> ids;
        const auto height=[](const FlowchartTextBox& b){return b.height;};
        for(std::size_t i=0;i<graph.nodes.size();++i) {
            const auto& n=graph.nodes[i];const auto& b=measured.nodes[i];

            if(!n.styles.empty()||!n.compiled_styles.empty()) return std::unexpected("Flowchart custom SVG styles require the style adapter");
            if(!valid_box(b)) return std::unexpected("Invalid measured Flowchart label");
            if(!n.group && b.width>theme.wrapping_width && theme.wrapping_width>0) return std::unexpected("Flowchart wrapped labels require the wrapping adapter");
            const double w=n.label.empty()?0:std::max(b.width,theme.minimum_label_width),h=n.label.empty()?0:height(b);
            FlowchartNodeBox box;
            if(n.group) box={b.width,h};
            else if(n.shape==u"squareRect") box={w+4*n.padding,h+2*n.padding};
            else if(n.shape==u"roundedRect") box={w+2*n.padding,h+2*n.padding};
            else if(n.shape==u"diamond") {const double side=w+h+2*n.padding;box={side,side};}
            else if(n.shape==u"circle") {const double diameter=std::hypot(b.width,h)+n.padding;box={diameter,diameter};}
            else if(n.shape==u"stadium") box=stadium_box(w,h,n.padding);
            else if(n.shape==u"subroutine") box={w+16+n.padding,h+n.padding};
            else if(n.shape==u"cylinder") {const double width=w+n.padding;box={width,h+n.padding+3*cylinder_radius_y(width)};}
            else if(n.shape==u"doublecircle") {const double diameter=std::hypot(b.width,h)+2*n.padding+10;box={diameter,diameter};}
            else if(classic_polygon(n.shape)) {
                const double padding=n.shape==u"inv_trapezoid"?2*n.padding:n.padding;
                const double height=h+padding;
                const double extra=n.shape==u"hexagon"?height/2:n.shape==u"odd"?height/4:height;
                box={w+padding+extra,height};
            }
            else return std::unexpected("Flowchart SVG shape is not implemented yet");
            boxes.push_back(box);ids.emplace(n.id,i);
        }
        for(std::size_t i=0;i<graph.edges.size();++i) {
            const auto& e=graph.edges[i];const auto& b=measured.edges[i];
            if(!valid_box(b)) return std::unexpected("Invalid measured Flowchart edge label");
            if(!e.styles.empty()||!e.compiled_styles.empty()) return std::unexpected("Flowchart custom edge styles require the style adapter");
            if(e.curve!=u"basis" && e.curve!=u"linear" && !e.curve.empty()) return std::unexpected("Flowchart SVG curve is not implemented yet");
            for(auto arrow:{e.arrow_start,e.arrow_end}) if(arrow!=u"none" && arrow!=u"arrow_point")
                return std::unexpected("Flowchart SVG marker is not implemented yet");
            labels.push_back(e.label.empty()?FlowchartEdgeLabelSize{}:FlowchartEdgeLabelSize{b.width,height(b)});
        }
        auto prepared=prepare_flowchart_graph(graph);if(!prepared) return std::unexpected(prepared.error());
        auto scene=layout_prepared(graph,prepared->graph,boxes,labels,measured,theme);
        const auto* layout=&scene.layout;
        FlowchartSvg output;output.width=scene.max_x-scene.min_x+2*theme.padding;output.height=scene.max_y-scene.min_y+2*theme.padding;
        std::string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"flowchart\" width=\""+number(output.width)+
            "\" height=\""+number(output.height)+"\" viewBox=\""+number(scene.min_x)+" "+number(scene.min_y)+" "+number(output.width)+" "+number(output.height)+"\">";
        for(auto [tag,value]:{std::pair{"title",std::u16string_view(graph.acc_title)},{"desc",std::u16string_view(graph.acc_description)}})
            if(!value.empty()) svg+="<"+std::string(tag)+">"+xml(value)+"</"+tag+">";
        svg+="<defs><marker id=\"flow-point-end\" viewBox=\"0 0 10 10\" refX=\"5\" refY=\"5\" markerUnits=\"userSpaceOnUse\" markerWidth=\"8\" markerHeight=\"8\" orient=\"auto\"><path d=\"M0 0L10 5L0 10Z\" fill=\""+theme.line+"\"/></marker>";
        svg+="<marker id=\"flow-point-start\" viewBox=\"0 0 10 10\" refX=\"4.5\" refY=\"5\" markerUnits=\"userSpaceOnUse\" markerWidth=\"8\" markerHeight=\"8\" orient=\"auto\"><path d=\"M0 5L10 10L10 0Z\" fill=\""+theme.line+"\"/></marker></defs>";
        svg+="<g font-family=\""+xml(theme.font_family)+"\" font-size=\""+number(theme.font_size)+"\" transform=\"translate("+number(theme.padding)+","+number(theme.padding)+")\">";
        const auto text=[&](std::u16string_view,FlowchartPoint center,const FlowchartTextBox& b,std::string_view css) {
            std::string out="<text class=\""+std::string(css)+"\" text-anchor=\"middle\" fill=\""+theme.text+"\">";
            for(std::size_t i=0;i<b.lines.size();++i)
                out+="<tspan x=\""+number(center.x)+"\" y=\""+number(center.y+b.baseline-b.height/2+i*b.line_step)+"\">"+xml(b.lines[i])+"</tspan>";
            return out+"</text>";
        };
        std::vector<std::size_t> groups;
        for(std::size_t i=0;i<graph.nodes.size();++i) if(graph.nodes[i].group) groups.push_back(i);
        const auto depth=[&](std::size_t i) {
            std::size_t value=0;
            while(!graph.nodes[i].parent.empty()) {i=ids.at(graph.nodes[i].parent);++value;}
            return value;
        };
        std::stable_sort(groups.begin(),groups.end(),[&](auto a,auto b){return depth(a)<depth(b);});
        for(auto i:groups) {
            const auto& n=layout->nodes[i];const auto& source=graph.nodes[i];
            svg+="<g class=\"cluster\" data-node=\""+xml(n.id)+"\"><rect x=\""+number(n.x-n.width/2)+
                "\" y=\""+number(n.y-n.height/2)+"\" width=\""+number(n.width)+"\" height=\""+number(n.height)+
                "\" fill=\""+theme.cluster_fill+"\" stroke=\""+theme.cluster_border+"\"/>";
            svg+=text(source.label,{n.x,n.y-n.height/2+theme.group_title_top+height(measured.nodes[i])/2},
                measured.nodes[i],"cluster-label")+"</g>";
        }
        // Original getEdgesToRender paints ordinary edges first, merged loops last.
        std::vector<std::size_t> edge_order;
        for(bool loops:{false,true}) for(std::size_t i=0;i<graph.edges.size();++i)
            if((graph.edges[i].start==graph.edges[i].end)==loops && !layout->edges[i].points.empty()) edge_order.push_back(i);
        for(auto i:edge_order) {
            const auto& e=graph.edges[i];auto points=layout->edges[i].points;
            if(e.arrow_start==u"arrow_point") shorten(points.front(),points[1],4);
            if(e.arrow_end==u"arrow_point") shorten(points.back(),points[points.size()-2],4);
            const auto path=curve_path(points,e.curve==u"linear");
            svg+="<path class=\"flowchart-link\" data-edge=\""+xml(e.id)+"\" d=\""+path+"\" fill=\"none\" stroke=\""+theme.line+
                "\" stroke-width=\""+(e.stroke==u"thick"?std::string("3.5"):e.stroke==u"invisible"?std::string("0"):std::string("1"))+"\"";
            svg+=e.stroke==u"dotted"?" stroke-dasharray=\"2\"":" stroke-dasharray=\"0\"";
            if(e.arrow_start==u"arrow_point") svg+=" marker-start=\"url(#flow-point-start)\"";
            if(e.arrow_end==u"arrow_point") svg+=" marker-end=\"url(#flow-point-end)\"";
            svg+="/>";
        }
        for(auto i:edge_order) if(!graph.edges[i].label.empty() && layout->edges[i].label) {
            const auto p=*layout->edges[i].label;const auto b=labels[i];
            svg+="<g class=\"edgeLabel\"><rect x=\""+number(p.x-b.width/2)+"\" y=\""+number(p.y-b.height/2)+"\" width=\""+number(b.width)+"\" height=\""+number(b.height)+"\" fill=\""+theme.label_background+"\"/>";
            svg+=text(graph.edges[i].label,p,measured.edges[i],"edge-text")+"</g>";
        }
        for(std::size_t i=0;i<graph.nodes.size();++i) {
            const auto& n=layout->nodes[i];const auto& source=graph.nodes[i];
            if(source.group) continue;
            svg+="<g class=\"node\" data-node=\""+xml(n.id)+"\" transform=\"translate("+number(n.x)+","+number(n.y)+")\">";
            const auto paint=" fill=\""+theme.fill+"\" stroke=\""+theme.border+"\" stroke-width=\"1\"";
            if(source.shape==u"circle") svg+="<circle class=\"label-container\" r=\""+number(n.width/2)+"\""+paint+"/>";
            else if(source.shape==u"doublecircle") svg+="<g class=\"label-container\""+paint+"><circle r=\""+number(n.width/2)+"\"/><circle r=\""+number(n.width/2-5)+"\"/></g>";
            else if(source.shape==u"diamond") svg+="<polygon class=\"label-container\" points=\"0,"+number(-n.height/2)+" "+number(n.width/2)+",0 0,"+number(n.height/2)+" "+number(-n.width/2)+",0\""+paint+"/>";
            else if(source.shape==u"stadium" || source.shape==u"subroutine" || source.shape==u"cylinder") {
                const auto path=source.shape==u"cylinder"?cylinder_path(n.width,n.height):
                    polygon_path(source.shape==u"stadium"?stadium_points(n.width,n.height):subroutine_points(n.width,n.height));
                svg+="<path class=\"label-container\" d=\""+path+"\""+paint+"/>";
            }
            else if(classic_polygon(source.shape)) svg+="<path class=\"label-container\" d=\""+
                polygon_path(classic_polygon_points(source.shape,n.width,n.height))+"\""+paint+"/>";
            else svg+="<rect class=\"label-container\" x=\""+number(-n.width/2)+"\" y=\""+number(-n.height/2)+"\" width=\""+number(n.width)+"\" height=\""+number(n.height)+"\" rx=\""+number(source.shape==u"roundedRect"?theme.radius:0)+"\""+paint+"/>";
            svg+=text(source.label,{source.shape==u"odd"?n.height/8:0,source.shape==u"cylinder"?source.padding/1.5:0},measured.nodes[i],"nodeLabel")+"</g>";
        }
        output.svg=svg+"</g></svg>";return output;
    } catch(const std::exception& e) {return std::unexpected(e.what());}
}
}
