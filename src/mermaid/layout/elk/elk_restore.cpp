// SPDX-License-Identifier: EPL-2.0
// Original long-edge segment joining and feedback-edge restoration projection.
module folia.mermaid.elk_restore;
import std;

namespace folia::mermaid {
std::expected<std::vector<ElkRestoredRoute>,std::string> elk_restore_flat_routes(
    const ElkLayeredGraph& g,const ElkOrthogonalRoutes& routes,std::span<const double> y,bool unnecessary_bendpoints) {
    if(routes.node_x.size()!=g.nodes.size()||y.size()!=g.nodes.size()||routes.bends.size()!=g.edges.size())
        return std::unexpected("ELK restoration requires all routed node positions and edge segments");
    const auto anchor=[&](std::size_t p) {
        const auto& port=g.ports[p];return ElkRoutingPoint{routes.node_x[port.node]+port.x,y[port.node]+port.y};
    };
    std::vector<ElkRestoredRoute> result;
    for(std::size_t index=0;index<g.edges.size();++index) {
        const auto& edge=g.edges[index];
        const auto source_kind=g.nodes[g.ports[edge.source_port].node].kind;
        if(source_kind==ElkLayerNodeKind::long_edge||source_kind==ElkLayerNodeKind::label)continue;
        ElkRestoredRoute route{.original_edge=edge.original_edge,.source_port=edge.source_port};
        auto segment=index;
        for(;;) {
            const auto& current=g.edges[segment];
            route.bends.insert(route.bends.end(),routes.bends[segment].begin(),routes.bends[segment].end());
            const auto target=current.target_port;
            const auto& node=g.nodes[g.ports[target].node];
            if(node.kind==ElkLayerNodeKind::normal||node.kind==ElkLayerNodeKind::external_port) {route.target_port=target;break;}
            if(unnecessary_bendpoints&&node.kind==ElkLayerNodeKind::long_edge)
                route.bends.push_back(anchor(node.ports.front()));
            // LongEdgeSplitter owns exactly one input and one output per dummy.
            std::optional<std::size_t> continuation;
            for(const auto p:node.ports)if(g.ports[p].side==ElkPortSide::east) {
                if(g.ports[p].outgoing.size()!=1)
                    return std::unexpected("ELK restoration requires unmerged long-edge segments");
                continuation=g.ports[p].outgoing.front();break;
            }
            if(!continuation)return std::unexpected("ELK long-edge dummy is missing its output segment");
            segment=*continuation;
        }
        if(edge.reversed) {std::swap(route.source_port,route.target_port);std::ranges::reverse(route.bends);}
        route.source_node=g.ports[route.source_port].node;route.target_node=g.ports[route.target_port].node;
        route.start=anchor(route.source_port);route.end=anchor(route.target_port);
        result.push_back(std::move(route));
    }
    std::ranges::sort(result,{},&ElkRestoredRoute::original_edge);
    return result;
}
}
