// SPDX-License-Identifier: EPL-2.0
// ELK CrossingsCounter for the existing implicit, one-edge E/W ports. Both
// minimizers use the same chord count, including same-side in-layer edges.
class OrderCrossingsCounter {
    const ElkLayeredGraph& graph;
    std::vector<std::size_t> positions,ports,tree;
public:
    explicit OrderCrossingsCounter(const ElkLayeredGraph& g)
        :graph(g),positions(g.ports.size(),g.ports.size()){}

    std::size_t count_gap(std::size_t gap) {
        ports.clear();
        if(gap>0)for(const auto n:graph.layers[gap-1])for(const auto p:graph.nodes[n].ports)
            if(graph.ports[p].side==ElkPortSide::east)ports.push_back(p);
        if(gap<graph.layers.size())for(const auto n:graph.layers[gap]|std::views::reverse)
            for(const auto p:graph.nodes[n].ports)
                if(graph.ports[p].side==ElkPortSide::west)ports.push_back(p);
        for(std::size_t i=0;i<ports.size();++i)positions[ports[i]]=i;
        tree.assign(ports.size()+1,0);
        const auto update=[&](std::size_t index,bool add) {
            for(auto i=index+1;i<tree.size();i+=i&(std::size_t{0}-i))
                if(add)++tree[i];else --tree[i];
        };
        // Ports around a routing gap are counterclockwise. A chord crosses
        // each currently open chord whose endpoint precedes its own endpoint.
        std::size_t total=0;
        for(std::size_t i=0;i<ports.size();++i) {
            const auto& port=graph.ports[ports[i]];
            if(port.incoming.empty()&&port.outgoing.empty())continue;
            const auto peer=port.incoming.empty()?graph.edges[port.outgoing.front()].target_port:
                graph.edges[port.incoming.front()].source_port;
            const auto end=positions[peer];
            if(end>=ports.size())continue;
            if(end<i)update(i,false);
            else {
                for(auto j=end;j;j&=j-1)total+=tree[j];
                update(end,true);
            }
        }
        for(const auto p:ports)positions[p]=graph.ports.size();
        return total;
    }
    std::size_t count_all() {
        std::size_t result=0;
        for(std::size_t gap=0;gap<=graph.layers.size();++gap)result+=count_gap(gap);
        return result;
    }
};
