// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_compound_restore;
import std;

namespace folia::mermaid {
std::vector<ElkCompoundRestoredEdge> elk_restore_compound_edges(
    const ElkCompoundGraph& g,const ElkCompoundGeometry& geometry,bool unnecessary) {
    const auto origin=[&](std::size_t graph) {
        ElkRoutingPoint result;
        for(;;) {
            const auto& content=geometry.graphs[graph];result.x+=content.offset.x;result.y+=content.offset.y;
            const auto parent=g.graphs[graph].parent_node;if(!parent)break;
            result.x+=content.padding_left+geometry.node_positions[*parent].x;
            result.y+=content.padding_top+geometry.node_positions[*parent].y;
            graph=g.nodes[*parent].graph;
        }
        return result;
    };
    const auto offset=[&](std::size_t from,std::size_t to){const auto a=origin(from),b=origin(to);return ElkRoutingPoint{a.x-b.x,a.y-b.y};};
    const auto add=[](ElkRoutingPoint p,ElkRoutingPoint delta){return ElkRoutingPoint{p.x+delta.x,p.y+delta.y};};
    const auto anchor=[&](std::size_t p){return add(geometry.node_positions[g.ports[p].node],geometry.port_positions[p]);};
    const auto actual_port=[&](std::size_t p){const auto& node=g.nodes[g.ports[p].node];return node.boundary_port.value_or(p);};
    const auto descendant=[&](std::size_t n,std::size_t ancestor){auto p=g.graphs[g.nodes[n].graph].parent_node;while(p){if(*p==ancestor)return true;p=g.graphs[g.nodes[*p].graph].parent_node;}return false;};
    std::vector<ElkCompoundRestoredEdge> result;
    for(std::size_t e=0;e<g.edges.size();++e) {
        const auto& chain=g.edges[e];
        const auto source=actual_port(g.segments[chain.segments.front()].source_port),target=actual_port(g.segments[chain.segments.back()].target_port);
        const auto source_node=g.ports[source].node,target_node=g.ports[target].node;
        const auto reference=descendant(target_node,source_node)?*g.nodes[source_node].nested_graph:g.nodes[source_node].graph;
        ElkCompoundRestoredEdge edge{.original_edge=e,.reference_graph=reference,.source_port=source,.target_port=target};
        edge.start=add(anchor(source),offset(g.nodes[source_node].graph,reference));
        edge.end=add(anchor(target),offset(g.nodes[target_node].graph,reference));
        edge.target_offset=offset(g.nodes[target_node].graph,reference);
        std::optional<ElkRoutingPoint> last;
        for(const auto s:chain.segments) {
            const auto& segment=g.segments[s];const auto& points=geometry.segments[s];const auto delta=offset(segment.graph,reference);
            const auto start=add(anchor(segment.source_port),delta),end=add(anchor(segment.target_port),delta);
            if(last) {
                const auto next=points.bends.empty()?end:add(points.bends.front(),delta);
                const auto x=std::abs(last->x-next.x)>0.001,y=std::abs(last->y-next.y)>0.001;
                if(unnecessary?(x||y):(x&&y))edge.bends.push_back(start);
            }
            for(const auto p:points.bends)edge.bends.push_back(add(p,delta));
            last=points.bends.empty()?start:add(points.bends.back(),delta);
            for(auto label:points.labels){label.x+=delta.x;label.y+=delta.y;edge.labels.push_back(label);}
        }
        result.push_back(std::move(edge));
    }
    return result;
}
}
