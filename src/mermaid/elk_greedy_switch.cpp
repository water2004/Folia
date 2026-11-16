// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 GreedySwitchHeuristic, GreedyPortDistributor and their deterministic
// LayerSweepCrossingMinimizer controller, for independent implicit-port graphs.
module folia.mermaid.elk_greedy_switch;
import std;
import folia.mermaid.elk_order_constraints;

namespace folia::mermaid {
namespace {
#include "elk_order_validation.inc"
#include "elk_order_crossings.inc"

struct GreedySwitch {
    ElkLayeredGraph& graph;
    OrderCrossingsCounter crossings;
    explicit GreedySwitch(ElkLayeredGraph& g):graph(g),crossings(g){}

    bool sweep_nodes(std::size_t layer) {
        auto& nodes=graph.layers[layer];bool improved=false;
        for(std::size_t i=0;i+1<nodes.size();++i) {
            // Swapping adjacent node blocks affects precisely the two routing
            // gaps bordering this layer. Their exact chord-count difference
            // equals SwitchDecider's between-layer plus in-layer pair counts.
            const auto before=crossings.count_gap(layer)+crossings.count_gap(layer+1);
            std::swap(nodes[i],nodes[i+1]);
            const auto after=crossings.count_gap(layer)+crossings.count_gap(layer+1);
            if(after<before)improved=true;
            else std::swap(nodes[i],nodes[i+1]);
        }
        return improved;
    }
    bool minimize_nodes(std::size_t layer) {
        bool improved=false;
        while(sweep_nodes(layer))improved=true;
        return improved;
    }
    bool distribute_ports(std::size_t layer,bool forward) {
        const auto side=forward?ElkPortSide::west:ElkPortSide::east;
        const auto gap=forward?layer:layer+1;
        bool improved=false;
        for(const auto n:graph.layers[layer]) {
            auto& node=graph.nodes[n];
            if(node.fixed_port_order||node.fixed_port_positions)continue;
            std::vector<std::size_t> indices;
            for(std::size_t p=0;p<node.ports.size();++p)
                if(graph.ports[node.ports[p]].side==side)indices.push_back(p);
            // Original GreedyPortDistributor walks each side top-to-bottom.
            if(side==ElkPortSide::west)std::ranges::reverse(indices);
            bool changed;
            do {
                changed=false;
                for(std::size_t i=0;i+1<indices.size();++i) {
                    auto& upper=node.ports[indices[i]];auto& lower=node.ports[indices[i+1]];
                    const auto before=crossings.count_gap(gap);
                    std::swap(upper,lower);
                    if(crossings.count_gap(gap)<before)changed=improved=true;
                    else std::swap(upper,lower);
                }
            } while(changed);
        }
        return improved;
    }
    bool sweep(bool forward) {
        const auto first=forward?0:graph.layers.size()-1;
        // setFirstLayerOrder makes one downward pass; minimizeCrossings on
        // the following layers keeps switching until that layer stabilizes.
        bool improved=sweep_nodes(first);
        improved=distribute_ports(first,forward)||improved;
        for(std::size_t step=1;step<graph.layers.size();++step) {
            const auto layer=forward?step:graph.layers.size()-step-1;
            improved=minimize_nodes(layer)||improved;
            improved=distribute_ports(layer,forward)||improved;
        }
        return improved;
    }
};
}

std::expected<std::size_t,std::string> elk_greedy_switch(ElkLayeredGraph& graph,ElkRandom& random) {
    if(const auto valid=supported(graph);!valid)return std::unexpected(valid.error());
    if(graph.layers.empty()||std::ranges::all_of(graph.layers,[](const auto& layer){return layer.empty();})||
        (graph.layers.size()==1&&graph.layers.front().size()==1))return 0;
    // The common original wrapper captures a seed even for deterministic
    // minimizers. Unlike BARYCENTER, this controller never resets that seed.
    static_cast<void>(random.next_long());
    bool forward=random.next(1)!=0;
    GreedySwitch order(graph);
    while(order.sweep(forward))forward=!forward;
    return order.crossings.count_all();
}
}
