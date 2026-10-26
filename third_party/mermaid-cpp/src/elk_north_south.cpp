// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 NorthSouthPortPreprocessor/Postprocessor, implicit point ports.
module folia.mermaid.elk_north_south;
import std;

namespace folia::mermaid {
std::expected<ElkNorthSouthPreparation,std::string> elk_prepare_north_south_ports(ElkLayeredGraph& g) {
    for(const auto& p:g.ports)if(p.side==ElkPortSide::north||p.side==ElkPortSide::south) {
        if(p.incoming.size()+p.outgoing.size()>1)return std::unexpected("ELK north/south preprocessing requires implicit one-edge ports");
        for(const auto* edges:{&p.incoming,&p.outgoing})for(const auto e:*edges)
            if(g.ports[g.edges[e].source_port].node==g.ports[g.edges[e].target_port].node)
                return std::unexpected("ELK north/south self loops require their own processor");
    }
    ElkNorthSouthPreparation result;
    result.barycenter_associates.resize(g.nodes.size());result.layout_units.resize(g.nodes.size());
    for(std::size_t l=0;l<g.layers.size();++l) {
        const auto original_nodes=g.layers[l];
        for(const auto n:original_nodes) {
            if(g.nodes[n].kind!=ElkLayerNodeKind::normal)continue;
            result.layout_units[n]=n;
            if(!g.nodes[n].fixed_port_positions&&!g.nodes[n].fixed_port_order) {
                const auto size=g.nodes[n].ports.size();std::size_t incoming=0,outgoing=2*size;
                std::vector<std::size_t> ranks(g.ports.size());
                for(const auto p:g.nodes[n].ports)if(g.ports[p].side==ElkPortSide::north||g.ports[p].side==ElkPortSide::south)
                    ranks[p]=g.ports[p].outgoing.empty()?incoming++:outgoing++;
                std::stable_sort(g.nodes[n].ports.begin(),g.nodes[n].ports.end(),[&](auto a,auto b) {
                    const auto side=g.ports[a].side;
                    if(side!=g.ports[b].side)return side<g.ports[b].side;
                    return side==ElkPortSide::north?ranks[a]<ranks[b]:ranks[a]>ranks[b];
                });
            }
            for(const auto side:{ElkPortSide::north,ElkPortSide::south}) {
                std::vector<std::size_t> ports;
                for(const auto p:g.nodes[n].ports)if(g.ports[p].side==side)ports.push_back(p);
                if(side==ElkPortSide::south)std::ranges::reverse(ports);
                std::vector<std::size_t> created;
                for(const bool input:{true,false})for(const auto p:ports) {
                    const auto edges=input?g.ports[p].incoming:g.ports[p].outgoing;
                    if(edges.empty())continue;
                    const auto dummy=g.nodes.size(),proxy=g.ports.size();
                    g.nodes.push_back({.kind=ElkLayerNodeKind::north_south,.layer=l,.ports={proxy},.fixed_port_positions=true});
                    g.ports.push_back({.node=dummy,.side=input?ElkPortSide::west:ElkPortSide::east});
                    for(const auto e:edges) {
                        if(input){g.edges[e].target_port=proxy;g.ports[proxy].incoming.push_back(e);}
                        else {g.edges[e].source_port=proxy;g.ports[proxy].outgoing.push_back(e);}
                    }
                    if(input)g.ports[p].incoming.clear();else g.ports[p].outgoing.clear();
                    result.dummies.push_back({dummy,n,p,proxy,side});created.push_back(dummy);
                    result.barycenter_associates[n].push_back(dummy);result.barycenter_associates.emplace_back();result.layout_units.push_back(n);
                    result.successors.emplace_back(side==ElkPortSide::north?dummy:n,side==ElkPortSide::north?n:dummy);
                }
                if(side==ElkPortSide::north)std::ranges::reverse(created);
                const auto found=std::ranges::find(g.layers[l],n);
                g.layers[l].insert(side==ElkPortSide::north?found:found+1,created.begin(),created.end());
            }
        }
    }
    return result;
}

void elk_restore_north_south_ports(ElkLayeredGraph& g,const ElkNorthSouthPreparation& prepared,
    ElkOrthogonalRoutes& routing,std::span<const double> node_y) {
    for(const auto& layer:g.layers)for(const auto n:layer) {
        const auto found=std::ranges::find(prepared.dummies,n,&ElkNorthSouthDummy::node);
        if(found==prepared.dummies.end())continue;
        const auto origin=found->origin_port,proxy=found->proxy_port;
        const ElkRoutingPoint bend{routing.node_x[found->origin_node]+g.ports[origin].x,node_y[n]};
        const auto incoming=g.ports[proxy].incoming,outgoing=g.ports[proxy].outgoing;
        for(const auto e:incoming){g.edges[e].target_port=origin;g.ports[origin].incoming.push_back(e);routing.bends[e].push_back(bend);}
        for(const auto e:outgoing){g.edges[e].source_port=origin;g.ports[origin].outgoing.push_back(e);routing.bends[e].insert(routing.bends[e].begin(),bend);}
        g.ports[proxy].incoming.clear();g.ports[proxy].outgoing.clear();
    }
    for(auto& layer:g.layers)std::erase_if(layer,[&](auto n){return g.nodes[n].kind==ElkLayerNodeKind::north_south;});
}
}
