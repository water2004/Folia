// SPDX-License-Identifier: EPL-2.0
// Port of ELK 0.9.3 OrthogonalRoutingGenerator and OrthogonalEdgeRouter.
module folia.mermaid.elk_routing;
import std;

namespace folia::mermaid {
namespace {
constexpr double epsilon = 1e-3;
struct Segment {
    std::vector<double> incoming, outgoing;
    std::vector<std::size_t> in_deps, out_deps;
    std::size_t edge = 0;
    std::optional<std::size_t> partner, split_by;
    double start = 0, end = 0;
    int mark = 0, in_weight = 0, out_weight = 0, critical_in = 0, critical_out = 0;
    std::size_t slot = 0;
};
struct Dependency { std::size_t source, target; int weight; bool critical, active = true; };
#include "elk_routing_cycles.inc"
int crossings(const std::vector<double>& positions, double start, double end) {
    return static_cast<int>(std::ranges::count_if(positions,[&](double p){return p>=start&&p<=end;}));
}
void extent(Segment& s) {
    s.start=std::numeric_limits<double>::infinity(); s.end=-s.start;
    for (const auto p:s.incoming) {s.start=std::min(s.start,p);s.end=std::max(s.end,p);}
    for (const auto p:s.outgoing) {s.start=std::min(s.start,p);s.end=std::max(s.end,p);}
}
struct RoutingGap {
    const ElkLayeredGraph& g;
    std::span<const double> y;
    ElkRoutingSpacing spacing;
    ElkRandom& random;
    std::vector<Segment> segments;
    std::vector<Dependency> deps;
    double critical_threshold = 0;
    RoutingGap(const ElkLayeredGraph& graph, std::span<const double> positions,
        ElkRoutingSpacing distances, ElkRandom& rng) : g(graph),y(positions),spacing(distances),random(rng) {}
    void dependency(std::size_t source,std::size_t target,int weight,bool critical) {
        const auto d=deps.size(); deps.push_back({source,target,weight,critical});
        segments[source].out_deps.push_back(d); segments[target].in_deps.push_back(d);
    }
    void remove(std::size_t d) {
        auto& e=deps[d];
        std::erase(segments[e.source].out_deps,d); std::erase(segments[e.target].in_deps,d); e.active=false;
    }
    void reverse(std::size_t d) {
        auto& e=deps[d];
        std::erase(segments[e.source].out_deps,d); std::erase(segments[e.target].in_deps,d);
        std::swap(e.source,e.target);
        segments[e.source].out_deps.push_back(d); segments[e.target].in_deps.push_back(d);
    }
    int conflicts(const std::vector<double>& a,const std::vector<double>& b) const {
        if (a.empty()||b.empty()) return 0;
        std::size_t i=0,j=0; int result=0;
        for (;;) {
            const auto x=a[i],z=b[j];
            if (x>z-critical_threshold&&x<z+critical_threshold) return -1;
            const auto threshold=spacing.edge_edge_between_layers*.5;
            if (x>z-threshold&&x<z+threshold) ++result;
            if (x<=z&&i+1<a.size()) ++i;
            else if (z<=x&&j+1<b.size()) ++j;
            else break;
        }
        return result;
    }
    int create_dependencies(std::size_t a,std::size_t b) {
        const auto& first=segments[a]; const auto& second=segments[b];
        if (std::abs(first.start-first.end)<epsilon||std::abs(second.start-second.end)<epsilon) return 0;
        const auto c1=conflicts(first.outgoing,second.incoming),c2=conflicts(second.outgoing,first.incoming);
        if (c1==-1||c2==-1) {
            if(c1==-1) dependency(b,a,1,true);
            if(c2==-1) dependency(a,b,1,true);
            return (c1==-1)+(c2==-1);
        }
        const auto v1=c1+16*(crossings(first.outgoing,second.start,second.end)+crossings(second.incoming,first.start,first.end));
        const auto v2=c2+16*(crossings(second.outgoing,first.start,first.end)+crossings(first.incoming,second.start,second.end));
        if(v1<v2) dependency(a,b,v2-v1,false);
        else if(v1>v2) dependency(b,a,v1-v2,false);
        else if(v1>0) {dependency(a,b,0,false);dependency(b,a,0,false);}
        return 0;
    }
    std::vector<std::size_t> cycles(bool critical_only) {
        return routing_dependency_cycles(segments,deps,random,critical_only);
    }
    struct Area {double start,end;};
    struct Rating {int dependencies=0,crossings=0;};
    static void rate_orderings(Rating& r,const Segment& a,const Segment& b) {
        const auto c1=crossings(a.outgoing,b.start,b.end)+crossings(b.incoming,a.start,a.end);
        const auto c2=crossings(b.outgoing,a.start,a.end)+crossings(a.incoming,b.start,b.end);
        if(c1==c2) {if(c1>0){r.dependencies+=2;r.crossings+=c1;}}
        else {++r.dependencies;r.crossings+=std::min(c1,c2);}
    }
    Rating rate_area(std::size_t n,const Area& area) const {
        const auto& s=segments[n]; Segment left,right;
        left.incoming=s.incoming;extent(left);right.outgoing=s.outgoing;extent(right);
        const auto center=(area.start+area.end)/2;
        // The source fixes each temporary extent before adding the split coordinate.
        left.outgoing={center};right.incoming={center};
        Rating result;
        for(const auto d:s.in_deps) {rate_orderings(result,left,segments[deps[d].source]);rate_orderings(result,right,segments[deps[d].source]);}
        for(const auto d:s.out_deps) {rate_orderings(result,left,segments[deps[d].target]);rate_orderings(result,right,segments[deps[d].target]);}
        result.dependencies+=2;
        const auto& cause=segments[*s.split_by];
        result.crossings+=crossings(left.outgoing,cause.start,cause.end)+crossings(cause.incoming,left.start,left.end);
        result.crossings+=crossings(cause.outgoing,right.start,right.end)+crossings(right.incoming,cause.start,cause.end);
        return result;
    }
    double split_position(std::size_t n,std::vector<Area>& areas) const {
        const auto& s=segments[n]; std::optional<std::size_t> best; Rating best_rating;
        for(std::size_t i=0;i<areas.size();++i) {
            const auto& a=areas[i]; if(a.start>s.end)break;if(a.end<s.start)continue;
            const auto rating=rate_area(n,a);
            if(!best||rating.crossings<best_rating.crossings||
                (rating.crossings==best_rating.crossings&&(rating.dependencies<best_rating.dependencies||
                (rating.dependencies==best_rating.dependencies&&a.end-a.start>areas[*best].end-areas[*best].start)))) {
                best=i;best_rating=rating;
            }
        }
        if(!best)return(s.start+s.end)/2;
        const auto area=areas[*best];const auto position=(area.start+area.end)/2;
        areas.erase(areas.begin()+*best);
        if((area.end-area.start)/2>=critical_threshold) {
            auto index=*best;
            if(area.start<=position-critical_threshold)areas.insert(areas.begin()+index++,{area.start,position-critical_threshold});
            if(position+critical_threshold<=area.end)areas.insert(areas.begin()+index,{position+critical_threshold,area.end});
        }
        return position;
    }
    void split_segments(const std::vector<std::size_t>& cycle_deps) {
        if(cycle_deps.empty())return;
        std::vector<double> coordinates;
        for(const auto& s:segments) {coordinates.insert(coordinates.end(),s.incoming.begin(),s.incoming.end());coordinates.insert(coordinates.end(),s.outgoing.begin(),s.outgoing.end());}
        std::ranges::sort(coordinates);std::vector<Area> areas;
        for(std::size_t i=1;i<coordinates.size();++i)
            if(coordinates[i]-coordinates[i-1]>=2*critical_threshold)areas.push_back({coordinates[i-1]+critical_threshold,coordinates[i]-critical_threshold});
        std::vector<std::size_t> selected;
        for(const auto d:cycle_deps) {
            auto source=deps[d].source,target=deps[d].target;
            if(std::ranges::find(selected,source)!=selected.end()||std::ranges::find(selected,target)!=selected.end())continue;
            if(segments[source].incoming.size()+segments[source].outgoing.size()>2&&segments[target].incoming.size()+segments[target].outgoing.size()<=2)std::swap(source,target);
            selected.push_back(source);segments[source].split_by=target;
        }
        std::stable_sort(selected.begin(),selected.end(),[&](auto a,auto b){return segments[a].end-segments[a].start<segments[b].end-segments[b].start;});
        for(const auto n:selected) {
            const auto position=split_position(n,areas);
            const auto partner=segments.size();
            Segment split;split.edge=segments[n].edge;split.partner=n;split.incoming={position};split.outgoing=segments[n].outgoing;extent(split);
            segments[n].partner=partner;segments[n].outgoing={position};extent(segments[n]);segments.push_back(std::move(split));
            while(!segments[n].in_deps.empty())remove(segments[n].in_deps.front());
            while(!segments[n].out_deps.empty())remove(segments[n].out_deps.front());
            const auto cause=*segments[n].split_by;
            dependency(n,cause,1,true);dependency(cause,partner,1,true);
            for(std::size_t other=0;other<segments.size();++other)
                if(other!=cause&&other!=n&&other!=partner){create_dependencies(other,n);create_dependencies(other,partner);}
        }
    }
    void number_slots() {
        std::deque<std::size_t> sources,rightward;
        for(std::size_t n=0;n<segments.size();++n) {
            auto& s=segments[n];s.in_weight=static_cast<int>(s.in_deps.size());s.out_weight=static_cast<int>(s.out_deps.size());
            if(s.in_weight==0)sources.push_back(n);
            if(s.out_weight==0&&s.incoming.empty())rightward.push_back(n);
        }
        std::optional<std::size_t> max_rank;
        while(!sources.empty()) {
            const auto n=sources.front();sources.pop_front();
            for(const auto d:segments[n].out_deps) {
                auto& target=segments[deps[d].target];target.slot=std::max(target.slot,segments[n].slot+1);
                max_rank=std::max(max_rank.value_or(0),target.slot);
                if(--target.in_weight==0)sources.push_back(deps[d].target);
            }
        }
        if(max_rank) {
            for(const auto n:rightward)segments[n].slot=*max_rank;
            while(!rightward.empty()) {
                const auto n=rightward.front();rightward.pop_front();
                for(const auto d:segments[n].in_deps) {
                    auto& source=segments[deps[d].source];if(!source.incoming.empty())continue;
                    source.slot=std::min(source.slot,segments[n].slot-1);
                    if(--source.out_weight==0)rightward.push_back(deps[d].source);
                }
            }
        }
    }
    std::size_t run(std::span<const std::size_t> left,std::span<const std::size_t> right,
        double start,ElkOrthogonalRoutes& result) {
        const auto collect = [&](std::span<const std::size_t> layer, ElkPortSide side) {
            for (const auto n : layer) for (const auto p : g.nodes[n].ports) {
                if (g.ports[p].side != side) continue;
                for (const auto e : g.ports[p].outgoing) {
                    Segment segment{.edge=e};
                    const auto& edge = g.edges[e];
                    for (const auto endpoint : {edge.source_port, edge.target_port}) {
                        const auto& port = g.ports[endpoint];
                        auto& coordinates = port.side == ElkPortSide::east ? segment.incoming : segment.outgoing;
                        coordinates.push_back(y[port.node] + port.y);
                    }
                    std::ranges::sort(segment.incoming);
                    std::ranges::sort(segment.outgoing);
                    extent(segment);
                    segments.push_back(std::move(segment));
                }
            }
        };
        collect(left, ElkPortSide::east);
        collect(right, ElkPortSide::west);
        double minimum=std::numeric_limits<double>::max();
        for(bool incoming:{true,false}) {
            std::vector<double> coordinates;
            for(const auto& s:segments){const auto& values=incoming?s.incoming:s.outgoing;coordinates.insert(coordinates.end(),values.begin(),values.end());}
            std::ranges::sort(coordinates);coordinates.erase(std::unique(coordinates.begin(),coordinates.end()),coordinates.end());
            for(std::size_t i=1;i<coordinates.size();++i)minimum=std::min(minimum,coordinates[i]-coordinates[i-1]);
        }
        critical_threshold=minimum*.2;int critical=0;
        for(std::size_t a=0;a<segments.size();++a)for(std::size_t b=a+1;b<segments.size();++b)critical+=create_dependencies(a,b);
        if(critical>=2)split_segments(cycles(true));
        for(const auto d:cycles(false)){if(deps[d].weight==0)remove(d);else reverse(d);}
        number_slots();std::size_t count=0;
        for(const auto& s:segments) {
            if(std::abs(s.start-s.end)<epsilon)continue;
            count=std::max(count,s.slot+1);
            if(s.partner&&!s.split_by)continue;
            const auto& e=g.edges[s.edge];const auto& source=g.ports[e.source_port];const auto& target=g.ports[e.target_port];
            const auto sy=y[source.node]+source.y,ty=y[target.node]+target.y;
            if(std::abs(sy-ty)<=epsilon)continue;
            auto x=start+s.slot*spacing.edge_edge_between_layers;
            auto& bends=result.bends[s.edge];bends.push_back({x,sy});
            if(s.partner) {
                const auto& partner=segments[*s.partner];const auto split=partner.incoming.front();
                bends.push_back({x,split});x=start+partner.slot*spacing.edge_edge_between_layers;bends.push_back({x,split});
            }
            bends.push_back({x,ty});
        }
        return count;
    }
};
}
std::expected<ElkOrthogonalRoutes,std::string> elk_route_flat_edges(const ElkLayeredGraph& g,
    std::span<const double> y,std::span<const ElkHorizontalMargin> margins,ElkRoutingSpacing spacing,std::int64_t seed) {
    ElkRandom random(static_cast<std::uint64_t>(seed));
    return elk_route_flat_edges(g,y,margins,random,spacing);
}
std::expected<ElkOrthogonalRoutes,std::string> elk_route_flat_edges(const ElkLayeredGraph& g,
    std::span<const double> y,std::span<const ElkHorizontalMargin> margins,ElkRandom& random,ElkRoutingSpacing spacing) {
    if(y.size()!=g.nodes.size()||margins.size()!=g.nodes.size())return std::unexpected("ELK routing requires measured positions and margins for each node");
    for(const auto& p:g.ports)if(p.incoming.size()+p.outgoing.size()>1)return std::unexpected("ELK flat routing requires one edge per port");
    for(const auto& e:g.edges) {
        const auto& s=g.ports[e.source_port];const auto& t=g.ports[e.target_port];
        const auto left = g.nodes[s.node].layer, right = g.nodes[t.node].layer;
        const bool forward = right == left + 1 && s.side == ElkPortSide::east && t.side == ElkPortSide::west;
        const bool in_layer = left == right && s.node != t.node && s.side == t.side
            && (s.side == ElkPortSide::east || s.side == ElkPortSide::west);
        if (!forward && !in_layer)
            return std::unexpected("ELK routing requires preprocessed adjacent or same-side in-layer edges");
    }
    ElkOrthogonalRoutes result;result.node_x.resize(g.nodes.size());result.bends.resize(g.edges.size());
    for(const auto& layer:g.layers) {
        double width=0;
        for(const auto n:layer)width=std::max(width,g.nodes[n].width+margins[n].left+margins[n].right);
        result.layer_width.push_back(width);
    }
    const auto initial_slots = g.layers.empty() ? 0
        : RoutingGap(g,y,spacing,random).run({},g.layers.front(),0,result);
    result.gap_slots.push_back(initial_slots);
    if (initial_slots > 0)
        result.width = (initial_slots - 1) * spacing.edge_edge_between_layers + spacing.edge_node_between_layers;
    for(std::size_t l=0;l<g.layers.size();++l) {
        const auto& layer=g.layers[l];double left=0,right=0;
        for(const auto n:layer){left=std::max(left,margins[n].left);right=std::max(right,margins[n].right);}
        for(const auto n:layer) {
            std::size_t in=0,out=0;
            for(const auto p:g.nodes[n].ports){in+=!g.ports[p].incoming.empty();out+=!g.ports[p].outgoing.empty();}
            const double ratio=in+out==0?.5:static_cast<double>(out)/(in+out);
            double x=(result.layer_width[l]-g.nodes[n].width)*ratio;
            if(ratio>.5)x-=right*2*(ratio-.5);else if(ratio<.5)x+=left*2*(.5-ratio);
            x=std::max(x,margins[n].left);x=std::min(x,result.layer_width[l]-margins[n].right-g.nodes[n].width);
            result.node_x[n]=result.width+x;
        }
        result.width+=result.layer_width[l];
        const auto external=[&](std::size_t layer_index) {
            return std::ranges::all_of(g.layers[layer_index],[&](auto n){return g.nodes[n].kind==ElkLayerNodeKind::external_port;});
        };
        const bool ordinary_gap=l+1<g.layers.size()&&!external(l)&&!external(l+1);
        const auto next = l+1<g.layers.size() ? std::span<const std::size_t>{g.layers[l+1]}
            : std::span<const std::size_t>{};
        const auto slots=RoutingGap(g,y,spacing,random).run(layer,next,result.width+spacing.edge_node_between_layers,result);
        result.gap_slots.push_back(slots);
        if(slots>0) {
            auto width=(slots-1)*spacing.edge_edge_between_layers+spacing.edge_node_between_layers;
            if(l+1<g.layers.size())width+=spacing.edge_node_between_layers;
            if(ordinary_gap)width=std::max(width,spacing.node_node_between_layers);
            result.width+=width;
        } else if(ordinary_gap)result.width+=spacing.node_node_between_layers;
    }
    return result;
}
}
