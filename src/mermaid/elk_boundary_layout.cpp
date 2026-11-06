// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_boundary_layout;
import std;
import folia.mermaid.elk_order;

namespace folia::mermaid {
std::expected<ElkBoundaryLayoutResult,std::string> elk_layout_boundary_graph(
    const ElkCycleGraph& source,std::span<const ElkExternalLayerRole> roles,
    std::span<const ElkMeasuredNode> measured,const ElkFlatLayoutOptions& options,double border_offset) {
    if(options.direction!=ElkLayoutDirection::right)return std::unexpected("ELK boundary layout requires direction-normalized input");
    if(measured.size()!=source.node_count)return std::unexpected("ELK boundary layout requires measured nodes");
    const auto layered=elk_layer_boundary_graph(source,roles);if(!layered)return std::unexpected(layered.error());
    auto made=make_elk_layered_graph(source,*layered);if(!made)return std::unexpected(made.error());auto& g=*made;
    for(std::size_t n=0;n<source.node_count;++n) {
        if(roles[n]==ElkExternalLayerRole::none){g.nodes[n].width=measured[n].width;g.nodes[n].height=measured[n].height;}
        else {g.nodes[n].kind=ElkLayerNodeKind::external_port;g.nodes[n].fixed_port_positions=true;}
    }
    elk_split_long_edges(g);elk_assign_flat_port_sides(g);
    if(const auto sorted=elk_sort_flat_ports(g);!sorted)return std::unexpected(sorted.error());
    ElkRandom random(static_cast<std::uint64_t>(options.seed));
    if(const auto ordered=elk_order_flat_graph(g,random);!ordered)return std::unexpected(ordered.error());
    std::vector<ElkPointPortOptions> ports(g.nodes.size(),options.ports);
    for(std::size_t n=0;n<source.node_count;++n)ports[n].center_alignment=measured[n].center_ports;
    if(const auto placed=elk_place_flat_point_ports(g,ports);!placed)return std::unexpected(placed.error());
    auto y=elk_place_flat_nodes(g,options.placement,false);if(!y)return std::unexpected(y.error());
    double min_y=std::numeric_limits<double>::infinity(),max_y=-min_y;
    for(const auto& layer:g.layers)if(!layer.empty()) {
        const auto first=layer.front(),last=layer.back();
        auto top=(*y)[first]-g.nodes[first].margin_top,bottom=(*y)[last]+g.nodes[last].height+g.nodes[last].margin_bottom;
        if(g.nodes[first].kind==ElkLayerNodeKind::external_port)top-=options.ports.surrounding_margin;
        if(g.nodes[last].kind==ElkLayerNodeKind::external_port)bottom+=options.ports.surrounding_margin;
        min_y=std::min(min_y,top);max_y=std::max(max_y,bottom);
    }
    if(g.nodes.empty())min_y=max_y=0;
    std::vector<ElkHorizontalMargin> margins(g.nodes.size());
    auto routed=elk_route_flat_edges(g,*y,margins,random,options.routing);if(!routed)return std::unexpected(routed.error());
    ElkCompoundContent content{.width=routed->width,.height=max_y-min_y,
        .padding_left=options.padding,.padding_right=options.padding,.padding_top=options.padding,.padding_bottom=options.padding,.offset={0,-min_y}};
    std::vector<std::optional<ElkRoutingPoint>> boundary_ports(source.node_count);
    for(std::size_t n=0;n<source.node_count;++n)if(roles[n]!=ElkExternalLayerRole::none) {
        auto dummy=elk_make_compound_port_dummy(ElkLayoutDirection::right,roles[n]==ElkExternalLayerRole::last_separate,border_offset);
        dummy.position={routed->node_x[n],(*y)[n]};boundary_ports[n]=elk_export_compound_port(content,dummy);
        routed->node_x[n]=dummy.position.x;
    }
    auto routes=elk_restore_flat_routes(g,*routed,*y,options.unnecessary_bendpoints);if(!routes)return std::unexpected(routes.error());
    return ElkBoundaryLayoutResult{std::move(g),std::move(*y),std::move(*routed),content,std::move(*routes),std::move(boundary_ports)};
}
}
