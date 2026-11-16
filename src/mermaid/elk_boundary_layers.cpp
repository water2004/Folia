// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_boundary_layers;
import std;

namespace folia::mermaid {
std::expected<std::vector<std::size_t>,std::string> elk_boundary_feedback_edges(
    const ElkCycleGraph& source,std::span<const ElkExternalLayerRole> roles) {
    if(roles.size()!=source.node_count)return std::unexpected("ELK boundary roles must match nodes");
    auto graph=source;
    std::vector<std::vector<std::size_t>> ports(source.node_count),incoming(source.ports.size()),outgoing(source.ports.size());
    for(std::size_t p=0;p<source.ports.size();++p) {
        if(source.ports[p].node>=source.node_count)return std::unexpected("ELK cycle port has no node");
        ports[source.ports[p].node].push_back(p);
    }
    for(std::size_t e=0;e<source.edges.size();++e) {
        const auto& edge=source.edges[e];
        if(edge.source_port>=source.ports.size()||edge.target_port>=source.ports.size())
            return std::unexpected("ELK cycle edge has no port");
        outgoing[edge.source_port].push_back(e);incoming[edge.target_port].push_back(e);
    }
    std::vector<std::size_t> result;
    std::vector<bool> reversed(source.edges.size());
    const auto reverse=[&](std::size_t e) {
        auto& edge=graph.edges[e];
        std::erase(outgoing[edge.source_port],e);std::erase(incoming[edge.target_port],e);
        std::swap(edge.source_port,edge.target_port);
        outgoing[edge.source_port].push_back(e);incoming[edge.target_port].push_back(e);
        reversed[e]=true;result.push_back(e);
    };
    // Original handleOuterNodes/reverseEdges visits nodes and their ports in
    // model order. Edges between equally constrained endpoints are retained;
    // an edge reversed by a previous boundary must not be reversed twice here.
    for(std::size_t n=0;n<source.node_count;++n) {
        const auto role=roles[n];
        if(role==ElkExternalLayerRole::none)continue;
        for(const auto p:ports[n]) {
            const auto incident=role==ElkExternalLayerRole::first_separate?incoming[p]:outgoing[p];
            for(const auto e:incident) {
                const auto& edge=graph.edges[e];
                const auto opposite=role==ElkExternalLayerRole::first_separate?edge.source_port:edge.target_port;
                if(!reversed[e]&&roles[source.ports[opposite].node]!=role)reverse(e);
            }
        }
    }
    const auto feedback=elk_depth_first_feedback_edges(graph);
    if(!feedback)return std::unexpected(feedback.error());
    result.insert(result.end(),feedback->begin(),feedback->end());
    return result;
}
std::expected<ElkLayeringResult,std::string> elk_layer_boundary_graph(
    const ElkCycleGraph& source,std::span<const ElkExternalLayerRole> roles) {
    if(roles.size()!=source.node_count)return std::unexpected("ELK boundary roles must match nodes");
    const auto reversed=elk_boundary_feedback_edges(source,roles);if(!reversed)return std::unexpected(reversed.error());
    auto edges=source.edges;
    std::vector<std::vector<std::size_t>> ports(source.node_count),incoming(source.ports.size()),outgoing(source.ports.size());
    std::vector<std::size_t> degree(source.node_count);
    for(std::size_t p=0;p<source.ports.size();++p)ports[source.ports[p].node].push_back(p);
    for(const auto e:*reversed)std::swap(edges[e].source_port,edges[e].target_port);
    for(std::size_t e=0;e<edges.size();++e) {
        outgoing[edges[e].source_port].push_back(e);incoming[edges[e].target_port].push_back(e);
        ++degree[source.ports[edges[e].source_port].node];++degree[source.ports[edges[e].target_port].node];
    }
    std::vector<bool> visible(edges.size(),true);
    std::vector<int> connections(source.node_count),inferred(source.node_count);
    for(std::size_t n=0;n<source.node_count;++n)if(roles[n]!=ElkExternalLayerRole::none) {
        for(const auto p:ports[n])for(const auto* list:{&incoming[p],&outgoing[p]})for(const auto e:*list)if(visible[e]) {
            const auto& edge=edges[e];const bool is_out=edge.source_port==p;
            const auto opposite=source.ports[is_out?edge.target_port:edge.source_port].node;
            if((roles[n]==ElkExternalLayerRole::first_separate&&!is_out||roles[n]==ElkExternalLayerRole::last_separate&&is_out)
                &&roles[opposite]==ElkExternalLayerRole::none)
                return std::unexpected("ELK separate boundary has an incompatible incident edge");
            visible[e]=false;--degree[opposite];
            if(roles[opposite]!=ElkExternalLayerRole::none||inferred[opposite])continue;
            connections[opposite]|=roles[n]==ElkExternalLayerRole::first_separate?1:2;
            if(degree[opposite]==0&&(connections[opposite]==1||connections[opposite]==2))inferred[opposite]=connections[opposite];
        }
    }
    const auto absent=std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> native_nodes,local_node(source.node_count,absent),local_port(source.ports.size(),absent);
    ElkCycleGraph internal;
    for(std::size_t n=0;n<source.node_count;++n)if(roles[n]==ElkExternalLayerRole::none){local_node[n]=native_nodes.size();native_nodes.push_back(n);}
    internal.node_count=native_nodes.size();
    for(std::size_t p=0;p<source.ports.size();++p)if(local_node[source.ports[p].node]!=absent){local_port[p]=internal.ports.size();internal.ports.push_back({local_node[source.ports[p].node]});}
    for(std::size_t e=0;e<edges.size();++e)if(visible[e])internal.edges.push_back({local_port[edges[e].source_port],local_port[edges[e].target_port]});
    const auto layered=elk_layer_flat_graph(internal);if(!layered)return std::unexpected(layered.error());
    ElkLayeringResult result{.layers=std::vector<int>(source.node_count),.reversed_edges=*reversed};
    for(const auto& layer:layered->layer_nodes){auto& target=result.layer_nodes.emplace_back();for(const auto n:layer)target.push_back(native_nodes[n]);}
    if(!result.layer_nodes.empty()) {
        const auto last=result.layer_nodes.size()-1;
        for(std::size_t l=0;l<result.layer_nodes.size();++l) {
            const auto snapshot=result.layer_nodes[l];
            for(const auto n:snapshot)if(inferred[n]) {
                std::erase(result.layer_nodes[l],n);result.layer_nodes[inferred[n]==1?0:last].push_back(n);
            }
        }
        std::erase_if(result.layer_nodes,[](const auto& layer){return layer.empty();});
    }
    std::vector<std::size_t> first,last;
    for(std::size_t n=0;n<source.node_count;++n){if(roles[n]==ElkExternalLayerRole::first_separate)first.push_back(n);else if(roles[n]==ElkExternalLayerRole::last_separate)last.push_back(n);}
    if(!first.empty())result.layer_nodes.insert(result.layer_nodes.begin(),std::move(first));
    if(!last.empty())result.layer_nodes.push_back(std::move(last));
    for(std::size_t l=0;l<result.layer_nodes.size();++l)for(const auto n:result.layer_nodes[l])result.layers[n]=static_cast<int>(l);
    return result;
}
}
