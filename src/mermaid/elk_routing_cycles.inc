// SPDX-License-Identifier: EPL-2.0
// Shared ELK HyperEdgeCycleDetector; preserve adjacency, queue and RNG order.
template<class Segments,class Dependencies>
void routing_update_neighbors(Segments& segments,const Dependencies& deps,std::size_t node,std::deque<std::size_t>& sources,std::deque<std::size_t>& sinks,bool critical_only) {
    for(const auto d:segments[node].out_deps) {
        const auto& e=deps[d]; auto& target=segments[e.target];
        if ((!critical_only||e.critical)&&target.mark<0&&e.weight>0) {
            target.in_weight-=e.weight; if(e.critical) target.critical_in-=e.weight;
            if(target.in_weight<=0&&target.out_weight>0) sources.push_back(e.target);
        }
    }
    for(const auto d:segments[node].in_deps) {
        const auto& e=deps[d]; auto& source=segments[e.source];
        if ((!critical_only||e.critical)&&source.mark<0&&e.weight>0) {
            source.out_weight-=e.weight; if(e.critical) source.critical_out-=e.weight;
            if(source.out_weight<=0&&source.in_weight>0) sinks.push_back(e.source);
        }
    }
}
template<class Segments,class Dependencies>
std::vector<std::size_t> routing_dependency_cycles(Segments& segments,const Dependencies& deps,ElkRandom& random,bool critical_only) {
    std::deque<std::size_t> sources,sinks;
    for(std::size_t n=0;n<segments.size();++n) {
        auto& s=segments[n]; s.mark=-static_cast<int>(n)-1;
        s.in_weight=s.out_weight=s.critical_in=s.critical_out=0;
        for(const auto d:s.in_deps) {if(deps[d].critical)s.critical_in+=deps[d].weight; if(!critical_only||deps[d].critical)s.in_weight+=deps[d].weight;}
        for(const auto d:s.out_deps) {if(deps[d].critical)s.critical_out+=deps[d].weight; if(!critical_only||deps[d].critical)s.out_weight+=deps[d].weight;}
        if(s.out_weight==0)sinks.push_back(n);else if(s.in_weight==0)sources.push_back(n);
    }
    const auto base=static_cast<int>(segments.size()); int sink_mark=base-1,source_mark=base+1;
    std::size_t remaining=segments.size();
    while(remaining) {
        while(!sinks.empty()) {
            const auto n=sinks.front();sinks.pop_front();--remaining;
            segments[n].mark=sink_mark--; routing_update_neighbors(segments,deps,n,sources,sinks,critical_only);
        }
        while(!sources.empty()) {
            const auto n=sources.front();sources.pop_front();--remaining;
            segments[n].mark=source_mark++; routing_update_neighbors(segments,deps,n,sources,sinks,critical_only);
        }
        int max_flow=std::numeric_limits<int>::min(); std::vector<std::size_t> candidates;
        // The original TreeSet is ordered by the initial negative marks.
        for(std::size_t i=segments.size();i>0;--i) {
            const auto n=i-1; const auto& s=segments[n]; if(s.mark>=0)continue;
            if(!critical_only&&s.critical_out>0&&s.critical_in<=0) {candidates={n};break;}
            const auto flow=s.out_weight-s.in_weight;
            if(flow>=max_flow) {if(flow>max_flow){candidates.clear();max_flow=flow;}candidates.push_back(n);}
        }
        if(!candidates.empty()) {
            const auto n=candidates[random.next_int(candidates.size())];--remaining;
            segments[n].mark=source_mark++;routing_update_neighbors(segments,deps,n,sources,sinks,critical_only);
        }
    }
    for(auto& s:segments)if(s.mark<base)s.mark+=base+1;
    std::vector<std::size_t> result;
    for(const auto& s:segments)for(const auto d:s.out_deps)
        if((!critical_only||deps[d].critical)&&s.mark>segments[deps[d].target].mark)result.push_back(d);
    return result;
}
