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
    if(local) if(const auto* circle=std::get_if<FlowchartCircleAttachment>(&local->attachment)) {
        const double dx=p.x-n.x,dy=p.y-n.y,distance=std::hypot(dx,dy);
        const double radius=circle->radius?circle->radius:n.width/2;
        return distance?FlowchartPoint{n.x+radius*dx/distance,n.y+radius*dy/distance}:FlowchartPoint{n.x,n.y};
    }
    if(shape==u"lin-doc") return polygon_intersection(n,lined_document_points(n.width,n.height,neo),p);
    if(!local&&extra_flowchart_shape(shape)&&shape!=u"lin-rect"&&shape!=u"div-rect") {
        return polygon_intersection(n,extra_shape_outline(shape,n.width,n.height),p);
    }
    auto result=classic_shape_intersection(n,shape,p,neo);
    if(local) if(const auto* cylinder=std::get_if<FlowchartCylinderAttachment>(&local->attachment))
        return apply_cylinder_attachment(*cylinder,n,p,result);
    return result;
}
#include "flowchart_elk_paint.inc"
#include "flowchart_elk_line_hops.inc"

void shorten(FlowchartPoint& p,FlowchartPoint neighbor,double amount) {
    const auto dx=neighbor.x-p.x,dy=neighbor.y-p.y,length=std::hypot(dx,dy);
    if(length>0) {p.x+=amount*dx/length;p.y+=amount*dy/length;}
}
#include "flowchart_curves.inc"
#include "flowchart_markers.inc"
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
#include "flowchart_symbol_shapes.inc"
#include "flowchart_storage_shapes.inc"
#include "flowchart_clusters.inc"
#include "flowchart_neo.inc"

FlowchartPoint flowchart_node_label_offset(const FlowchartNode& node,FlowchartNodeBox box,
    const FlowchartTextBox& label,const FlowchartSvgTheme& theme,const FlowchartLocalShape* local,
    const RoughFlowchartNode* rough=nullptr) {
    if(local) return local->label_offset;
    if(node.shape==u"cylinder") return {0,node.padding/1.5};
    if(node.shape==u"collapsedGroup") return {0,-14};
    if(node.shape==u"lin-doc") {
        const double w=box.width/1.1,h=box.height/(theme.neo?1.5:1.25),amplitude=theme.neo?h/4:h/8;
        const double label_width=std::max(label.width,theme.minimum_label_width);
        return {node.padding+w*0.025-w/2+label_width/2,
            -h/2+node.padding-amplitude+label.height/2};
    }
    if(rough) return rough->label_offset;
    return {node.shape==u"odd"?box.height/8:0,0};
}

}

bool flowchart_shape_has_label(std::u16string_view shape) {
    const auto canonical=flowchart_classic_shape(shape);
    return symbol_has_label(canonical.empty()?shape:canonical);
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
        for(std::size_t i=0;i<graph.nodes.size();++i) if(!graph.nodes[i].group&&!flowchart_shape_has_label(graph.nodes[i].shape)) {
            graph.nodes[i].label.clear();measured.nodes[i]={};
        }
        for(auto v:{theme.font_size,theme.minimum_label_width,theme.wrapping_width,theme.padding,theme.radius,theme.group_title_top,theme.group_title_bottom,theme.node_stroke_width,theme.cluster_radius,theme.collapsed_radius,theme.cluster_stroke_width,theme.collapsed_indicator_opacity})
            if(!std::isfinite(v)||v<0) return std::unexpected("Invalid Flowchart style dimension");
        if(theme.collapsed_indicator_opacity>1) return std::unexpected("Invalid Flowchart indicator opacity");
        if(theme.font_size==0||theme.font_family.empty()) return std::unexpected("Invalid Flowchart font");
        for(const auto& paint:{theme.fill,theme.border,theme.text,theme.line,theme.label_background,theme.cluster_fill,theme.cluster_border,theme.cluster_text})
            if(paint!="none") theme_detail::color_adjust(std::u16string(paint.begin(),paint.end()),{});
        if(!theme.collapsed_indicator_fill.empty()&&theme.collapsed_indicator_fill!="none")
            theme_detail::color_adjust(std::u16string(theme.collapsed_indicator_fill.begin(),theme.collapsed_indicator_fill.end()),{});
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
        for(std::size_t i=0;i<graph.nodes.size();++i) ids.emplace(graph.nodes[i].id,i);
        std::vector<std::u16string_view> directions(graph.nodes.size(),graph.direction);
        std::optional<FlowchartGraphPreparation> prepared_graph;
        if(use_elk) {
            // createGraph.resolveNodeDir runs before ELK's hierarchy mode is
            // selected: the nearest explicit direction controls shape drawing.
            for(std::size_t i=0;i<graph.nodes.size();++i) {
                auto current=i;
                for(std::size_t depth=0;;++depth) {
                    if(depth>graph.nodes.size()) return std::unexpected("Cyclic Flowchart containment");
                    const auto& node=graph.nodes[current];
                    if(!node.direction.empty()) {directions[i]=node.direction;break;}
                    if(node.parent.empty()) break;
                    current=ids.at(node.parent);
                }
            }
        } else {
            auto prepared=prepare_flowchart_graph(graph);if(!prepared) return std::unexpected(prepared.error());
            prepared_graph=std::move(*prepared);
            const auto directions_in_scope=[&](auto&& self,const FlowchartPreparedGraph& scope)->void {
                for(const auto& node:scope.nodes) if(node.source_index) directions[*node.source_index]=scope.direction;
                for(const auto& child:scope.subgraphs) self(self,child);
            };
            directions_in_scope(directions_in_scope,prepared_graph->graph);
        }
        double state_padding=0;
        if(const auto* state=graph.effective_config.find(u"state")) if(const auto* value=state->find(u"padding")) {
            if(value->kind!=ConfigKind::number||!std::isfinite(value->number)||value->number<0)
                return std::unexpected("Invalid Flowchart state symbol padding");
            state_padding=value->number;
        }
        const auto height=[](const FlowchartTextBox& b){return b.height;};
        for(std::size_t i=0;i<graph.nodes.size();++i) {
            const auto& n=graph.nodes[i];const auto& b=measured.nodes[i];

            node_paints.push_back(compile_paint(n.compiled_styles,n.styles));
            if(!valid_box(b)) return std::unexpected("Invalid measured Flowchart label");
            const double w=n.label.empty()?0:std::max(b.width,theme.minimum_label_width),h=n.label.empty()?0:height(b);
            FlowchartNodeBox box;
            if(n.group) box={b.width,h};
            else if(n.shape==u"collapsedGroup") {
                box={std::max(b.width+2*n.padding,80.0),h+28+2*n.padding};
                if(theme.hand_drawn) {
                    auto& rough=rough_nodes[i];
                    rough.drawing=sketch_svg_path(rough_rounded_rect(-box.width/2,-box.height/2,
                        box.width,box.height,theme.collapsed_radius),{.seed=theme.hand_drawn_seed,
                        .fill=theme.cluster_fill=="none"?RoughFill::none:RoughFill::solid,
                        .stroke=theme.cluster_border!="none"});
                    rough.solid_fill=true;
                    if(rough.drawing.bounds) box={rough.drawing.bounds->width(),rough.drawing.bounds->height()};
                }
            }
            else if(symbol_flowchart_shape(n.shape)) {
                auto& local=local_shapes[i].emplace(measure_symbol_shape(n.shape,w,h,n.padding,
                    directions[i],state_padding,node_paints[i],theme));
                box=local.box;
            }
            else if(const auto classic=classic_shape_box(n.shape,n.label.empty()&&n.shape!=u"circle"&&n.shape!=u"doublecircle"?0:b.width,h,n.label.empty()?0:theme.minimum_label_width,n.padding,theme.neo)) box=*classic;
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
            else if(storage_flowchart_shape(n.shape)) {
                auto& local=local_shapes[i].emplace(measure_storage_flowchart_shape(
                    n.shape,w,h,n.padding,theme.neo));
                if(theme.hand_drawn) rough_nodes[i]=measure_rough_storage_shape(n.shape,local,node_paints[i],theme);
                else if(local.flat_rough_paths) measure_storage_flat_paths(local,node_paints[i],theme);
                box=local.box;
            }
            else return std::unexpected("Flowchart SVG shape is not implemented yet");
            if(theme.hand_drawn && !n.group && n.shape!=u"collapsedGroup" && !symbol_flowchart_shape(n.shape) && !document_flowchart_shape(n.shape) && !storage_flowchart_shape(n.shape)) {
                rough_nodes[i]=rough_flowchart_node(n.shape,box,node_paints[i],theme);
                if(const auto& bounds=rough_nodes[i].drawing.bounds)
                    box={bounds->width(),bounds->height()};
            }
            if(theme.hand_drawn && !n.group && n.shape!=u"collapsedGroup" && !symbol_flowchart_shape(n.shape)) {
                auto& rough=rough_nodes[i];
                rough.label_offset=flowchart_node_label_offset(n,box,b,theme,find_local_shape(local_shapes,i),&rough);
                // With no drawable paths Mermaid measures the label itself for layout.
                if(!n.label.empty()&&!rough_flowchart_bounds(rough)) {
                    box={w,h};
                    const auto [x,y]=rough.label_offset;
                    rough.label_only_bounds=RoughBounds{x-w/2,y-h/2,x+w/2,y+h/2};
                }
            }
            boxes.push_back(box);
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
                clip_elk_node_endpoints(points,scene.layout.nodes[a],scene.layout.nodes[b],graph.nodes[a].shape,graph.nodes[b].shape,
                    [&](const auto& node,auto shape,auto point) {
                        return intersect(node,shape,point,theme.neo,find_local_shape(local_shapes,ids.at(node.id)));
                    },theme.hand_drawn,graph.nodes[a].group,graph.nodes[b].group);
                if(edge.arrow_end==u"arrow_point")elk_remove_short_marker_segment(points,scene.layout.nodes[b],false);
                if(edge.arrow_start==u"arrow_point")elk_remove_short_marker_segment(points,scene.layout.nodes[a],true);
            }
            if(!theme.hand_drawn) measure_flowchart_paint_bounds(scene,graph,labels,local_shapes);
        }else {
            scene=layout_prepared(graph,prepared_graph->graph,boxes,labels,measured,theme,nullptr,
                theme.hand_drawn?&rough_nodes:nullptr,local_shapes);
        }
        const auto* layout=&scene.layout;
        const auto hopped=use_elk&&theme.elk_line_hops!=FlowchartElkLineHops::off?
            paint_elk_line_hops(layout->edges,graph.edges,theme.elk_line_hops):std::vector<ElkHopRoute>{};
        const auto edge_path=[&](std::size_t i,double* length=nullptr) {
            const auto& e=graph.edges[i];
            if(!hopped.empty()&&!hopped[i].path.empty()) {
                if(length)*length+=hopped[i].length;
                return hopped[i].path;
            }
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
                const bool group=graph.nodes[i].group;
                if(group&&use_elk) rough.drawing=rough_cluster_frame(n,theme);
                if(const auto bounds=rough_flowchart_bounds(rough))
                    include(*bounds,group?0:n.x,group?0:n.y);
                else if(local_shapes[i]) include(local_shapes[i]->paint_bounds,n.x,n.y);
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
            if(b.math_label) {
                const auto& font=b.paint_font?b.paint_font:b.font;
                const auto color=paint.get(u"color",css=="cluster-label"?theme.cluster_text:theme.text);
                auto label=render_math_label_svg(*b.math_label,center.x-b.width/2,
                    center.y-b.height/2,{font?font->family:theme.font_family,color,css,b.width});
                if(!label) throw std::runtime_error(label.error());
                return std::move(*label);
            }
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
                "\" rx=\""+number(theme.cluster_radius)+"\" ry=\""+number(theme.cluster_radius)+"\""+node_paints[i].attributes(theme.cluster_fill,theme.neo?border:theme.cluster_border,number(theme.cluster_stroke_width))+shadow.attribute()+"/>";
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
            if(source.shape==u"collapsedGroup") {
                const auto width=std::max(measured.nodes[i].width+2*source.padding,80.0);
                const auto height=measured.nodes[i].height+28+2*source.padding;
                if(theme.hand_drawn) {
                    auto frame_paint=node_paints[i];
                    frame_paint.values.erase(u"fill");frame_paint.values.erase(u"stroke");
                    const auto frame_width=theme.cluster_stroke_width==1?std::string{}:number(theme.cluster_stroke_width);
                    svg+=rough_flowchart_shape(rough_nodes[i].drawing,frame_paint,
                        theme.cluster_fill,theme.cluster_border,"0",{},frame_width,
                        "label-container collapsed-group",true);
                } else {
                    // The ordinary .node rect rule outranks the handler's
                    // presentation attributes; explicit node CSS still wins.
                    svg+="<rect class=\"label-container collapsed-group\" x=\""+number(-width/2)+
                        "\" y=\""+number(-height/2)+"\" width=\""+number(width)+"\" height=\""+number(height)+
                        "\" rx=\""+number(theme.collapsed_radius)+"\" ry=\""+number(theme.collapsed_radius)+"\""+
                        node_paints[i].attributes(theme.fill,border,stroke_width)+shadow.attribute()+"/>";
                }
                const auto separator=-height/2+source.padding+measured.nodes[i].height+8;
                svg+="<line class=\"collapsed-separator\" x1=\""+number(-width/2+8)+"\" x2=\""+number(width/2-8)+
                    "\" y1=\""+number(separator)+"\" y2=\""+number(separator)+"\" stroke=\""+theme.cluster_border+
                    "\" stroke-width=\""+(theme.hand_drawn?"1":"0.75")+"\" stroke-dasharray=\"3, 3\"/>";
                const auto indicator=theme.hand_drawn||theme.collapsed_indicator_fill.empty()?
                    theme.cluster_border:theme.collapsed_indicator_fill;
                for(int dot=-1;dot<=1;++dot) svg+="<circle class=\"collapsed-indicator\" cx=\""+number(dot*10)+
                    "\" cy=\""+number(separator+10)+"\" r=\"2.5\" fill=\""+indicator+
                    "\" stroke=\"none\" opacity=\""+number(theme.hand_drawn?1:theme.collapsed_indicator_opacity)+"\"/>";
            }
            else if(symbol_flowchart_shape(source.shape)) svg+=paint_symbol_shape(source.shape,
                *local_shapes[i],node_paints[i],theme,border,stroke_width,shadow.attribute());
            else if(theme.hand_drawn) {
                const auto& rough=rough_nodes[i];
                const bool grouped=source.shape==u"doublecircle"||rough.grouped;
                if(grouped) svg+="<g class=\"label-container\">";
                svg+=rough_flowchart_shape(rough.drawing,node_paints[i],theme.fill,
                    theme.border,"1.5",rough.offset,rough.stroke_width,grouped?"":"label-container",
                    rough.solid_fill,rough.invisible_stroke);
                for(const auto& layer:rough.overlays)
                    svg+=rough_flowchart_shape(layer.drawing,node_paints[i],theme.fill,
                        theme.border,"1.5",layer.offset,layer.stroke_width,"",layer.solid_fill,layer.invisible_stroke);
                if(storage_flowchart_shape(source.shape))
                    svg+=paint_storage_decorations(*local_shapes[i],theme.border,false);
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
                border,stroke_width,shadow.attribute(),theme.hand_drawn_seed,theme.border);
            else if(classic_polygon(source.shape)) svg+="<path class=\"label-container\" d=\""+
                polygon_path(classic_polygon_points(source.shape,n.width,n.height,theme.neo))+"\""+paint+"/>";
            else svg+="<rect class=\"label-container\" x=\""+number(-n.width/2)+"\" y=\""+number(-n.height/2)+"\" width=\""+number(n.width)+"\" height=\""+number(n.height)+"\" rx=\""+number(source.shape==u"roundedRect"?theme.radius:0)+"\""+paint+"/>";
            const auto label_offset=theme.hand_drawn&&source.shape!=u"collapsedGroup"&&!symbol_flowchart_shape(source.shape)?rough_nodes[i].label_offset:
                flowchart_node_label_offset(source,{n.width,n.height},measured.nodes[i],theme,find_local_shape(local_shapes,i));
            if(flowchart_shape_has_label(source.shape)) svg+=text(source.label,label_offset,measured.nodes[i],"nodeLabel",node_paints[i]);
            svg+="</g>";
        }
        if(!graph.title.empty()) svg+="<text class=\"flowchartTitleText\" text-anchor=\"middle\" font-size=\"18\" fill=\""+
            theme.title_color+"\" x=\""+number(title_x)+"\" y=\""+number(title_y)+"\">"+xml(graph.title)+"</text>";
        output.svg=svg+"</g></svg>";return output;
    } catch(const std::exception& e) {return std::unexpected(e.what());}
}
}
