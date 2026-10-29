// Private continuation of the owned rank graph. Port of dagre-d3-es 7.0.14
// normalize, parent-dummy-chains, add-border-segments and order/. MIT.
struct DummyChain { Edge edge; std::vector<Id> nodes; };
using Layers = std::vector<std::vector<Id>>;

std::vector<DummyChain> normalize_edges(Graph& g,const FlowchartRanks& ranks,
    std::span<const FlowchartEdgeLabelSize> labels) {
    std::vector<DummyChain> chains;
    const auto edge_count=g.edges.size();
    for(Id i=0;i<edge_count;++i) {
        const auto e=g.edges[i];
        if(!e.active || e.original==absent) continue;
        const int first=*g.nodes[e.v].rank,last=*g.nodes[e.w].rank;
        if(last==first+1) continue;
        g.edges[i].active=false;
        DummyChain chain{e};auto previous=e.v;
        for(int rank=first+1;rank<last;++rank) {
            const auto v=g.dummy(u"_d");
            auto& node=g.nodes[v];node.rank=rank;node.kind=Dummy::edge;
            if(ranks.edges[e.original].label_rank==rank) {
                node.kind=Dummy::label;node.width=labels[e.original].width;node.height=labels[e.original].height;
            }
            chain.nodes.push_back(v);g.edges.push_back({previous,v,1,e.weight});previous=v;
        }
        g.edges.push_back({previous,e.w,1,e.weight});chains.push_back(std::move(chain));
    }
    // Walk the compound path from the tail parent to the common ancestor and
    // down to the head parent, assigning each short-edge dummy to its rank.
    for(const auto& chain:chains) {
        std::vector<Id> from,to;
        for(auto v=g.nodes[chain.edge.v].parent;;v=g.nodes[v].parent) {from.push_back(v);if(v==absent) break;}
        auto lca=g.nodes[chain.edge.w].parent;
        while(std::ranges::find(from,lca)==from.end()) {to.push_back(lca);lca=g.nodes[lca].parent;}
        from.erase(std::ranges::find(from,lca)+1,from.end());
        from.insert(from.end(),to.rbegin(),to.rend());
        Id index=0;bool ascending=true;
        for(auto v:chain.nodes) {
            const int rank=*g.nodes[v].rank;
            if(ascending) {
                while(from[index]!=lca && g.nodes[from[index]].max_rank<rank) ++index;
                if(from[index]==lca) ascending=false;
            }
            if(!ascending) while(index+1<from.size() && g.nodes[from[index+1]].min_rank<=rank) ++index;
            g.nodes[v].parent=from[index];
        }
    }
    return chains;
}

void add_side_borders(Graph& g) {
    const auto visit=[&](auto&& self,Id v)->void {
        for(auto child:g.children(v)) self(self,child);
        if(g.nodes[v].top==absent) return;
        const auto minimum=g.nodes[v].min_rank,maximum=g.nodes[v].max_rank;
        g.nodes[v].left.resize(maximum+1,absent);g.nodes[v].right.resize(maximum+1,absent);
        for(int rank=minimum;rank<=maximum;++rank) for(int side:{-1,1}) {
            const auto n=g.dummy(side<0?u"_bl":u"_br",v);
            g.nodes[n].kind=Dummy::border;g.nodes[n].rank=rank;g.nodes[n].border_side=side;
            auto& border=side<0?g.nodes[v].left:g.nodes[v].right;
            border[rank]=n;
            if(rank>minimum) g.edges.push_back({border[rank-1],n,1,1});
        }
    };
    for(auto v:g.children(absent)) visit(visit,v);
}

Layers layer_matrix(const Graph& g) {
    int maximum=0;for(const auto& n:g.nodes) if(n.active && n.rank) maximum=std::max(maximum,*n.rank);
    Layers layers(maximum+1);
    for(auto v:g.node_order()) if(g.nodes[v].rank) layers[*g.nodes[v].rank].push_back(v);
    for(auto& layer:layers) std::stable_sort(layer.begin(),layer.end(),[&](Id a,Id b){return g.nodes[a].order<g.nodes[b].order;});
    return layers;
}
void assign_order(Graph& g,const Layers& layers) {
    for(const auto& layer:layers) for(Id i=0;i<layer.size();++i) g.nodes[layer[i]].order=static_cast<int>(i);
}

// A sweep layer shares node orders with the retained graph. Neighbor nodes
// are fixed; only the hierarchy of the requested rank is movable.
struct SweepLayer {
    const Graph& g;int rank;bool down;
    std::vector<Id> children(Id parent) const {
        auto result=g.children(parent);
        std::erase_if(result,[&](Id v){const auto& n=g.nodes[v];return n.rank!=rank &&
            !(n.top!=absent && n.min_rank<=rank && rank<=n.max_rank);});
        return result;
    }
    std::vector<std::pair<Id,double>> incoming(Id v) const {
        std::vector<std::pair<Id,double>> result;
        for(const auto& e:g.edges) if(e.active && (down?e.w:e.v)==v) {
            const auto u=down?e.v:e.w;
            auto found=std::ranges::find_if(result,[&](auto item){return item.first==u;});
            if(found==result.end()) result.emplace_back(u,e.weight);else found->second+=e.weight;
        }
        return result;
    }
};
struct BaryEntry {
    std::vector<Id> vs;
    Id index=0;
    std::optional<double> barycenter;
    double weight=0;
};
using Constraints=std::vector<std::pair<Id,Id>>;

void merge_barycenter(BaryEntry& target,const BaryEntry& other) {
    if(target.barycenter) {
        target.barycenter=(*target.barycenter*target.weight+*other.barycenter*other.weight)/(target.weight+other.weight);
        target.weight+=other.weight;
    } else {target.barycenter=other.barycenter;target.weight=other.weight;}
}

std::vector<BaryEntry> resolve_order_conflicts(const Graph& g,std::vector<BaryEntry> entries,const Constraints& constraints) {
    const auto count=entries.size();
    std::vector<int> indegree(count);std::vector<bool> merged(count);
    std::vector<std::vector<Id>> incoming(count),outgoing(count);
    const auto find=[&](Id v){for(Id i=0;i<count;++i) if(entries[i].vs.front()==v) return i;return absent;};
    for(auto [v,w]:constraints) {
        auto a=find(v),b=find(w);if(a==absent||b==absent) continue;
        ++indegree[b];outgoing[a].push_back(b);
    }
    std::vector<Id> sources,visited;
    for(Id i=0;i<count;++i) if(!indegree[i]) sources.push_back(i);
    std::stable_sort(sources.begin(),sources.end(),[&](Id a,Id b){return index_key(g.nodes[entries[a].vs.front()].id)<index_key(g.nodes[entries[b].vs.front()].id);});
    while(!sources.empty()) {
        const auto v=sources.back();sources.pop_back();visited.push_back(v);
        for(auto it=incoming[v].rbegin();it!=incoming[v].rend();++it) {
            const auto u=*it;if(merged[u]) continue;
            auto& target=entries[v];auto& source=entries[u];
            if(!source.barycenter||!target.barycenter||*source.barycenter>=*target.barycenter) {
                double sum=0,weight=0;
                if(target.weight) {sum+=*target.barycenter*target.weight;weight+=target.weight;}
                if(source.weight) {sum+=*source.barycenter*source.weight;weight+=source.weight;}
                target.vs.insert(target.vs.begin(),source.vs.begin(),source.vs.end());
                target.barycenter=weight?sum/weight:std::numeric_limits<double>::quiet_NaN();target.weight=weight;
                target.index=std::min(target.index,source.index);merged[u]=true;
            }
        }
        for(auto w:outgoing[v]) {incoming[w].push_back(v);if(--indegree[w]==0) sources.push_back(w);}
    }
    std::vector<BaryEntry> result;
    for(auto v:visited) if(!merged[v]) result.push_back(std::move(entries[v]));
    return result;
}

BaryEntry sort_subgraph(const SweepLayer& layer,Id parent,const Constraints& constraints,bool bias_right) {
    const auto& g=layer.g;auto movable=layer.children(parent);
    const auto left=parent==absent?absent:g.nodes[parent].left[layer.rank];
    const auto right=parent==absent?absent:g.nodes[parent].right[layer.rank];
    std::erase(movable,left);std::erase(movable,right);
    std::vector<BaryEntry> entries;
    std::unordered_map<Id,BaryEntry> subgraphs;
    for(auto v:movable) {
        BaryEntry entry{{v},entries.size()};auto in=layer.incoming(v);
        if(!in.empty()) {
            double sum=0;for(auto [u,weight]:in) {sum+=weight*g.nodes[u].order;entry.weight+=weight;}
            entry.barycenter=sum/entry.weight;
        }
        if(!layer.children(v).empty()) {
            auto sub=sort_subgraph(layer,v,constraints,bias_right);
            if(sub.barycenter) merge_barycenter(entry,sub);
            subgraphs.emplace(v,std::move(sub));
        }
        entries.push_back(std::move(entry));
    }
    entries=resolve_order_conflicts(g,std::move(entries),constraints);
    std::vector<BaryEntry> sortable,unsortable;
    for(auto& entry:entries) {
        std::vector<Id> expanded;
        for(auto v:entry.vs) {
            auto found=subgraphs.find(v);
            if(found==subgraphs.end()) expanded.push_back(v);
            else expanded.insert(expanded.end(),found->second.vs.begin(),found->second.vs.end());
        }
        entry.vs=std::move(expanded);
        (entry.barycenter?sortable:unsortable).push_back(std::move(entry));
    }
    std::stable_sort(sortable.begin(),sortable.end(),[&](const BaryEntry& a,const BaryEntry& b){
        if(*a.barycenter<*b.barycenter) return true;
        if(*a.barycenter>*b.barycenter) return false;
        return bias_right?a.index>b.index:a.index<b.index;
    });
    std::stable_sort(unsortable.begin(),unsortable.end(),[](const BaryEntry& a,const BaryEntry& b){return a.index>b.index;});
    BaryEntry result;double sum=0;Id index=0;
    const auto consume=[&]() {
        while(!unsortable.empty() && unsortable.back().index<=index) {
            const auto& last=unsortable.back();result.vs.insert(result.vs.end(),last.vs.begin(),last.vs.end());
            unsortable.pop_back();++index;
        }
    };
    consume();
    for(const auto& entry:sortable) {
        index+=entry.vs.size();result.vs.insert(result.vs.end(),entry.vs.begin(),entry.vs.end());
        sum+=*entry.barycenter*entry.weight;result.weight+=entry.weight;consume();
    }
    if(result.weight) result.barycenter=sum/result.weight;
    if(left!=absent) {
        result.vs.insert(result.vs.begin(),left);result.vs.push_back(right);
        auto lp=layer.incoming(left),rp=layer.incoming(right);
        if(!lp.empty()) {
            result.barycenter=(result.barycenter.value_or(0)*result.weight+g.nodes[lp.front().first].order+
                g.nodes[rp.front().first].order)/(result.weight+2);result.weight+=2;
        }
    }
    return result;
}

void add_order_constraints(const Graph& g,Constraints& constraints,std::span<const Id> nodes) {
    std::unordered_map<Id,Id> previous;
    for(auto v:nodes) {
        auto child=g.nodes[v].parent;
        while(child!=absent) {
            const auto parent=g.nodes[child].parent;
            auto found=previous.find(parent);const auto before=found==previous.end()?absent:found->second;
            previous[parent]=child;
            if(before!=absent && before!=child) {
                const auto edge=std::pair{before,child};
                if(std::ranges::find(constraints,edge)==constraints.end()) constraints.push_back(edge);
                break;
            }
            child=parent;
        }
    }
}

double crossing_count(const Graph& g,const Layers& layers) {
    double count=0;
    for(Id rank=1;rank<layers.size();++rank) {
        std::vector<std::pair<int,double>> sequence;
        for(auto v:layers[rank-1]) {
            std::vector<std::pair<int,double>> outgoing;
            for(const auto& e:g.edges) if(e.active && e.v==v) outgoing.emplace_back(g.nodes[e.w].order,e.weight);
            std::stable_sort(outgoing.begin(),outgoing.end(),[](auto a,auto b){return a.first<b.first;});
            sequence.insert(sequence.end(),outgoing.begin(),outgoing.end());
        }
        // Fenwick accumulation of the same weighted inversion count as the
        // upstream complete accumulator tree; no quadratic edge-pair scan.
        std::vector<double> tree(layers[rank].size()+1);double total=0;
        for(auto [pos,weight]:sequence) {
            double before=0;for(Id i=pos+1;i;i-=i&(~i+1)) before+=tree[i];
            count+=weight*(total-before);total+=weight;
            for(Id i=pos+1;i<tree.size();i+=i&(~i+1)) tree[i]+=weight;
        }
    }
    return count;
}

void order_nodes(Graph& g) {
    auto layers=layer_matrix(g);for(auto& layer:layers) layer.clear();
    std::vector<Id> simple;for(auto v:g.node_order()) if(g.nodes[v].rank) simple.push_back(v);
    std::stable_sort(simple.begin(),simple.end(),[&](Id a,Id b){return g.nodes[a].rank<g.nodes[b].rank;});
    std::vector<bool> visited(g.nodes.size());
    const auto dfs=[&](auto&& self,Id v)->void {
        if(visited[v]) return;visited[v]=true;layers[*g.nodes[v].rank].push_back(v);
        for(auto w:g.neighbors(v,true)) self(self,w);
    };
    for(auto v:simple) dfs(dfs,v);assign_order(g,layers);
    Layers best;double best_count=std::numeric_limits<double>::infinity();
    for(int iteration=0,last_best=0;last_best<4;++iteration,++last_best) {
        const bool down=iteration%2!=0,bias=iteration%4>=2;Constraints constraints;
        const int maximum=static_cast<int>(layers.size())-1;
        for(int rank=down?1:maximum-1;down?rank<=maximum:rank>=0;rank+=down?1:-1) {
            auto sorted=sort_subgraph({g,rank,down},absent,constraints,bias);
            for(Id i=0;i<sorted.vs.size();++i) g.nodes[sorted.vs[i]].order=static_cast<int>(i);
            add_order_constraints(g,constraints,sorted.vs);
        }
        layers=layer_matrix(g);const double count=crossing_count(g,layers);
        if(count<best_count) {last_best=0;best=layers;best_count=count;}
    }
    assign_order(g,best);
}
