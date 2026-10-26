// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_compound_graph;
import std;

namespace folia::mermaid {
namespace {
struct External {
    std::size_t edge,dummy,port;
    bool outgoing,exported;
};
struct Builder {
    const ElkCompoundInput& input;
    ElkCompoundGraph result;
    std::vector<std::vector<std::size_t>> incoming,outgoing;
    std::vector<std::optional<std::size_t>> port_dummy;
    std::vector<std::size_t> original_source,original_target;
    bool descendant(std::size_t node,std::optional<std::size_t> parent)const {
        auto p=result.graphs[result.nodes[node].graph].parent_node;
        while(p){if(p==parent)return true;p=result.graphs[result.nodes[*p].graph].parent_node;}return false;
    }
    std::size_t port(std::size_t node,ElkPortSide side) {
        const auto p=result.ports.size();result.ports.push_back({node,side});result.nodes[node].ports.push_back(p);
        incoming.emplace_back();outgoing.emplace_back();port_dummy.emplace_back();return p;
    }
    std::size_t segment(std::size_t graph,std::size_t e,std::size_t s,std::size_t t) {
        const auto index=result.segments.size();result.segments.push_back({e,graph,s,t});
        result.graphs[graph].segments.push_back(index);result.edges[e].segments.push_back(index);return index;
    }
    std::size_t dummy(std::size_t graph,std::size_t parent,std::size_t e,bool out,
        std::optional<std::size_t> existing={}) {
        if(existing&&port_dummy[*existing])return *port_dummy[*existing];
        const auto n=result.nodes.size();
        auto boundary=elk_make_compound_port_dummy(result.graphs[graph].direction,out,existing?0:input.edge_spacing/2);
        result.nodes.push_back({.graph=graph,.boundary=boundary});
        port(n,boundary.inner_port_side);
        const auto outer=existing?*existing:port(parent,boundary.side);
        result.nodes[n].boundary_port=outer;port_dummy[outer]=n;
        return n;
    }
    External introduce(std::size_t graph,std::size_t parent,std::size_t e,std::size_t opposite,bool out) {
        const auto endpoint=out?original_target[e]:original_source[e];
        const auto existing=result.ports[endpoint].node==parent?std::optional{endpoint}:std::nullopt;
        const auto n=dummy(graph,parent,e,out,existing),inner=result.nodes[n].ports.front();
        segment(graph,e,out?opposite:inner,out?inner:opposite);
        return {e,n,*result.nodes[n].boundary_port,out,!existing};
    }
    void append_created(std::size_t graph,const std::vector<External>& created,std::vector<External>& exported) {
        for(const auto& e:created){auto& nodes=result.graphs[graph].nodes;
            if(std::ranges::find(nodes,e.dummy)==nodes.end())nodes.push_back(e.dummy);
            if(e.exported)exported.push_back(e);}
    }
    std::vector<External> transform(std::size_t graph) {
        const auto parent=result.graphs[graph].parent_node;
        std::vector<External> contained,exported,created;
        const auto children=result.graphs[graph].nodes;
        for(const auto n:children)if(result.nodes[n].nested_graph) {
            const auto nested=*result.nodes[n].nested_graph;
            auto ports=transform(nested);contained.insert(contained.end(),ports.begin(),ports.end());
            const bool external=std::ranges::any_of(result.graphs[nested].nodes,[&](auto child){return result.nodes[child].boundary.has_value();});
            if(external) {
                const auto outer_ports=result.nodes[n].ports;
                for(const auto p:outer_ports)if(!port_dummy[p]) {
                    int vote=0;
                    for(const auto e:outgoing[p])vote+=result.graphs[result.nodes[input.edges[e].target].graph].parent_node==n?-1:1;
                    for(const auto e:incoming[p])vote+=result.graphs[result.nodes[input.edges[e].source].graph].parent_node==n?1:-1;
                    const auto d=dummy(nested,n,0,vote>=0,p);result.graphs[nested].nodes.push_back(d);
                }
            }
        }
        // Original inner segments are visited in the child-export order.
        for(const auto& ext:contained) {
            const auto other=ext.outgoing?input.edges[ext.edge].target:input.edges[ext.edge].source;
            if(result.nodes[other].graph==graph) {
                segment(graph,ext.edge,ext.outgoing?ext.port:original_source[ext.edge],ext.outgoing?original_target[ext.edge]:ext.port);
            } else if(!parent||descendant(other,parent)) {
                if(ext.outgoing) {
                    const auto peer=std::ranges::find_if(contained,[&](const auto& p){return p.edge==ext.edge&&p.dummy!=ext.dummy;});
                    if(peer==contained.end())throw std::logic_error("ELK cross-hierarchy edge has no sibling boundary");
                    segment(graph,ext.edge,ext.port,peer->port);
                }
            } else created.push_back(introduce(graph,*parent,ext.edge,ext.port,ext.outgoing));
        }
        append_created(graph,created,exported);created.clear();
        if(parent) {
            const auto nodes=result.graphs[graph].nodes;
            for(const auto n:nodes) {
                const auto ports=result.nodes[n].ports;
                for(const auto p:ports) {
                    const auto out_edges=outgoing[p],in_edges=incoming[p];
                    for(const auto e:out_edges)if(!descendant(input.edges[e].target,parent))created.push_back(introduce(graph,*parent,e,p,true));
                    for(const auto e:in_edges)if(!descendant(input.edges[e].source,parent))created.push_back(introduce(graph,*parent,e,p,false));
                }
            }
            append_created(graph,created,exported);
        }
        return exported;
    }
    ElkCompoundGraph run() {
        result.graphs.push_back({.direction=input.direction});result.nodes.resize(input.nodes.size());result.edges.resize(input.edges.size());
        for(std::size_t n=0;n<input.nodes.size();++n){result.nodes[n].original=n;if(input.nodes[n].group){result.nodes[n].nested_graph=result.graphs.size();result.graphs.push_back({.parent_node=n,.direction=input.nodes[n].direction});}}
        for(std::size_t n=0;n<input.nodes.size();++n){const auto p=input.nodes[n].parent;
            if(p&&(*p>=input.nodes.size()||!result.nodes[*p].nested_graph))throw std::logic_error("ELK compound node has no parent group");
            const auto g=p?*result.nodes[*p].nested_graph:0;result.nodes[n].graph=g;result.graphs[g].nodes.push_back(n);}
        for(std::size_t e=0;e<input.edges.size();++e){const auto& edge=input.edges[e];
            if(edge.source>=input.nodes.size()||edge.target>=input.nodes.size())throw std::logic_error("ELK compound edge has no node");
            const auto s=port(edge.source,elk_make_compound_port_dummy(result.graphs[result.nodes[edge.source].graph].direction,true,0).side);
            const auto t=port(edge.target,elk_make_compound_port_dummy(result.graphs[result.nodes[edge.target].graph].direction,false,0).side);
            original_source.push_back(s);original_target.push_back(t);outgoing[s].push_back(e);incoming[t].push_back(e);
            if(result.nodes[edge.source].graph==result.nodes[edge.target].graph)segment(result.nodes[edge.source].graph,e,s,t);
        }
        transform(0);
        for(std::size_t p=0;p<port_dummy.size();++p)if(port_dummy[p])result.ports[p].side=result.nodes[*port_dummy[p]].boundary->side;
        for(std::size_t e=0;e<result.edges.size();++e) {
            auto remaining=result.edges[e].segments;std::vector<std::size_t> chain;
            auto current=original_source[e];
            while(!remaining.empty()) {
                auto found=std::ranges::find_if(remaining,[&](auto i){const auto p=result.segments[i].source_port;
                    return p==current||(port_dummy[current]&&result.nodes[*port_dummy[current]].ports.front()==p);});
                if(found==remaining.end())throw std::logic_error("ELK compound edge segments are not connected");
                const auto i=*found;remaining.erase(found);chain.push_back(i);current=result.segments[i].target_port;
                const auto n=result.ports[current].node;if(result.nodes[n].boundary_port)current=*result.nodes[n].boundary_port;
            }
            auto& edge=result.edges[e];edge.segments=std::move(chain);edge.tail_segment=edge.segments.front();edge.head_segment=edge.segments.back();edge.center_segment=edge.tail_segment;
            const auto depth=[&](std::size_t g){std::size_t d=0;while(result.graphs[g].parent_node){++d;g=result.nodes[*result.graphs[g].parent_node].graph;}return d;};
            for(const auto i:edge.segments)if(depth(result.segments[i].graph)<depth(result.segments[edge.center_segment].graph))edge.center_segment=i;
        }
        return std::move(result);
    }
};
}
std::expected<ElkCompoundGraph,std::string> elk_prepare_compound_graph(const ElkCompoundInput& input) {
    try{return Builder{input}.run();}catch(const std::exception& e){return std::unexpected(e.what());}
}
}
