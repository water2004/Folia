// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_order_constraints;
import std;

namespace folia::mermaid {
ElkOrderConstraints elk_make_order_constraints(const ElkLayeredGraph& g,const ElkNorthSouthPreparation& p) {
    ElkOrderConstraints result{.normal=std::vector<bool>(g.nodes.size()),.units=std::vector<std::vector<std::size_t>>(g.nodes.size()),
        .successors=std::vector<std::vector<std::size_t>>(g.nodes.size())};
    for(const auto& layer:g.layers)for(const auto n:layer) {
        result.normal[n]=g.nodes[n].kind==ElkLayerNodeKind::normal;
        if(n<p.layout_units.size()&&p.layout_units[n])result.units[*p.layout_units[n]].push_back(n);
    }
    for(const auto [a,b]:p.successors)result.successors[a].push_back(b);
    return result;
}

void elk_resolve_order_constraints(const ElkOrderConstraints& c,std::vector<std::size_t>& layer,
    std::span<std::optional<double>> barycenters) {
    struct Group {std::vector<std::size_t> nodes,outgoing;std::size_t incoming=0;};
    std::vector<Group> groups(c.normal.size());std::vector<std::size_t> order=layer;
    for(const auto n:layer)groups[n].nodes={n};
    std::optional<std::size_t> last;
    for(const auto n:layer) {
        for(const auto to:c.successors[n]){groups[n].outgoing.push_back(to);++groups[to].incoming;}
        if(c.normal[n]) {
            if(last)for(const auto a:c.units[*last])for(const auto b:c.units[n]){groups[a].outgoing.push_back(b);++groups[b].incoming;}
            last=n;
        }
    }
    const auto value=[&](std::size_t group){return *barycenters[groups[group].nodes.front()];};
    const auto erase_one=[](auto& list,auto value){const auto it=std::ranges::find(list,value);if(it==list.end())return false;list.erase(it);return true;};
    for(;;) {
        std::vector<std::vector<std::size_t>> incoming(groups.size());std::deque<std::size_t> active;
        for(const auto g:order)if(!groups[g].outgoing.empty()&&!groups[g].incoming)active.push_back(g);
        std::optional<std::pair<std::size_t,std::size_t>> violation;
        while(!active.empty()&&!violation) {
            const auto g=active.front();active.pop_front();
            for(const auto predecessor:incoming[g]) {
                const auto a=value(predecessor),b=value(g);
                if(a==b?std::ranges::find(order,predecessor)>std::ranges::find(order,g):a>b){violation={{predecessor,g}};break;}
            }
            if(violation)break;
            for(const auto successor:groups[g].outgoing) {
                incoming[successor].insert(incoming[successor].begin(),g);
                if(groups[successor].incoming==incoming[successor].size())active.push_back(successor);
            }
        }
        if(!violation)break;
        const auto [a,b]=*violation;Group merged;merged.nodes=groups[a].nodes;
        merged.nodes.insert(merged.nodes.end(),groups[b].nodes.begin(),groups[b].nodes.end());
        merged.outgoing=groups[a].outgoing;erase_one(merged.outgoing,b);
        for(const auto next:groups[b].outgoing)if(next!=a) {
            if(std::ranges::find(merged.outgoing,next)!=merged.outgoing.end())--groups[next].incoming;
            else merged.outgoing.push_back(next);
        }
        // Original ConstraintGroup degree/weight fields remain zero; merging
        // uses the mean of the two groups' current barycenters.
        const auto mean=(value(a)+value(b))/2;
        for(const auto n:merged.nodes)barycenters[n]=mean;
        const auto id=groups.size();groups.push_back(std::move(merged));
        bool inserted=false;
        for(std::size_t i=0;i<order.size();) {
            const auto g=order[i];
            if(g==a||g==b){order.erase(order.begin()+i);continue;}
            // ListIterator.previous()+add() leaves the original group as the
            // next item. Revisit it so its constraints target the merged group.
            if(!inserted&&value(g)>mean){order.insert(order.begin()+i,id);inserted=true;++i;continue;}
            const auto from_a=erase_one(groups[g].outgoing,a),from_b=erase_one(groups[g].outgoing,b);
            if(from_a||from_b){groups[g].outgoing.push_back(id);++groups[id].incoming;}
            ++i;
        }
        if(!inserted)order.push_back(id);
    }
    layer.clear();for(const auto g:order)layer.insert(layer.end(),groups[g].nodes.begin(),groups[g].nodes.end());
}

std::size_t elk_count_north_south_crossings(const ElkLayeredGraph& g,const ElkNorthSouthPreparation& p,
    std::span<const std::size_t> layer) {
    const auto absent=std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> partner(g.ports.size(),absent),ports,stack;
    for(const auto& d:p.dummies){partner[d.origin_port]=d.proxy_port;partner[d.proxy_port]=d.origin_port;}
    const auto flush=[&]{while(!stack.empty()){ports.push_back(stack.back());stack.pop_back();}};
    std::optional<std::size_t> last_unit;
    for(const auto n:layer) {
        const auto unit=n<p.layout_units.size()?p.layout_units[n]:std::optional<std::size_t>{};
        if(last_unit&&*last_unit!=n&&unit&&unit!=last_unit)flush();
        if(unit)last_unit=unit;
        const auto& node=g.nodes[n];
        if(node.kind==ElkLayerNodeKind::normal) {
            for(const auto port:node.ports)if(g.ports[port].side==ElkPortSide::north&&partner[port]!=absent)ports.push_back(port);
            flush();
            for(const auto port:node.ports)if(g.ports[port].side==ElkPortSide::south&&partner[port]!=absent)ports.push_back(port);
        } else if(node.kind==ElkLayerNodeKind::north_south||node.kind==ElkLayerNodeKind::long_edge) {
            std::optional<std::size_t> west,east;
            for(const auto port:node.ports)if(g.ports[port].side==ElkPortSide::west){ports.push_back(port);west=port;}
                else if(g.ports[port].side==ElkPortSide::east){stack.push_back(port);east=port;}
            if(node.kind==ElkLayerNodeKind::long_edge&&west&&east){partner[*west]=*east;partner[*east]=*west;}
        }
    }
    flush();std::vector<std::size_t> positions(g.ports.size());for(std::size_t i=0;i<ports.size();++i)positions[ports[i]]=i;
    std::vector<std::size_t> ends;std::size_t crossings=0;
    for(std::size_t i=0;i<ports.size();++i) {
        std::erase(ends,i);const auto other=partner[ports[i]];if(other==absent)continue;const auto end=positions[other];
        if(end>i){crossings+=std::ranges::count_if(ends,[&](auto active){return active<end;});ends.push_back(end);}
    }
    return crossings;
}
}
