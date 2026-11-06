// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_north_south_layout;
import std;

namespace folia::mermaid {
std::expected<ElkNorthSouthLayoutResult,std::string> elk_layout_north_south_graph(
    ElkLayeredGraph& g,const ElkNorthSouthPreparation& prepared,ElkRandom& random,const ElkFlatLayoutOptions& options) {
    std::vector<ElkHorizontalMargin> margins(g.nodes.size());
    for(std::size_t n=0;n<g.nodes.size();++n)for(const auto p:g.nodes[n].ports) {
        auto& node=g.nodes[n];const auto& port=g.ports[p];
        margins[n].left=std::max(margins[n].left,-port.x);margins[n].right=std::max(margins[n].right,port.x-node.width);
        node.margin_top=std::max(node.margin_top,-port.y);node.margin_bottom=std::max(node.margin_bottom,port.y-node.height);
    }
    const auto y=elk_place_flat_nodes(g,options.placement,false);if(!y)return std::unexpected(y.error());
    double minimum=std::numeric_limits<double>::infinity(),maximum=-minimum;
    for(const auto& layer:g.layers)if(!layer.empty()) {
        const auto first=layer.front(),last=layer.back();minimum=std::min(minimum,(*y)[first]-g.nodes[first].margin_top);
        maximum=std::max(maximum,(*y)[last]+g.nodes[last].height+g.nodes[last].margin_bottom);
    }
    if(g.nodes.empty())minimum=maximum=0;
    auto routed=elk_route_flat_edges(g,*y,margins,random,options.routing);if(!routed)return std::unexpected(routed.error());
    elk_restore_north_south_ports(g,prepared,*routed,*y);
    auto routes=elk_restore_flat_routes(g,*routed,*y,options.unnecessary_bendpoints);if(!routes)return std::unexpected(routes.error());
    return ElkNorthSouthLayoutResult{*y,std::move(*routed),std::move(*routes),maximum-minimum,-minimum};
}
}
