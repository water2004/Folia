// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_layout;
import std;
import folia.mermaid.elk_order;

namespace folia::mermaid {
std::expected<ElkFlatLayoutResult,std::string> elk_layout_flat(
    const ElkFlatLayoutInput& input,const ElkFlatLayoutOptions& options) {
    ElkCycleGraph source{.node_count=input.nodes.size()};
    for(const auto& edge:input.edges) {
        if(edge.source>=source.node_count||edge.target>=source.node_count)
            return std::unexpected("ELK edge has no measured node");
        if(edge.source==edge.target)return std::unexpected("ELK flat layout requires self-loop preprocessing");
        const auto p=source.ports.size();source.ports.push_back({edge.source});source.ports.push_back({edge.target});source.edges.push_back({p,p+1});
    }
    const bool vertical=options.direction==ElkLayoutDirection::down||options.direction==ElkLayoutDirection::up;
    auto centers=input.center_labels;auto ends=input.end_labels;
    if(vertical){for(auto& l:centers)std::swap(l.width,l.height);for(auto& l:ends)std::swap(l.width,l.height);}
    const auto label_direction=!vertical?ElkLabelDirection::horizontal:options.direction==ElkLayoutDirection::up?ElkLabelDirection::up:ElkLabelDirection::down;
    auto prepared=elk_prepare_inline_labels(source,centers,label_direction,options.edge_label_spacing,options.label_label_spacing);
    if(!prepared)return std::unexpected(prepared.error());auto& g=prepared->graph;
    for(std::size_t n=0;n<input.nodes.size();++n){g.nodes[n].width=vertical?input.nodes[n].height:input.nodes[n].width;g.nodes[n].height=vertical?input.nodes[n].width:input.nodes[n].height;}
    elk_split_long_edges(g);elk_assign_flat_port_sides(g);
    if(const auto sorted=elk_sort_flat_ports(g);!sorted)return std::unexpected(sorted.error());
    ElkRandom random(static_cast<std::uint64_t>(options.seed));
    if(const auto ordered=elk_order_flat_graph(g,random);!ordered)return std::unexpected(ordered.error());
    std::vector<ElkPointPortOptions> port_options(g.nodes.size(),options.ports);
    for(std::size_t n=0;n<input.nodes.size();++n)port_options[n].center_alignment=input.nodes[n].center_ports;
    if(const auto ports=elk_place_flat_point_ports(g,port_options);!ports)return std::unexpected(ports.error());
    elk_switch_label_dummies(g);elk_place_inline_label_ports(g,prepared->dummies,options.edge_label_spacing);
    std::vector<ElkHorizontalMargin> margins(g.nodes.size());
    // Original configuration runs EndLabelPreprocessor before LabelSideSelector:
    // newly imported terminal labels still have UNKNOWN (the below branch).
    const auto end_cells=elk_prepare_end_labels(g,ends,margins,vertical,options.edge_label_spacing,options.label_label_spacing,vertical,false);
    if(!end_cells)return std::unexpected(end_cells.error());
    const auto placed=elk_place_flat_nodes(g,options.placement,false);if(!placed)return std::unexpected(placed.error());
    double minimum=std::numeric_limits<double>::infinity(),maximum=-minimum;
    for(const auto& layer:g.layers)if(!layer.empty()) {
        const auto first=layer.front(),last=layer.back();
        minimum=std::min(minimum,(*placed)[first]-g.nodes[first].margin_top);
        maximum=std::max(maximum,(*placed)[last]+g.nodes[last].height+g.nodes[last].margin_bottom);
    }
    if(g.nodes.empty())minimum=maximum=0;
    const auto routed=elk_route_flat_edges(g,*placed,margins,random,options.routing);if(!routed)return std::unexpected(routed.error());
    auto routes=elk_restore_flat_routes(g,*routed,*placed,options.unnecessary_bendpoints);if(!routes)return std::unexpected(routes.error());
    ElkFlatLayoutResult result{.routes=std::move(*routes),
        .center_labels=elk_restore_inline_labels(g,prepared->dummies,routed->node_x,*placed,label_direction,options.label_label_spacing),
        .end_labels=elk_restore_end_labels(*end_cells,routed->node_x,*placed,vertical,options.label_label_spacing)};
    const auto width=routed->width,height=maximum-minimum;
    const auto transform=[&](double x,double y,double w,double h) {
        y-=minimum;
        if(options.direction==ElkLayoutDirection::left)x=width-x-w;
        if(vertical){std::swap(x,y);std::swap(w,h);if(options.direction==ElkLayoutDirection::up)y=width-y-h;}
        return ElkNodeBox{x+options.padding,y+options.padding,w,h};
    };
    for(std::size_t n=0;n<input.nodes.size();++n)result.nodes.push_back(transform(routed->node_x[n],(*placed)[n],g.nodes[n].width,g.nodes[n].height));
    const auto transform_point=[&](ElkRoutingPoint p){const auto r=transform(p.x,p.y,0,0);return ElkRoutingPoint{r.x,r.y};};
    for(auto& route:result.routes){route.start=transform_point(route.start);route.end=transform_point(route.end);for(auto& p:route.bends)p=transform_point(p);}
    for(auto* labels:{&result.center_labels,&result.end_labels})for(auto& l:*labels){const auto box=transform(l.x,l.y,l.width,l.height);l.x=box.x;l.y=box.y;l.width=box.width;l.height=box.height;}
    result.width=(vertical?height:width)+2*options.padding;result.height=(vertical?width:height)+2*options.padding;
    return result;
}
}
