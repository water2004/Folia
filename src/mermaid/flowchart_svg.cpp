module folia.mermaid.flowchart_svg;
import std;
import folia.mermaid.svg_text;
import folia.mermaid.number;
import folia.mermaid.theme_color;
import folia.mermaid.flowchart_graph;
import folia.mermaid.flowchart_elk;
import folia.mermaid.rough_polygon;

namespace folia::mermaid {
namespace {
std::string number(double v) {auto out=format_number(v);if(!out) throw std::runtime_error("Invalid Flowchart coordinate");return *out;}
std::string xml(std::u16string_view s) {
    auto out=escape_svg_text(s,u"Flowchart text");if(!out) throw std::runtime_error("Invalid Flowchart XML text");
    std::string escaped;for(auto c:*out) {if(c=='"') escaped+="&quot;";else if(c=='\'') escaped+="&apos;";else escaped+=c;}
    return escaped;
}
#include "flowchart_styles.inc"
bool valid_box(const FlowchartTextBox& b) {
    return std::isfinite(b.width)&&std::isfinite(b.height)&&std::isfinite(b.baseline)&&b.width>=0&&b.height>=0&&b.baseline>=0&&b.baseline<=b.height;
}
std::string point(FlowchartPoint p) {return number(p.x)+","+number(p.y);}
#include "flowchart_shapes.inc"
#include "flowchart_shape_geometry.inc"
#include "flowchart_extra_shapes.inc"
#include "flowchart_sampled_shape_math.inc"
#include "flowchart_lined_document.inc"
#include "flowchart_organic_shapes.inc"
#include "flowchart_document_shapes.inc"

FlowchartPoint intersect(const FlowchartPlacedNode& n,std::u16string_view shape,FlowchartPoint p,bool neo=false,
    const FlowchartLocalShape* local=nullptr) {
    if(local) if(const auto* polygon=std::get_if<std::vector<FlowchartPoint>>(&local->attachment))
        return polygon_intersection(n,*polygon,p);
    if(shape==u"stadium") return polygon_intersection(n,stadium_points(n.width,n.height),p);
    if(shape==u"subroutine") return polygon_intersection(n,subroutine_points(n.width,n.height),p);
    if(shape==u"lin-doc") return polygon_intersection(n,lined_document_points(n.width,n.height,neo),p);
    if(classic_polygon(shape)) return polygon_intersection(n,classic_polygon_points(shape,n.width,n.height,neo),p);
    if(!local&&extra_flowchart_shape(shape)&&shape!=u"lin-rect"&&shape!=u"div-rect") {
        return polygon_intersection(n,extra_shape_outline(shape,n.width,n.height),p);
    }
    const double dx=p.x-n.x,dy=p.y-n.y;
    double factor;
    if(shape==u"circle" || shape==u"doublecircle") factor=(n.width/2)/std::hypot(dx,dy);
    else if(shape==u"diamond") factor=1/(std::abs(dx)/(n.width/2)+std::abs(dy)/(n.height/2));
    else factor=1/std::max(std::abs(dx)/(n.width/2),std::abs(dy)/(n.height/2));
    if(!std::isfinite(factor)) throw std::runtime_error("Invalid Flowchart shape intersection");
    FlowchartPoint result{n.x+dx*factor,n.y+dy*factor};
    if(local) if(const auto* cylinder=std::get_if<FlowchartCylinderAttachment>(&local->attachment))
        return apply_cylinder_attachment(*cylinder,n,p,result);
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
#include "flowchart_elk_paint.inc"

void shorten(FlowchartPoint& p,FlowchartPoint neighbor,double amount) {
    const auto dx=neighbor.x-p.x,dy=neighbor.y-p.y,length=std::hypot(dx,dy);
    if(length>0) {p.x+=amount*dx/length;p.y+=amount*dy/length;}
}
#include "flowchart_curves.inc"
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
#include "flowchart_rough.inc"
#include "flowchart_clusters.inc"
#include "flowchart_neo.inc"

}

std::expected<FlowchartLabelFont,std::string> flowchart_label_font(
    const std::vector<std::u16string>& classes,const std::vector<std::u16string>& direct,
    const FlowchartSvgTheme& theme) {
    try {return compile_paint(classes,direct).font(theme);}
    catch(const std::exception& e) {return std::unexpected(e.what());}
}

std::expected<FlowchartScene,std::string> layout_measured_flowchart(
    const FlowchartInput& graph,std::span<const FlowchartNodeBox> boxes,
    std::span<const FlowchartEdgeLabelSize> labels,const FlowchartMeasurements& measured,const FlowchartSvgTheme& theme) {
    try {
        auto prepared=prepare_flowchart_graph(graph);if(!prepared) return std::unexpected(prepared.error());
        auto labels_for_layout=measured;
        return layout_prepared(graph,prepared->graph,boxes,labels,labels_for_layout,theme);
    } catch(const std::exception& e) {return std::unexpected(e.what());}
}

std::expected<FlowchartSvg,std::string> render_flowchart_svg(const FlowchartRenderInput& input,const FlowchartMeasurements& measurements) {
    try {
        auto measured=measurements;
        auto graph=input.document;const auto& theme=input.theme;
        const auto selected_layout=graph.effective_config.find(u"layout");
        const bool use_elk=selected_layout&&selected_layout->string==u"elk";
        for(auto& node:graph.nodes) if(!node.group) {
            if(const auto shape=flowchart_classic_shape(node.shape);!shape.empty())node.shape=shape;
        }
        if(measured.nodes.size()!=graph.nodes.size() || measured.edges.size()!=graph.edges.size())
            return std::unexpected("Flowchart needs one measured label per node and edge");
        for(auto v:{theme.font_size,theme.minimum_label_width,theme.wrapping_width,theme.padding,theme.radius,theme.group_title_top,theme.group_title_bottom,theme.node_stroke_width})
            if(!std::isfinite(v)||v<0) return std::unexpected("Invalid Flowchart style dimension");
        if(theme.font_size==0||theme.font_family.empty()) return std::unexpected("Invalid Flowchart font");
        for(const auto& paint:{theme.fill,theme.border,theme.text,theme.line,theme.label_background,theme.cluster_fill,theme.cluster_border,theme.cluster_text})
            theme_detail::color_adjust(std::u16string(paint.begin(),paint.end()),{});
        if(!graph.title.empty()) {
            if(!measured.title || !valid_box(*measured.title) || !std::isfinite(theme.title_top_margin))
                return std::unexpected("Flowchart needs valid measured title metrics and margin");
            theme_detail::color_adjust(std::u16string(theme.title_color.begin(),theme.title_color.end()),{});
        }
        std::vector<FlowchartNodeBox> boxes;std::vector<FlowchartEdgeLabelSize> labels;
        std::vector<FlowchartPaint> node_paints,edge_paints;
        std::vector<std::optional<FlowchartLocalShape>> local_shapes(graph.nodes.size());
        std::vector<RoughFlowchartNode> rough_nodes;
        if(theme.hand_drawn) rough_nodes.resize(graph.nodes.size());
        std::unordered_map<std::u16string,std::size_t> ids;
        const auto height=[](const FlowchartTextBox& b){return b.height;};
        for(std::size_t i=0;i<graph.nodes.size();++i) {
            const auto& n=graph.nodes[i];const auto& b=measured.nodes[i];

            node_paints.push_back(compile_paint(n.compiled_styles,n.styles));
            if(!valid_box(b)) return std::unexpected("Invalid measured Flowchart label");
            const double w=n.label.empty()?0:std::max(b.width,theme.minimum_label_width),h=n.label.empty()?0:height(b);
            FlowchartNodeBox box;
            if(n.group) box={b.width,h};
            else if(n.shape==u"squareRect") box={w+(theme.neo?32:4*n.padding),h+(theme.neo?24:2*n.padding)};
            else if(n.shape==u"roundedRect") box={w+2*n.padding,h+2*n.padding};
            else if(n.shape==u"diamond") {const double side=w+h+2*n.padding;box={side,side};}
            else if(n.shape==u"circle") {const double diameter=std::hypot(b.width,h)+(theme.neo?32:n.padding);box={diameter,diameter};}
            else if(n.shape==u"stadium") box=stadium_box(w,h,theme.neo?40:n.padding,theme.neo?24:n.padding);
            else if(n.shape==u"subroutine") box={w+16+(theme.neo?28:n.padding),h+(theme.neo?12:n.padding)};
            else if(n.shape==u"lin-doc") {const double px=theme.neo?16:n.padding,py=theme.neo?12:n.padding;box={(w+2*px)*1.1,(h+2*py)*(theme.neo?1.5:1.25)};}
            else if(extra_flowchart_shape(n.shape)) {
                box=extra_shape_box(n.shape,w,h,n.padding,theme.neo);
                auto& local=local_shapes[i].emplace();
                local.box=box;
                local.paint_bounds={-box.width/2,-box.height/2,box.width/2,box.height/2};
                local.label_offset=extra_shape_label_offset(n.shape,box,w,h,n.padding,theme.html_label_spacing);
                local.contours.push_back({extra_shape_path(n.shape,box.width,box.height)});
                if(n.shape!=u"lin-rect"&&n.shape!=u"div-rect")
                    local.attachment=extra_shape_outline(n.shape,box.width,box.height);
            }
            else if(organic_flowchart_shape(n.shape)) {
                local_shapes[i]=measure_organic_shape(n.shape,w,h,n.padding);
                box=local_shapes[i]->box;
            }
            else if(document_flowchart_shape(n.shape)) {
                auto& local=local_shapes[i].emplace(
                    measure_document_flowchart_shape(n.shape,w,h,n.padding,theme.neo,theme.hand_drawn));
                if(theme.hand_drawn)
                    rough_nodes[i]=rough_flowchart_contours(local,node_paints[i],theme);
                box=local.box;
            }
            else if(n.shape==u"cylinder") {const double padding=theme.neo?24:n.padding;const double width=w+padding;box={width,h+padding+3*cylinder_radius_y(width)};}
            else if(n.shape==u"doublecircle") {const double diameter=std::hypot(b.width,h)+(theme.neo?56:2*n.padding+10);box={diameter,diameter};}
            else if(classic_polygon(n.shape)) {
                const double padding=n.shape==u"inv_trapezoid"?2*n.padding:n.padding;
                const double height=h+padding;
                const double extra=n.shape==u"hexagon"?height/2:n.shape==u"odd"?height/4:height;
                if(!theme.neo) box={w+padding+extra,height};
                else if(n.shape==u"hexagon") box={w+32+2*(h+70)/3.5,h+70};
                else if(n.shape==u"odd") box={w+42+(h+24)/4,h+24};
                else box={w+2*padding+extra,height};
            }
            else return std::unexpected("Flowchart SVG shape is not implemented yet");
            if(theme.hand_drawn && !n.group && !document_flowchart_shape(n.shape)) {
                rough_nodes[i]=rough_flowchart_node(n.shape,box,node_paints[i],theme);
                if(const auto& bounds=rough_nodes[i].drawing.bounds)
                    box={bounds->width(),bounds->height()};
            }
            boxes.push_back(box);ids.emplace(n.id,i);
        }
        for(std::size_t i=0;i<graph.edges.size();++i) {
            const auto& e=graph.edges[i];const auto& b=measured.edges[i];
            if(!valid_box(b)) return std::unexpected("Invalid measured Flowchart edge label");
            edge_paints.push_back(compile_paint(e.compiled_styles,e.styles));
            for(auto arrow:{e.arrow_start,e.arrow_end}) marker_name(arrow);
            labels.push_back(e.label.empty()?FlowchartEdgeLabelSize{}:FlowchartEdgeLabelSize{b.width,height(b)});
        }
        FlowchartScene scene;
        if(use_elk) {
            auto placed=layout_measured_elk_flowchart(graph,boxes,labels,measured,theme);
            if(!placed)return std::unexpected(placed.error());scene=std::move(*placed);
            for(std::size_t i=0;i<graph.edges.size();++i) {
                auto& points=scene.layout.edges[i].points;const auto& edge=graph.edges[i];
                const auto a=ids.at(edge.start),b=ids.at(edge.end);
                if(graph.nodes[a].group||graph.nodes[b].group)
                    return std::unexpected("ELK Flowchart group endpoints require frame clipping");
                clip_elk_node_endpoints(points,scene.layout.nodes[a],scene.layout.nodes[b],graph.nodes[a].shape,graph.nodes[b].shape,
                    [&](const auto& node,auto shape,auto point) {
                        return intersect(node,shape,point,theme.neo,find_local_shape(local_shapes,ids.at(node.id)));
                    },theme.hand_drawn);
                if(edge.arrow_end==u"arrow_point")elk_remove_short_marker_segment(points,scene.layout.nodes[b],false);
                if(edge.arrow_start==u"arrow_point")elk_remove_short_marker_segment(points,scene.layout.nodes[a],true);
            }
        }else {
            auto prepared=prepare_flowchart_graph(graph);if(!prepared)return std::unexpected(prepared.error());
            scene=layout_prepared(graph,prepared->graph,boxes,labels,measured,theme,nullptr,
                theme.hand_drawn?&rough_nodes:nullptr,local_shapes);
        }
        const auto* layout=&scene.layout;
        const auto edge_path=[&](std::size_t i,double* length=nullptr) {
            const auto& e=graph.edges[i];
            return flowchart_edge_path(layout->edges[i].points,use_elk?u"rounded":std::u16string_view(e.curve),e,length);
        };
        std::vector<RoughPolygonResult> rough_edges;
        if(theme.hand_drawn) {
            rough_edges.resize(graph.edges.size());
            scene.min_x=scene.min_y=std::numeric_limits<double>::infinity();
            scene.max_x=scene.max_y=-std::numeric_limits<double>::infinity();
            const auto include=[&](const RoughBounds& bounds,double x=0,double y=0) {
                scene.min_x=std::min(scene.min_x,x+bounds.left);
                scene.min_y=std::min(scene.min_y,y+bounds.top);
                scene.max_x=std::max(scene.max_x,x+bounds.right);
                scene.max_y=std::max(scene.max_y,y+bounds.bottom);
            };
            for(std::size_t i=0;i<graph.nodes.size();++i) {
                const auto& n=layout->nodes[i];
                auto& rough=rough_nodes[i];
                if(graph.nodes[i].group) {
                    if(use_elk) rough.drawing=rough_cluster_frame(n,theme);
                    if(rough.drawing.bounds) include(*rough.drawing.bounds,rough.offset.x,rough.offset.y);
                } else if(rough.drawing.bounds) {
                    include(*rough.drawing.bounds,n.x+rough.offset.x,n.y+rough.offset.y);
                }
                for(const auto& layer:rough.overlays) if(layer.drawing.bounds)
                    include(*layer.drawing.bounds,n.x+layer.offset.x,n.y+layer.offset.y);
            }
            for(std::size_t i=0;i<graph.edges.size();++i) {
                const auto& e=layout->edges[i];
                if(!e.points.empty()) {
                    rough_edges[i]=sketch_svg_path(edge_path(i),
                        {.seed=theme.hand_drawn_seed,.roughness=.3,.fill=RoughFill::none});
                    if(const auto& bounds=rough_edges[i].bounds) include(*bounds);
                }
                if(e.label) include({-labels[i].width/2,-labels[i].height/2,
                    labels[i].width/2,labels[i].height/2},e.label->x,e.label->y);
            }
            if(!std::isfinite(scene.min_x)) return std::unexpected("Flowchart hand-drawn graph has no painted content");
        }
        const double wrapper_offset=use_elk?0:theme.padding;
        // insertTitle centers over the graph before setupViewPortForSVG expands
        // its bounds. This group is translated by padding, so compensate the
        // title baseline to retain the original root-space -titleTopMargin.
        const double title_x=(scene.min_x+scene.max_x)/2;
        const double title_y=-theme.title_top_margin-wrapper_offset;
        if(!graph.title.empty()) {
            const auto& title=*measured.title;
            scene.min_x=std::min(scene.min_x,title_x-title.width/2);
            scene.max_x=std::max(scene.max_x,title_x+title.width/2);
            scene.min_y=std::min(scene.min_y,title_y-title.baseline);
            scene.max_y=std::max(scene.max_y,title_y+title.height-title.baseline);
        }
        FlowchartSvg output;output.width=scene.max_x-scene.min_x+2*theme.padding;output.height=scene.max_y-scene.min_y+2*theme.padding;
        std::string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"flowchart\"";
        if(theme.hand_drawn) svg+=" fill=\""+theme.title_color+"\"";
        svg+=" width=\""+number(output.width)+
            "\" height=\""+number(output.height)+"\" viewBox=\""+number(scene.min_x+(use_elk?-theme.padding:0))+" "+number(scene.min_y+(use_elk?-theme.padding:0))+" "+number(output.width)+" "+number(output.height)+"\">";
        for(auto [tag,value]:{std::pair{"title",std::u16string_view(graph.acc_title)},{"desc",std::u16string_view(graph.acc_description)}})
            if(!value.empty()) svg+="<"+std::string(tag)+">"+xml(value)+"</"+tag+">";
        const auto border=theme.neo && theme.use_gradient?"url(#flow-gradient)":theme.border;
        const auto shadow=theme.neo?neo_shadow(theme):NeoShadow{};
        const auto stroke_width=theme.neo?number(theme.node_stroke_width):"1";
        if(theme.neo) svg+=neo_look_definitions(theme,shadow);
        svg+=theme.neo?neo_marker_definitions(theme.line):marker_definitions(theme.line);
        for(std::size_t i=0;i<edge_paints.size();++i) if(!edge_paints[i].marker_stroke.empty())
            svg+=theme.neo?neo_marker_definitions(theme.line,"flow-"+std::to_string(i),xml(edge_paints[i].marker_stroke)):
                marker_definitions(theme.line,"flow-"+std::to_string(i),xml(edge_paints[i].marker_stroke));
        svg+="<g font-family=\""+xml(theme.font_family)+"\" font-size=\""+number(theme.font_size)+"\" transform=\"translate("+number(wrapper_offset)+","+number(wrapper_offset)+")\">";
        const auto text=[&](std::u16string_view,FlowchartPoint center,const FlowchartTextBox& b,std::string_view css,const FlowchartPaint& paint) {
            // createLabel's unbounded HTML group box keeps each line left aligned.
            const bool left=css=="cluster-label" && theme.html_label_spacing && !b.centered_group;
            if(left) center.x-=b.width/2;
            std::string out="<text class=\""+std::string(css)+"\" xml:space=\"preserve\" text-anchor=\""+(left?"start":"middle")+"\" fill=\""+paint.get(u"color",css=="cluster-label"?theme.cluster_text:theme.text)+"\"";
            const auto& font=b.paint_font?b.paint_font:b.font;
            if(font) out+=" font-family=\""+xml(font->family)+"\" font-size=\""+number(font->size)+
                "\" font-weight=\""+std::to_string(font->weight)+"\" font-style=\""+(font->italic?"italic":"normal")+"\"";
            out+=">";
            for(std::size_t i=0;i<b.lines.size();++i) {
                out+="<tspan x=\""+number(center.x)+"\" y=\""+number(center.y+(b.paint_font?b.paint_baseline:b.baseline)-b.height/2+i*(b.paint_font?b.paint_line_step:b.line_step))+"\">";
                if(b.runs.empty()) out+=xml(b.lines[i]);
                else for(const auto& run:b.runs.at(i)) {
                    out+="<tspan";
                    // HTML strong uses CSS bolder relative to the label font.
                    if(b.svg_label) {
                        out+=" font-weight=\""+std::string(run.strong?"bold":"normal")+"\"";
                        out+=" font-style=\""+std::string(run.emphasis?"italic":"normal")+"\"";
                    } else {
                        if(run.strong) out+=" font-weight=\""+std::to_string(
                            font && font->weight>=550?900:font && font->weight<350?400:700)+"\"";
                        if(run.emphasis) out+=" font-style=\"italic\"";
                    }
                    out+=">"+xml(run.text)+"</tspan>";
                }
                out+="</tspan>";
            }
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
            svg+="<g class=\"cluster\" data-node=\""+xml(n.id)+"\">";
            if(theme.hand_drawn) svg+=rough_flowchart_shape(rough_nodes[i].drawing,node_paints[i],
                theme.cluster_fill,theme.cluster_border,"3",rough_nodes[i].offset);
            else svg+="<rect x=\""+number(n.x-n.width/2)+
                "\" y=\""+number(n.y-n.height/2)+"\" width=\""+number(n.width)+"\" height=\""+number(n.height)+
                "\""+node_paints[i].attributes(theme.cluster_fill,theme.neo?border:theme.cluster_border)+shadow.attribute()+"/>";
            const auto rough_title_offset=theme.hand_drawn?rough_nodes[i].label_offset.y:0;
            svg+=text(source.label,{n.x,n.y-n.height/2+rough_title_offset+theme.group_title_top+height(measured.nodes[i])/2},
                measured.nodes[i],"cluster-label",node_paints[i])+"</g>";
        }
        // Original getEdgesToRender paints ordinary edges first, merged loops last.
        std::vector<std::size_t> edge_order;
        for(bool loops:{false,true}) for(std::size_t i=0;i<graph.edges.size();++i)
            if((graph.edges[i].start==graph.edges[i].end)==loops && !layout->edges[i].points.empty()) edge_order.push_back(i);
        for(auto i:edge_order) {
            const auto& e=graph.edges[i];
            double path_length=0;
            auto length=theme.neo?&path_length:nullptr;
            const auto path=theme.hand_drawn?rough_edges[i].stroke_path:edge_path(i,length);
            const auto dash=theme.neo?neo_edge_dash(e,path_length):e.stroke==u"dotted"?"2":"0";
            svg+="<path class=\"flowchart-link"+std::string(theme.hand_drawn?" transition":"")+"\" data-edge=\""+xml(e.id)+"\" d=\""+path+"\""+
                edge_paints[i].attributes("none",theme.line,e.stroke==u"thick"?"3.5":e.stroke==u"invisible"?"0":stroke_width,dash);
            const auto prefix=edge_paints[i].marker_stroke.empty()?std::string("flow"):"flow-"+std::to_string(i);
            if(auto name=marker_name(e.arrow_start);!name.empty()) svg+=" marker-start=\"url(#"+prefix+"-"+std::string(name)+"-start)\"";
            if(auto name=marker_name(e.arrow_end);!name.empty()) svg+=" marker-end=\"url(#"+prefix+"-"+std::string(name)+"-end)\"";
            svg+="/>";
        }
        for(auto i:edge_order) if(!graph.edges[i].label.empty() && layout->edges[i].label) {
            const auto p=*layout->edges[i].label;const auto b=labels[i];
            svg+="<g class=\"edgeLabel\"><rect x=\""+number(p.x-b.width/2)+"\" y=\""+number(p.y-b.height/2)+"\" width=\""+number(b.width)+"\" height=\""+number(b.height)+"\" fill=\""+theme.label_background+"\"/>";
            svg+=text(graph.edges[i].label,p,measured.edges[i],"edge-text",edge_paints[i])+"</g>";
        }
        for(std::size_t i=0;i<graph.nodes.size();++i) {
            const auto& n=layout->nodes[i];const auto& source=graph.nodes[i];
            if(source.group) continue;
            svg+="<g class=\""+std::string(theme.hand_drawn?"rough-node":"node")+"\" data-node=\""+xml(n.id)+"\"";
            if(theme.hand_drawn) svg+=" data-look=\"handDrawn\"";
            svg+=" transform=\"translate("+number(n.x)+","+number(n.y)+")\">";
            const auto paint=node_paints[i].attributes(theme.fill,border,stroke_width)+
                (source.shape!=u"doublecircle"?shadow.attribute():"");
            if(theme.hand_drawn) {
                const auto& rough=rough_nodes[i];
                const bool grouped=source.shape==u"doublecircle"||rough.grouped;
                if(grouped) svg+="<g class=\"label-container\">";
                svg+=rough_flowchart_shape(rough.drawing,node_paints[i],theme.fill,
                    theme.border,"1.5",rough.offset,rough.stroke_width,grouped?"":"label-container",
                    rough.solid_fill,rough.invisible_stroke);
                for(const auto& layer:rough.overlays)
                    svg+=rough_flowchart_shape(layer.drawing,node_paints[i],theme.fill,
                        theme.border,"1.5",layer.offset,layer.stroke_width,"",layer.solid_fill,layer.invisible_stroke);
                if(grouped) svg+="</g>";
            }
            else if(source.shape==u"circle") svg+="<circle class=\"label-container\" r=\""+number(n.width/2)+"\""+paint+"/>";
            else if(source.shape==u"doublecircle") svg+="<g class=\"label-container\""+paint+"><circle r=\""+number(n.width/2)+"\""+shadow.attribute()+"/><circle r=\""+number(n.width/2-(theme.neo?12:5))+"\""+shadow.attribute()+"/></g>";
            else if(source.shape==u"diamond") svg+="<polygon class=\"label-container\" points=\"0,"+number(-n.height/2)+" "+number(n.width/2)+",0 0,"+number(n.height/2)+" "+number(-n.width/2)+",0\""+(theme.neo?" transform=\"translate(0.5,0)\"":"")+paint+"/>";
            else if(source.shape==u"stadium" || source.shape==u"subroutine" || source.shape==u"cylinder") {
                const auto path=source.shape==u"cylinder"?cylinder_path(n.width,n.height):
                    polygon_path(source.shape==u"stadium"?stadium_points(n.width,n.height):subroutine_points(n.width,n.height));
                svg+="<path class=\"label-container\" d=\""+path+"\""+paint+"/>";
            }
            else if(source.shape==u"lin-doc") svg+="<path class=\"label-container\" d=\""+lined_document_path(n.width,n.height,theme.neo)+"\""+paint+"/>";
            else if(local_shapes[i]) svg+=paint_local_shape(*local_shapes[i],node_paints[i],theme.fill,
                border,stroke_width,shadow.attribute());
            else if(classic_polygon(source.shape)) svg+="<path class=\"label-container\" d=\""+
                polygon_path(classic_polygon_points(source.shape,n.width,n.height,theme.neo))+"\""+paint+"/>";
            else svg+="<rect class=\"label-container\" x=\""+number(-n.width/2)+"\" y=\""+number(-n.height/2)+"\" width=\""+number(n.width)+"\" height=\""+number(n.height)+"\" rx=\""+number(source.shape==u"roundedRect"?theme.radius:0)+"\""+paint+"/>";
            auto label_offset=theme.hand_drawn?rough_nodes[i].label_offset:FlowchartPoint{source.shape==u"odd"?n.height/8:0,0};
            if(local_shapes[i]) label_offset=local_shapes[i]->label_offset;
            if(source.shape==u"cylinder") label_offset.y=source.padding/1.5;
            if(source.shape==u"lin-doc") {
                const double w=n.width/1.1,h=n.height/(theme.neo?1.5:1.25),amplitude=theme.neo?h/4:h/8;
                const double label_width=std::max(measured.nodes[i].width,theme.minimum_label_width);
                label_offset={source.padding+w*0.025-w/2+label_width/2,
                    -h/2+source.padding-amplitude+measured.nodes[i].height/2};
            }
            svg+=text(source.label,label_offset,measured.nodes[i],"nodeLabel",node_paints[i])+"</g>";
        }
        if(!graph.title.empty()) svg+="<text class=\"flowchartTitleText\" text-anchor=\"middle\" font-size=\"18\" fill=\""+
            theme.title_color+"\" x=\""+number(title_x)+"\" y=\""+number(title_y)+"\">"+xml(graph.title)+"</text>";
        output.svg=svg+"</g></svg>";return output;
    } catch(const std::exception& e) {return std::unexpected(e.what());}
}
}
