// SPDX-License-Identifier: EPL-2.0
// Port of ELK 0.9.3 intermediate.loops for implicit, unshared point ports.
module folia.mermaid.elk_self_loops;
import std;

namespace folia::mermaid {
namespace {
using Side=ElkPortSide;
constexpr auto absent=std::numeric_limits<std::size_t>::max();
std::size_t index(Side side) {return static_cast<std::size_t>(side);}
Side clockwise(Side side) {return side==Side::west?Side::north:static_cast<Side>(index(side)+1);}
Side counterclockwise(Side side) {return side==Side::north?Side::west:static_cast<Side>(index(side)-1);}
constexpr std::array sides{Side::north,Side::east,Side::south,Side::west};

struct Dependency {std::size_t source,target;int weight;bool critical=false,active=true;};
struct Segment {
    std::vector<std::size_t> in_deps,out_deps;
    int mark=0,in_weight=0,out_weight=0,critical_in=0,critical_out=0;
    std::size_t slot=0;
};
#include "elk_routing_cycles.inc"

enum class Alignment {center,left,right,top};
struct Loop {
    const ElkSelfLoop* source=nullptr;
    std::size_t from=0,to=0,left=0,right=0;
    std::array<bool,5> occupied{};
    std::array<std::size_t,5> slots{};
    std::vector<bool> active;
    Side label_side=Side::undefined;
    Alignment alignment=Alignment::center;
    double label_x=0,label_y=0,label_width=0,label_height=0;
    bool has_labels()const {return !source->labels.empty();}
};

void assign_routes(std::vector<Loop>& loops,const ElkLayeredGraph& graph,
    const std::vector<std::size_t>& ports,const std::vector<std::size_t>& order) {
    std::vector<int> penalties;
    int total=0;
    for(const auto p:ports) {
        const auto& port=graph.ports[p];
        total+=port.incoming.empty()&&port.outgoing.empty()?1:3;
        penalties.push_back(total);
    }
    const auto penalty=[&](std::size_t left,std::size_t right) {
        const auto first=order[left],last=order[right]?order[right]-1:ports.size()-1;
        return first<=last?penalties[last]-penalties[first]:
            penalties.back()-penalties[first]+penalties[last];
    };
    for(auto& loop:loops) {
        const auto a=graph.ports[loop.from].side,b=graph.ports[loop.to].side;
        if(a==b) {
            loop.left=order[loop.from]<order[loop.to]?loop.from:loop.to;
            loop.right=loop.left==loop.from?loop.to:loop.from;
        } else if(clockwise(a)==b) {loop.left=loop.from;loop.right=loop.to;}
        else if(clockwise(b)==a) {loop.left=loop.to;loop.right=loop.from;}
        else {
            // RoutingDirector compares both ways around opposing port sides.
            const auto first=index(a)<index(b)?loop.from:loop.to;
            const auto second=first==loop.from?loop.to:loop.from;
            if(penalty(first,second)<=penalty(second,first)) {loop.left=first;loop.right=second;}
            else {loop.left=second;loop.right=first;}
        }
        auto side=graph.ports[loop.left].side;
        loop.occupied[index(side)]=true;
        while(side!=graph.ports[loop.right].side) {
            side=clockwise(side);loop.occupied[index(side)]=true;
        }
        loop.active.resize(ports.size());
        auto p=order[loop.left];
        for(;;) {
            loop.active[p]=true;
            if(p==order[loop.right])break;
            p=(p+1)%ports.size();
        }
    }
}

void align_label(Loop& loop,Side side,Alignment alignment,std::size_t reference,
    const ElkLayeredGraph& graph,double node_width) {
    loop.label_side=side;loop.alignment=alignment;
    switch(alignment) {
    case Alignment::center:loop.label_x=(node_width-loop.label_width)/2;break;
    case Alignment::left:loop.label_x=graph.ports[reference].x;break;
    case Alignment::right:loop.label_x=graph.ports[reference].x-loop.label_width;break;
    case Alignment::top:loop.label_y=graph.ports[reference].y;break;
    }
}

void place_labels(std::vector<Loop>& loops,const ElkLayeredGraph& graph,
    const std::vector<std::size_t>& order,ElkSelfLoopOrdering ordering,
    const ElkSelfLoopSpacing& spacing,double node_width) {
    for(auto& loop:loops) {
        if(!loop.has_labels())continue;
        const bool horizontal=spacing.label_direction==ElkLabelDirection::horizontal;
        for(const auto& label:loop.source->labels) {
            if(horizontal) {
                loop.label_width=std::max(loop.label_width,label.width);
                loop.label_height+=label.height+spacing.label_label;
            } else {
                loop.label_width+=label.width+spacing.label_label;
                loop.label_height=std::max(loop.label_height,label.height);
            }
        }
        if(horizontal)loop.label_height-=spacing.label_label;
        else loop.label_width-=spacing.label_label;
        const auto left=graph.ports[loop.left].side,right=graph.ports[loop.right].side;
        if(left==right) {
            if(left==Side::north||left==Side::south)
                align_label(loop,left,Alignment::center,0,graph,node_width);
            else align_label(loop,left,Alignment::top,
                graph.ports[loop.left].y<=graph.ports[loop.right].y?loop.left:loop.right,graph,node_width);
        } else if(clockwise(left)==right) {
            if(left==Side::north)align_label(loop,Side::north,Alignment::left,loop.left,graph,node_width);
            else if(right==Side::north)align_label(loop,Side::north,Alignment::right,loop.right,graph,node_width);
            else if(left==Side::south)align_label(loop,Side::south,Alignment::right,loop.left,graph,node_width);
            else align_label(loop,Side::south,Alignment::left,loop.right,graph,node_width);
        } else if(loop.occupied[index(Side::north)]) {
            if(loop.occupied[index(Side::south)]) {
                if(loop.occupied[index(Side::west)])
                    align_label(loop,Side::north,Alignment::right,loop.right,graph,node_width);
                else align_label(loop,Side::north,Alignment::left,loop.left,graph,node_width);
            } else align_label(loop,Side::north,Alignment::center,0,graph,node_width);
        } else align_label(loop,Side::south,Alignment::center,0,graph,node_width);
    }
    if(ordering!=ElkSelfLoopOrdering::sequenced)return;
    for(const auto side:{Side::north,Side::south}) {
        std::vector<std::size_t> selected;
        for(std::size_t i=0;i<loops.size();++i)if(loops[i].has_labels()&&
            graph.ports[loops[i].left].side==side&&graph.ports[loops[i].right].side==side)selected.push_back(i);
        std::stable_sort(selected.begin(),selected.end(),[&](auto a,auto b) {
            return side==Side::north?order[loops[a].left]<order[loops[b].left]:order[loops[a].left]>order[loops[b].left];
        });
        for(std::size_t i=0;i<(selected.size()+1)/2;++i) {
            auto& left=loops[selected[i]];auto& right=loops[selected[selected.size()-1-i]];
            if(&left==&right)align_label(left,side,Alignment::center,0,graph,node_width);
            else {
                align_label(left,side,Alignment::right,side==Side::north?left.right:left.left,graph,node_width);
                align_label(right,side,Alignment::left,side==Side::north?right.left:right.right,graph,node_width);
            }
        }
    }
}

void assign_slots(std::vector<Loop>& loops,const ElkLayeredGraph& graph,
    const std::vector<std::size_t>& ports,const std::vector<std::size_t>& order,ElkRandom& random) {
    std::vector<std::vector<bool>> label_crossings(loops.size(),std::vector<bool>(loops.size()));
    std::vector<Segment> segments(loops.size());std::vector<Dependency> dependencies;
    const auto dependency=[&](std::size_t from,std::size_t to,int weight) {
        const auto d=dependencies.size();dependencies.push_back({from,to,weight});
        segments[from].out_deps.push_back(d);segments[to].in_deps.push_back(d);
    };
    for(std::size_t a=0;a<loops.size();++a)for(std::size_t b=a+1;b<loops.size();++b) {
        const auto& first=loops[a];const auto& second=loops[b];
        const bool overlap=first.has_labels()&&second.has_labels()&&first.label_side==second.label_side&&
            (first.label_side==Side::north||first.label_side==Side::south)&&
            first.label_x<=second.label_x+second.label_width&&first.label_x+first.label_width>=second.label_x;
        label_crossings[a][b]=label_crossings[b][a]=overlap;
        const int ab=second.active[order[first.from]]+second.active[order[first.to]];
        const int ba=first.active[order[second.from]]+first.active[order[second.to]];
        if(ab<ba)dependency(a,b,ba-ab);
        else if(ba<ab)dependency(b,a,ab-ba);
        else if(ab||overlap) {dependency(a,b,0);dependency(b,a,0);}
    }
    for(const auto d:routing_dependency_cycles(segments,dependencies,random,false)) {
        auto& edge=dependencies[d];
        std::erase(segments[edge.source].out_deps,d);std::erase(segments[edge.target].in_deps,d);
        if(edge.weight) {
            std::swap(edge.source,edge.target);
            segments[edge.source].out_deps.push_back(d);segments[edge.target].in_deps.push_back(d);
        } else edge.active=false;
    }
    std::deque<std::size_t> sinks;
    for(std::size_t n=0;n<segments.size();++n) {
        segments[n].out_weight=static_cast<int>(segments[n].out_deps.size());
        if(!segments[n].out_weight)sinks.push_back(n);
    }
    while(!sinks.empty()) {
        const auto n=sinks.front();sinks.pop_front();
        for(const auto d:segments[n].in_deps) {
            auto& source=segments[dependencies[d].source];
            source.slot=std::max(source.slot,segments[n].slot+1);
            if(!--source.out_weight)sinks.push_back(dependencies[d].source);
        }
    }
    std::vector<std::size_t> next_free(ports.size());
    for(const auto side:sides) {
        std::vector<std::size_t> selected,side_ports;
        for(std::size_t i=0;i<loops.size();++i)if(loops[i].occupied[index(side)])selected.push_back(i);
        std::stable_sort(selected.begin(),selected.end(),[&](auto a,auto b){return segments[a].slot<segments[b].slot;});
        for(std::size_t p=0;p<ports.size();++p)if(graph.ports[ports[p]].side==side)side_ports.push_back(p);
        if(side_ports.empty()) {
            for(std::size_t i=0;i<selected.size();++i)loops[selected[i]].slots[index(side)]=i;
            continue;
        }
        std::vector<std::optional<std::size_t>> label_slots(loops.size());
        for(const auto i:selected) {
            auto& loop=loops[i];std::size_t slot=0;
            for(const auto p:side_ports)if(loop.active[p])slot=std::max(slot,next_free[p]);
            if(loop.has_labels()) {
                const auto conflict=[&] {
                    for(std::size_t j=0;j<loops.size();++j)
                        if(label_crossings[i][j]&&label_slots[j]==slot)return true;
                    return false;
                };
                while(conflict())++slot;
                label_slots[i]=slot;
            }
            loop.slots[index(side)]=slot;
            for(const auto p:side_ports)if(loop.active[p])next_free[p]=slot+1;
        }
    }
}
}

std::expected<ElkSelfLoopPreparation,std::string> elk_prepare_self_loops(
    const ElkCycleGraph& source,std::span<const ElkInlineCenterLabel> centers,
    std::span<const ElkEndLabel> ends,std::span<const std::size_t> retained_nodes) {
    ElkSelfLoopPreparation result;result.graph.node_count=source.node_count;
    result.node_options.resize(source.node_count);result.port_indices.resize(source.ports.size());
    std::vector<std::optional<std::size_t>> loop_for_edge(source.edges.size());
    std::vector<bool> hidden(source.ports.size());
    for(std::size_t e=0;e<source.edges.size();++e) {
        const auto& edge=source.edges[e];
        if(edge.source_port>=source.ports.size()||edge.target_port>=source.ports.size())
            return std::unexpected("ELK self-loop preprocessing requires valid ports");
        const auto n=source.ports[edge.source_port].node;
        if(n!=source.ports[edge.target_port].node)continue;
        if(n>=source.node_count)return std::unexpected("ELK self-loop has no node");
        const bool hide=std::ranges::find(retained_nodes,n)==retained_nodes.end();
        loop_for_edge[e]=result.loops.size();
        result.loops.push_back({n,e,edge.source_port,edge.target_port,hide});
        hidden[edge.source_port]=hidden[edge.target_port]=hide;
    }
    for(std::size_t p=0;p<source.ports.size();++p)if(!hidden[p]) {
        result.port_indices[p]=result.graph.ports.size();result.port_origins.push_back(p);
        result.graph.ports.push_back(source.ports[p]);
    }
    std::vector<std::size_t> edge_indices(source.edges.size(),absent);
    for(std::size_t e=0;e<source.edges.size();++e)if(!loop_for_edge[e]) {
        const auto& edge=source.edges[e];
        if(!result.port_indices[edge.source_port]||!result.port_indices[edge.target_port])
            return std::unexpected("ELK self-loop processors require unshared implicit ports");
        edge_indices[e]=result.graph.edges.size();result.edge_origins.push_back(e);
        result.graph.edges.push_back({*result.port_indices[edge.source_port],*result.port_indices[edge.target_port]});
    }
    for(auto label:centers) {
        if(label.original_edge>=source.edges.size())return std::unexpected("ELK center label has no edge");
        if(const auto loop=loop_for_edge[label.original_edge])
            result.loops[*loop].labels.push_back({label.id,label.width,label.height,false});
        else {label.original_edge=edge_indices[label.original_edge];result.center_labels.push_back(label);}
    }
    for(auto label:ends) {
        if(label.original_edge>=source.edges.size())return std::unexpected("ELK end label has no edge");
        if(const auto loop=loop_for_edge[label.original_edge])
            result.loops[*loop].labels.push_back({label.id,label.width,label.height,true});
        else {label.original_edge=edge_indices[label.original_edge];result.end_labels.push_back(label);}
    }
    return result;
}

void elk_restore_self_loop_ports(ElkSelfLoopPreparation& prepared,ElkLayeredGraph& graph,
    std::span<const ElkSelfLoopNodeOptions> options) {
    for(std::size_t n=0;n<prepared.node_options.size();++n)if(n<options.size())prepared.node_options[n]=options[n];
    for(std::size_t n=0;n<prepared.graph.node_count;++n) {
        std::vector<const ElkSelfLoop*> loops;
        for(const auto& loop:prepared.loops)if(loop.node==n&&loop.hidden_ports)loops.push_back(&loop);
        if(loops.empty())continue;
        std::stable_sort(loops.begin(),loops.end(),[](auto a,auto b){return a->source_port<b->source_port;});
        const auto settings=prepared.node_options[n];
        std::array<std::array<std::vector<std::size_t>,3>,5> areas;
        struct Pair {std::size_t source,target;Side first,second;};
        std::vector<Pair> restored;
        for(std::size_t i=0;i<loops.size();++i) {
            const auto& loop=*loops[i];
            constexpr std::array<std::pair<Side,Side>,8> targets{{
                {Side::north,Side::north},{Side::south,Side::south},
                {Side::east,Side::east},{Side::west,Side::west},
                {Side::west,Side::north},{Side::north,Side::east},
                {Side::south,Side::west},{Side::east,Side::south}}};
            auto target=std::pair{Side::north,Side::north};
            if(settings.distribution==ElkSelfLoopDistribution::equally)target=targets[i%targets.size()];
            else if(settings.distribution==ElkSelfLoopDistribution::north_south&&i%2)target={Side::south,Side::south};
            const auto from=graph.ports.size(),to=from+1;
            graph.ports.push_back({.node=n,.side=target.first});graph.ports.push_back({.node=n,.side=target.second});
            prepared.port_indices[loop.source_port]=from;prepared.port_indices[loop.target_port]=to;
            restored.push_back({from,to,target.first,target.second});
        }
        const auto one_side=[&](const Pair& pair) {
            auto& middle=areas[index(pair.first)][1];
            if(settings.ordering==ElkSelfLoopOrdering::sequenced) {
                middle.push_back(pair.target);middle.push_back(pair.source);
            } else {middle.insert(middle.begin(),pair.source);middle.push_back(pair.target);}
        };
        if(settings.ordering==ElkSelfLoopOrdering::reverse_stacked) {
            for(const auto& pair:restored|std::views::reverse)if(pair.first==pair.second)one_side(pair);
        } else for(const auto& pair:restored)if(pair.first==pair.second)one_side(pair);
        for(const auto& pair:restored)if(pair.first!=pair.second) {
            auto& end=areas[index(pair.first)][2];end.insert(end.begin(),pair.source);
            areas[index(pair.second)][0].push_back(pair.target);
        }
        const auto previous=std::move(graph.nodes[n].ports);auto& ports=graph.nodes[n].ports;ports.clear();
        for(const auto side:sides) {
            const auto append=[&](std::size_t area) {const auto& values=areas[index(side)][area];ports.insert(ports.end(),values.begin(),values.end());};
            append(0);
            if(side!=Side::west)append(1);
            for(const auto p:previous)if(graph.ports[p].side==side)ports.push_back(p);
            if(side==Side::west)append(1);
            append(2);
        }
    }
}

ElkSelfLoopGeometry elk_route_self_loops(const ElkSelfLoopPreparation& prepared,
    ElkLayeredGraph& graph,std::span<ElkHorizontalMargin> margins,ElkRandom& random,
    const ElkSelfLoopSpacing& spacing) {
    ElkSelfLoopGeometry result;
    // The original processor visits the final crossing order. Slot cycle
    // breaking consumes the graph RNG in that same node traversal order.
    for(const auto& layer:graph.layers)for(const auto n:layer) {
        if(n>=prepared.graph.node_count)continue;
        auto& node=graph.nodes[n];const auto& ports=node.ports;
        std::vector<Loop> loops;
        for(const auto& source:prepared.loops)if(source.node==n)
            loops.push_back({.source=&source,.from=*prepared.port_indices[source.source_port],.to=*prepared.port_indices[source.target_port]});
        if(loops.empty())continue;
        std::stable_sort(loops.begin(),loops.end(),[](const auto& a,const auto& b){return a.source->source_port<b.source->source_port;});
        std::vector<std::size_t> order(graph.ports.size());
        for(std::size_t i=0;i<ports.size();++i)order[ports[i]]=i;
        assign_routes(loops,graph,ports,order);
        place_labels(loops,graph,order,prepared.node_options[n].ordering,spacing,node.width);
        assign_slots(loops,graph,ports,order,random);
        std::array<std::vector<double>,5> positions;
        for(const auto& loop:loops)for(const auto side:sides)if(loop.occupied[index(side)])
            positions[index(side)].resize(std::max(positions[index(side)].size(),loop.slots[index(side)]+1));
        for(const auto& loop:loops)if(loop.has_labels()&&(loop.label_side==Side::north||loop.label_side==Side::south)) {
            auto& height=positions[index(loop.label_side)][loop.slots[index(loop.label_side)]];
            height=std::max(height,loop.label_height);
        }
        const std::array baseline{0.0,-node.margin_top-spacing.node_loop,
            node.width+margins[n].right+spacing.node_loop,
            node.height+node.margin_bottom+spacing.node_loop,-margins[n].left-spacing.node_loop};
        for(const auto side:sides) {
            double current=baseline[index(side)];const double sign=side==Side::north||side==Side::west?-1:1;
            for(auto& position:positions[index(side)]) {
                const auto label_height=position;position=current;
                current+=sign*(spacing.edge_edge+label_height+(label_height>0?spacing.edge_label:0));
            }
        }
        const auto reserve=[&](ElkRoutingPoint p) {
            margins[n].left=std::max(margins[n].left,-p.x);margins[n].right=std::max(margins[n].right,p.x-node.width);
            node.margin_top=std::max(node.margin_top,-p.y);node.margin_bottom=std::max(node.margin_bottom,p.y-node.height);
        };
        for(auto& loop:loops) {
            const auto& from=graph.ports[loop.from];const auto& to=graph.ports[loop.to];
            const auto base=[&](Side side) {
                const auto p=positions[index(side)][loop.slots[index(side)]];
                return side==Side::north||side==Side::south?ElkRoutingPoint{0,p}:ElkRoutingPoint{p,0};
            };
            const auto outer=[&](const ElkLayerPort& port) {
                auto p=base(port.side);
                if(port.side==Side::north||port.side==Side::south)p.x=port.x;else p.y=port.y;
                return p;
            };
            ElkRestoredRoute route{.original_edge=loop.source->original_edge,.source_node=n,.target_node=n,
                .source_port=loop.source->source_port,.target_port=loop.source->target_port,
                .start={from.x,from.y},.end={to.x,to.y},.bends={outer(from)}};
            const bool forward=from.side==to.side?order[loop.from]<order[loop.to]:
                clockwise(from.side)==to.side?true:counterclockwise(from.side)==to.side?false:loop.occupied[index(clockwise(from.side))];
            for(auto side=from.side;side!=to.side;) {
                const auto next=forward?clockwise(side):counterclockwise(side);
                const auto a=base(side),b=base(next);route.bends.push_back({a.x+b.x,a.y+b.y});side=next;
            }
            route.bends.push_back(outer(to));
            for(const auto p:route.bends)reserve(p);
            result.routes.push_back(std::move(route));
            if(!loop.has_labels())continue;
            const auto position=positions[index(loop.label_side)][loop.slots[index(loop.label_side)]];
            switch(loop.label_side) {
            case Side::north:loop.label_y=position-spacing.edge_label-loop.label_height;break;
            case Side::south:loop.label_y=position+spacing.edge_label;break;
            case Side::west:loop.label_x=position-spacing.edge_label-loop.label_width;break;
            case Side::east:loop.label_x=position+spacing.edge_label;break;
            default:break;
            }
            reserve({loop.label_x,loop.label_y});reserve({loop.label_x+loop.label_width,loop.label_y+loop.label_height});
            double x=loop.label_x,y=loop.label_y;
            for(const auto& label:loop.source->labels) {
                auto lx=x,ly=y;
                if(spacing.label_direction==ElkLabelDirection::horizontal) {
                    if(loop.alignment==Alignment::left||loop.label_side==Side::east) {}
                    else if(loop.alignment==Alignment::right||loop.label_side==Side::west)lx+=loop.label_width-label.width;
                    else lx+=(loop.label_width-label.width)/2;
                    y+=label.height+spacing.label_label;
                } else {
                    if(loop.label_side==Side::north)ly+=loop.label_height-label.height;
                    x+=label.width+spacing.label_label;
                }
                result.labels.push_back({n,label.terminal,{label.id,loop.source->original_edge,lx,ly,label.width,label.height}});
            }
        }
    }
    return result;
}

void elk_restore_self_loops(const ElkSelfLoopPreparation& prepared,const ElkSelfLoopGeometry& loops,
    std::span<const double> x,std::span<const double> y,std::vector<ElkRestoredRoute>& routes,
    std::vector<ElkLabelPosition>& centers,std::vector<ElkLabelPosition>& ends) {
    for(auto& route:routes) {
        route.original_edge=prepared.edge_origins[route.original_edge];
        route.source_port=prepared.port_origins[route.source_port];route.target_port=prepared.port_origins[route.target_port];
    }
    for(auto* labels:{&centers,&ends})for(auto& label:*labels)label.original_edge=prepared.edge_origins[label.original_edge];
    for(auto route:loops.routes) {
        const auto shift=[&](ElkRoutingPoint& p) {p.x+=x[route.source_node];p.y+=y[route.source_node];};
        shift(route.start);shift(route.end);for(auto& p:route.bends)shift(p);
        routes.push_back(std::move(route));
    }
    for(const auto& value:loops.labels) {
        auto label=value.label;label.x+=x[value.node];label.y+=y[value.node];
        (value.terminal?ends:centers).push_back(label);
    }
    std::ranges::sort(routes,{},&ElkRestoredRoute::original_edge);
    std::ranges::sort(centers,{},&ElkLabelPosition::id);std::ranges::sort(ends,{},&ElkLabelPosition::id);
}
}
