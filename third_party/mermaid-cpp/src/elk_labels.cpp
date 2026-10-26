// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_labels;
import std;

namespace folia::mermaid {
std::expected<ElkLabelPreparation,std::string> elk_prepare_inline_labels(
    const ElkCycleGraph& source,std::span<const ElkInlineCenterLabel> labels,
    ElkLabelDirection direction,double edge_label_spacing,double label_label_spacing) {
    const auto reversed=elk_depth_first_feedback_edges(source);
    if(!reversed)return std::unexpected(reversed.error());
    ElkLayeringResult initial{.layers=std::vector<int>(source.node_count),.reversed_edges=*reversed,
        .layer_nodes={std::vector<std::size_t>(source.node_count)}};
    std::iota(initial.layer_nodes[0].begin(),initial.layer_nodes[0].end(),0);
    auto made=make_elk_layered_graph(source,initial);
    if(!made)return std::unexpected(made.error());
    ElkLabelPreparation result{.graph=std::move(*made)};auto& g=result.graph;
    std::vector<std::vector<ElkInlineCenterLabel>> by_edge(source.edges.size());
    for(const auto& label:labels) {
        if(label.original_edge>=by_edge.size())return std::unexpected("ELK center label has no edge");
        by_edge[label.original_edge].push_back(label);
    }
    // Inserter visits original layerless nodes, then their outgoing port order.
    for(std::size_t n=0;n<source.node_count;++n) {
        const auto ports=g.nodes[n].ports;
        for(const auto p:ports) { const auto outgoing=g.ports[p].outgoing; for(const auto e:outgoing) {
            const auto target=g.edges[e].target_port;
            if(g.ports[target].node==n||by_edge[e].empty())continue;
            const auto dummy=g.nodes.size(),input=g.ports.size(),output=input+1,next=g.edges.size();
            const auto thickness=std::max(0.0,g.edges[e].thickness);
            g.edges[e].thickness=thickness;
            ElkLayerNode node{.kind=ElkLayerNodeKind::label,.ports={input,output},.height=thickness,
                .fixed_port_positions=true,.origin_edge=e,.long_edge_source=g.edges[e].source_port,.long_edge_target=target};
            for(const auto& label:by_edge[e]) {
                if(direction==ElkLabelDirection::horizontal) {
                    node.width=std::max(node.width,label.width);node.height+=label.height+label_label_spacing;
                } else {node.width+=label.width+label_label_spacing;node.height=std::max(node.height,label.height);}
            }
            if(direction==ElkLabelDirection::horizontal)node.height+=edge_label_spacing-label_label_spacing+thickness;
            else {node.width-=label_label_spacing;node.height+=edge_label_spacing+thickness;}
            g.nodes.push_back(std::move(node));result.dummies.push_back({dummy,std::move(by_edge[e])});
            const auto y=std::floor(thickness/2);
            g.ports.push_back({.node=dummy,.side=ElkPortSide::west,.y=y,.incoming={e}});
            g.ports.push_back({.node=dummy,.side=ElkPortSide::east,.y=y,.outgoing={next}});
            std::erase(g.ports[target].incoming,e);g.edges[e].target_port=input;
            auto segment=g.edges[e];segment.source_port=output;segment.target_port=target;
            g.edges.push_back(segment);g.ports[target].incoming.push_back(next);
        }
    }
    }
    ElkCycleGraph expanded{.node_count=g.nodes.size()};
    for(const auto& p:g.ports)expanded.ports.push_back({p.node});
    for(const auto& e:g.edges)expanded.edges.push_back({e.source_port,e.target_port});
    // The prepared graph is acyclic: the existing layerer's DFS does not change
    // it. One edge per port retains original port-adjacency traversal order.
    const auto layered=elk_layer_flat_graph(expanded);
    if(!layered)return std::unexpected(layered.error());
    g.layers=layered->layer_nodes;
    for(std::size_t n=0;n<g.nodes.size();++n)g.nodes[n].layer=layered->layers[n];
    return result;
}
namespace {
std::size_t port_for(const ElkLayeredGraph& g,std::size_t n,bool incoming) {
    for(const auto p:g.nodes[n].ports)
        if(!(incoming?g.ports[p].incoming:g.ports[p].outgoing).empty())return p;
    throw std::logic_error("ELK label chain has no continuation");
}
std::size_t neighbor(const ElkLayeredGraph& g,std::size_t n,bool incoming) {
    const auto& p=g.ports[port_for(g,n,incoming)];
    const auto& e=g.edges[(incoming?p.incoming:p.outgoing).front()];
    return g.ports[incoming?e.source_port:e.target_port].node;
}
void swap_dummy_nodes(ElkLayeredGraph& g,std::size_t a,std::size_t b) {
    const auto ai=port_for(g,a,true),ao=port_for(g,a,false),bi=port_for(g,b,true),bo=port_for(g,b,false);
    const auto ain=g.ports[ai].incoming,aout=g.ports[ao].outgoing;
    const auto bin=g.ports[bi].incoming,bout=g.ports[bo].outgoing;
    for(auto& layer:g.layers)for(auto& n:layer)if(n==a)n=b;else if(n==b)n=a;
    std::swap(g.nodes[a].layer,g.nodes[b].layer);
    for(const auto e:bin)g.edges[e].target_port=ai;
    for(const auto e:bout)g.edges[e].source_port=ao;
    for(const auto e:ain)g.edges[e].target_port=bi;
    for(const auto e:aout)g.edges[e].source_port=bo;
    g.ports[ai].incoming=bin;g.ports[ao].outgoing=bout;
    g.ports[bi].incoming=ain;g.ports[bo].outgoing=aout;
}
}
void elk_switch_label_dummies(ElkLayeredGraph& g) {
    std::vector<std::size_t> labels;
    for(const auto& layer:g.layers)for(const auto n:layer)if(g.nodes[n].kind==ElkLayerNodeKind::label)labels.push_back(n);
    for(const auto n:labels) {
        std::vector<std::size_t> chain;
        for(auto p=neighbor(g,n,true);g.nodes[p].kind==ElkLayerNodeKind::long_edge;p=neighbor(g,p,true))chain.push_back(p);
        std::ranges::reverse(chain);chain.push_back(n);
        for(auto p=neighbor(g,n,false);g.nodes[p].kind==ElkLayerNodeKind::long_edge;p=neighbor(g,p,false))chain.push_back(p);
        const auto median=chain[(chain.size()-1)/2];if(median!=n)swap_dummy_nodes(g,n,median);
        for(auto p=neighbor(g,n,true);g.nodes[p].kind==ElkLayerNodeKind::long_edge;p=neighbor(g,p,true))g.nodes[p].long_edge_before_label=true;
    }
}
void elk_place_inline_label_ports(ElkLayeredGraph& g,std::span<const ElkLabelDummy> dummies,double spacing) {
    for(const auto& dummy:dummies) {
        auto& node=g.nodes[dummy.node];const auto thickness=g.edges[*node.origin_edge].thickness;
        const auto y=std::ceil(node.height-spacing-thickness)/2;
        node.height-=spacing+thickness;
        for(const auto p:node.ports)g.ports[p].y=y;
    }
}
std::vector<ElkLabelPosition> elk_restore_inline_labels(const ElkLayeredGraph& g,
    std::span<const ElkLabelDummy> dummies,std::span<const double> x,std::span<const double> y,
    ElkLabelDirection direction,double spacing) {
    std::vector<ElkLabelPosition> result;
    for(const auto& dummy:dummies) {
        const auto& node=g.nodes[dummy.node];auto px=x[dummy.node],py=y[dummy.node];
        for(std::size_t i=0;i<dummy.labels.size();++i) {
            const auto& label=dummy.labels[direction==ElkLabelDirection::up?dummy.labels.size()-1-i:i];
            const auto lx=direction==ElkLabelDirection::horizontal?px+(node.width-label.width)/2:px;
            const auto ly=direction==ElkLabelDirection::horizontal?py:py+(node.height-label.height)/2;
            result.push_back({label.id,label.original_edge,lx,ly,label.width,label.height});
            if(direction==ElkLabelDirection::horizontal)py+=label.height+spacing;else px+=label.width+spacing;
        }
    }
    std::ranges::sort(result,{},&ElkLabelPosition::id);return result;
}
}

