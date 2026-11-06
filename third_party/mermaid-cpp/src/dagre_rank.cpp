// Port of dagre-d3-es 7.0.14 acyclic, nesting-graph, rank and layout rank helpers.
// MIT attribution: ../DAGRE-LICENSE. Graph storage is owned native data.
module folia.mermaid.dagre_rank;
import std;

namespace folia::mermaid {
namespace {
using Id = std::size_t;
constexpr Id absent = std::numeric_limits<Id>::max();
enum class Dummy { none, border, edge, label, self };
struct Node {
    std::u16string id;
    Id parent = absent, top = absent, bottom = absent;
    std::optional<int> rank;
    bool active = true;
    Dummy kind = Dummy::none;
    int min_rank = 0, max_rank = 0, order = 0, border_side = 0;
    double width = 0, height = 0, x = 0, y = 0;
    Id edge = absent;
    std::vector<Id> left, right;
};
struct Edge {
    Id v, w;
    int minlen = 1;
    double weight = 1;
    Id original = absent;
    bool reversed = false, nesting = false, active = true;
};

// Graphlib enumerates ordinary string ids by insertion order, but decimal
// array-index ids first. Edge keys contain separators and are never indexes.
std::uint64_t index_key(std::u16string_view s) {
    constexpr auto none = std::numeric_limits<std::uint64_t>::max();
    if (s.empty() || s.size() > 10 || (s.size() > 1 && s[0] == u'0')) return none;
    std::uint64_t value = 0;
    for (auto c : s) { if (c < u'0' || c > u'9') return none; value = value * 10 + c-u'0'; }
    return value < 4294967295ULL ? value : none;
}
struct Graph {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::size_t serial = 0;
    void key_order(std::vector<Id>& ids) const {
        std::stable_sort(ids.begin(),ids.end(),[&](Id a,Id b) {
            return index_key(nodes[a].id) < index_key(nodes[b].id);
        });
    }
    std::vector<Id> node_order() const {
        std::vector<Id> result;
        for(Id i=0;i<nodes.size();++i) if(nodes[i].active) result.push_back(i);
        key_order(result); return result;
    }
    std::vector<Id> children(Id parent) const {
        std::vector<Id> result;
        for (Id i=0;i<nodes.size();++i) if (nodes[i].active && nodes[i].parent == parent) result.push_back(i);
        key_order(result); return result;
    }
    std::vector<Id> incident(Id v) const {
        std::vector<Id> result;
        for (Id i=0;i<edges.size();++i) if (edges[i].active && edges[i].w == v) result.push_back(i);
        for (Id i=0;i<edges.size();++i) if (edges[i].active && edges[i].v == v) result.push_back(i);
        return result;
    }
    Id dummy(std::u16string_view prefix,Id parent=absent) {
        std::u16string id;
        do {
            auto number = std::to_string(++serial);
            id = prefix; for (auto c:number) id.push_back(static_cast<char16_t>(c));
        } while (std::ranges::any_of(nodes,[&](const Node& n) { return n.id == id; }));
        nodes.push_back({std::move(id),parent}); return nodes.size()-1;
    }
    std::vector<Id> neighbors(Id v, bool outgoing) const {
        std::vector<Id> result;
        for(const auto& e:edges) if(e.active && (outgoing?e.v:e.w)==v) {
            auto w=outgoing?e.w:e.v;
            if(std::ranges::find(result,w)==result.end()) result.push_back(w);
        }
        key_order(result); return result;
    }
    int slack(const Edge& e) const { return *nodes[e.w].rank - *nodes[e.v].rank - e.minlen; }
};

void remove_cycles(Graph& g) {
    std::vector<unsigned char> state(g.nodes.size()); std::vector<Id> reverse;
    const auto visit = [&](auto&& self,Id v)->void {
        if (state[v]) return;
        state[v]=1;
        for (Id i=0;i<g.edges.size();++i) {
            const auto& e=g.edges[i]; if (!e.active || e.v!=v) continue;
            if (state[e.w]==1) reverse.push_back(i); else self(self,e.w);
        }
        state[v]=2;
    };
    for (auto v:g.node_order()) visit(visit,v);
    // Graphlib removes/reinserts each reverse edge, so it moves to the end.
    for (auto i:reverse) {
        auto e=g.edges[i]; g.edges[i].active=false;
        std::swap(e.v,e.w); e.reversed=true; g.edges.push_back(e);
    }
}

struct Nesting { Id root; int factor; };
Nesting add_nesting(Graph& g) {
    const auto root=g.dummy(u"_root");
    std::vector<int> depths(g.nodes.size()); int height=0;
    const auto depth = [&](auto&& self,Id v,int d)->void {
        depths[v]=d; height=std::max(height,d-1);
        for (auto c:g.children(v)) self(self,c,d+1);
    };
    for (auto v:g.children(absent)) depth(depth,v,1);
    const int factor=2*height+1;
    double weight=1;
    for (auto& e:g.edges) if(e.active) { e.minlen*=factor; weight+=e.weight; }
    const auto visit = [&](auto&& self,Id v)->void {
        const auto children=g.children(v);
        if(children.empty()) {
            if(v!=root) g.edges.push_back({root,v,factor,0});
            return;
        }
        const auto top=g.dummy(u"_bt",v),bottom=g.dummy(u"_bb",v);
        g.nodes[v].top=top; g.nodes[v].bottom=bottom;
        g.nodes[top].kind=g.nodes[bottom].kind=Dummy::border;
        for(auto c:children) {
            self(self,c);
            const bool group=g.nodes[c].top!=absent;
            const auto ct=group?g.nodes[c].top:c, cb=group?g.nodes[c].bottom:c;
            const int len=group?1:height-depths[v]+1;
            const double w=group?weight:2*weight;
            g.edges.push_back({top,ct,len,w,absent,false,true});
            g.edges.push_back({cb,bottom,len,w,absent,false,true});
        }
        if(g.nodes[v].parent==absent) g.edges.push_back({root,top,height+depths[v],0});
    };
    for(auto v:g.children(absent)) visit(visit,v);
    return {root,factor};
}

// Network simplex uses a simple graph: parallel weights add and minlen is max.
std::vector<Edge> simplify(std::span<const Edge> edges) {
    std::vector<Edge> result;
    for(const auto& e:edges) if(e.active) {
        auto it=std::ranges::find_if(result,[&](const Edge& a){return a.v==e.v && a.w==e.w;});
        if(it==result.end()) { auto copy=e; copy.minlen=std::max(1,e.minlen); result.push_back(copy); }
        else { it->weight+=e.weight; it->minlen=std::max(it->minlen,e.minlen); }
    }
    return result;
}
struct TreeEdge { Id v,w; double cut=0; bool active=true; };
struct Tree {
    const Graph& g;
    std::vector<Id> inserted;
    std::vector<bool> contains;
    std::vector<TreeEdge> edges;
    std::vector<Id> parent;
    std::vector<int> low,lim;
    explicit Tree(const Graph& graph):g(graph),contains(g.nodes.size()),parent(g.nodes.size(),absent),
        low(g.nodes.size()),lim(g.nodes.size()) {}
    void node(Id v) { if(!contains[v]) {contains[v]=true; inserted.push_back(v);} }
    std::vector<Id> nodes() const {auto result=inserted;g.key_order(result);return result;}
    void edge(Id v,Id w) {
        node(v); node(w);
        if(g.nodes[v].id>g.nodes[w].id) std::swap(v,w);
        edges.push_back({v,w});
    }
    Id find(Id v,Id w) const {
        for(Id i=0;i<edges.size();++i) if(edges[i].active &&
            ((edges[i].v==v && edges[i].w==w)||(edges[i].w==v && edges[i].v==w))) return i;
        return absent;
    }
    std::vector<Id> neighbors(Id v) const {
        std::vector<Id> before,after;
        for(const auto& e:edges) if(e.active) {
            if(e.w==v) before.push_back(e.v);
            if(e.v==v) after.push_back(e.w);
        }
        g.key_order(before);g.key_order(after);
        before.insert(before.end(),after.begin(),after.end());return before;
    }
    std::vector<Id> low_lim() {
        int next=1;std::vector<Id> post;
        const auto dfs=[&](auto&& self,Id v,Id p)->void {
            parent[v]=p;low[v]=next;
            for(auto w:neighbors(v)) if(w!=p) self(self,w,v);
            lim[v]=next++;post.push_back(v);
        };
        dfs(dfs,nodes().front(),absent);return post;
    }
};

void network_simplex(Graph& g) {
    // Work on simple edges while sharing the node ranks, as in util.simplify.
    auto original=std::move(g.edges);g.edges=simplify(original);
    const auto order=g.node_order();
    const auto longest=[&](auto&& self,Id v)->int {
        if(g.nodes[v].rank) return *g.nodes[v].rank;
        std::optional<int> value;
        for(const auto& e:g.edges) if(e.v==v) {
            const int next=self(self,e.w)-e.minlen;
            value=value?std::min(*value,next):next;
        }
        g.nodes[v].rank=value.value_or(0);return *g.nodes[v].rank;
    };
    std::vector<Id> leaves;
    for(auto v:order) if(g.nodes[v].top==absent) leaves.push_back(v);
    for(auto v:leaves) if(std::ranges::none_of(g.edges,[&](const Edge& e){return e.w==v;})) longest(longest,v);
    Tree tree(g);tree.node(leaves.front());
    const auto tight=[&](auto&& self,Id v)->void {
        for(auto i:g.incident(v)) {
            const auto& e=g.edges[i];const auto w=e.v==v?e.w:e.v;
            if(!tree.contains[w] && !g.slack(e)) {tree.edge(v,w);self(self,w);}
        }
    };
    for(;;) {
        for(auto v:tree.nodes()) tight(tight,v);
        if(tree.inserted.size()==leaves.size()) break;
        Id best=absent;int slack=std::numeric_limits<int>::max();
        for(Id i=0;i<g.edges.size();++i) {
            const auto& e=g.edges[i];
            if(tree.contains[e.v]!=tree.contains[e.w] && g.slack(e)<slack) {best=i;slack=g.slack(e);}
        }
        if(best==absent) throw std::runtime_error("Dagre rank graph is disconnected");
        const int delta=tree.contains[g.edges[best].v]?slack:-slack;
        for(auto v:tree.inserted) *g.nodes[v].rank+=delta;
    }
    const auto directed=[&](Id v,Id w)->const Edge* {
        for(const auto& e:g.edges) if(e.v==v && e.w==w) return &e;
        return nullptr;
    };
    const auto cuts=[&]() {
        const auto post=tree.low_lim();
        for(auto child:post) {
            const auto parent=tree.parent[child];if(parent==absent) continue;
            const auto forward=directed(child,parent);
            const bool child_tail=forward!=nullptr;
            double value=(forward?forward:directed(parent,child))->weight;
            for(auto i:g.incident(child)) {
                const auto& e=g.edges[i];const bool out=e.v==child;
                const auto other=out?e.w:e.v;if(other==parent) continue;
                const bool head=out==child_tail;
                value+=head?e.weight:-e.weight;
                const auto te=tree.find(child,other);
                if(te!=absent) value+=head?-tree.edges[te].cut:tree.edges[te].cut;
            }
            tree.edges[tree.find(child,parent)].cut=value;
        }
    };
    cuts();
    for(std::size_t iteration=0;;++iteration) {
        const auto leaving=std::ranges::find_if(tree.edges,[](const TreeEdge& e){return e.active && e.cut<0;});
        if(leaving==tree.edges.end()) break;
        if(iteration==100000) throw std::runtime_error("Dagre rank iteration limit exceeded");
        auto v=leaving->v,w=leaving->w;
        if(!directed(v,w)) std::swap(v,w);
        const bool flip=tree.lim[v]>tree.lim[w];const auto tail=flip?w:v;
        const auto descendant=[&](Id n){return tree.low[tail]<=tree.lim[n] && tree.lim[n]<=tree.lim[tail];};
        const Edge* best=nullptr;int slack=std::numeric_limits<int>::max();
        for(const auto& e:g.edges) if(flip==descendant(e.v) && flip!=descendant(e.w) && g.slack(e)<slack) {
            best=&e;slack=g.slack(e);
        }
        if(!best) throw std::runtime_error("Dagre rank has no entering edge");
        leaving->active=false;tree.edge(best->v,best->w);cuts();
        // Original updateRanks traverses from the first tree node. Graph nodes
        // contain no tree-parent field; tree parents live only in Tree.
        const auto update=[&](auto&& self,Id n,Id previous)->void {
            if(previous!=absent) {
                const auto p=tree.parent[n];const auto e=directed(n,p);
                g.nodes[n].rank=*g.nodes[p].rank+(e?-e->minlen:directed(p,n)->minlen);
            }
            for(auto next:tree.neighbors(n)) if(next!=previous) self(self,next,n);
        };
        update(update,tree.nodes().front(),absent);
    }
    g.edges=std::move(original);
}

void normalize_ranks(Graph& g,Nesting nesting) {
    int minimum=std::numeric_limits<int>::max(),maximum=std::numeric_limits<int>::min();
    for(const auto& n:g.nodes) if(n.rank) {minimum=std::min(minimum,*n.rank);maximum=std::max(maximum,*n.rank);}
    if(static_cast<long long>(maximum)-minimum>1000000) throw std::runtime_error("Dagre rank span limit exceeded");
    std::vector<std::vector<Id>> layers(static_cast<std::size_t>(maximum-minimum)+1);
    for(Id v=0;v<g.nodes.size();++v) if(g.nodes[v].rank) layers[*g.nodes[v].rank-minimum].push_back(v);
    int delta=0;
    for(std::size_t i=0;i<layers.size();++i) {
        if(layers[i].empty() && i%nesting.factor!=0) --delta;
        else for(auto v:layers[i]) *g.nodes[v].rank+=delta;
    }
    g.nodes[nesting.root].rank.reset();
    minimum=std::numeric_limits<int>::max();
    for(const auto& n:g.nodes) if(n.rank) minimum=std::min(minimum,*n.rank);
    for(auto& n:g.nodes) if(n.rank) *n.rank-=minimum;
}
struct RankedGraph { Graph graph; FlowchartRanks summary; };

std::expected<RankedGraph,std::string> build_ranked_graph(
    const FlowchartInput& input,std::span<const FlowchartEdgeLabelSize> labels) {
    try {
        if(labels.size()!=input.edges.size()) return std::unexpected("Dagre needs one label box per edge");
        if(input.nodes.size()>10000 || input.edges.size()>500) return std::unexpected("Dagre graph exceeds Flowchart limits");
        if(!std::isfinite(input.rank_spacing) || input.rank_spacing<0) return std::unexpected("Invalid Dagre rank spacing");
        Graph g;std::unordered_map<std::u16string,Id> ids;
        for(const auto& n:input.nodes) {
            if(!ids.emplace(n.id,g.nodes.size()).second) return std::unexpected("Duplicate Dagre node id");
            g.nodes.push_back({n.id});
        }
        for(Id i=0;i<input.nodes.size();++i) if(!input.nodes[i].parent.empty()) {
            auto p=ids.find(input.nodes[i].parent);
            if(p==ids.end()) return std::unexpected("Missing Dagre parent");
            g.nodes[i].parent=p->second;
        }
        for(Id v=0;v<g.nodes.size();++v) {
            auto p=v;std::size_t depth=0;
            while(p!=absent) {
                if(++depth>65) return std::unexpected("Cyclic or too deep Dagre parent tree");
                p=g.nodes[p].parent;
            }
        }
        FlowchartRanks result;result.rank_spacing=input.rank_spacing/2;
        result.edges.resize(input.edges.size());
        for(Id i=0;i<input.edges.size();++i) {
            const auto& e=input.edges[i];const auto label=labels[i];
            if(!std::isfinite(label.width)||!std::isfinite(label.height)||label.width<0||label.height<0)
                return std::unexpected("Invalid Dagre edge label box");
            const auto v=ids.find(e.start),w=ids.find(e.end);
            if(v==ids.end()||w==ids.end()) return std::unexpected("Missing Dagre edge endpoint");
            if(!g.children(v->second).empty()||!g.children(w->second).empty())
                return std::unexpected("Dagre group endpoints require Mermaid cluster adaptation");
            if(e.min_length==0||e.min_length>10000) return std::unexpected("Invalid Dagre edge minimum length");
            result.edges[i].self=v->second==w->second;
            if(!result.edges[i].self) g.edges.push_back({v->second,w->second,static_cast<int>(2*e.min_length),1,i});
        }
        remove_cycles(g);const auto nesting=add_nesting(g);network_simplex(g);
        std::vector<std::pair<Id,Id>> proxies;
        for(const auto& e:g.edges) if(e.active && e.original!=absent) {
            result.edges[e.original].reversed=e.reversed;
            if(labels[e.original].width && labels[e.original].height) {
                const auto rank=(*g.nodes[e.v].rank+*g.nodes[e.w].rank)/2;
                const auto proxy=g.dummy(u"_ep");g.nodes[proxy].rank=rank;
                proxies.emplace_back(proxy,e.original);
            }
        }
        normalize_ranks(g,nesting);
        for(Id i=0;i<input.nodes.size();++i) {
            const auto& n=g.nodes[i];FlowchartNodeRank output{n.id,n.rank};
            if(n.top!=absent) {output.min_rank=g.nodes[n.top].rank;output.max_rank=g.nodes[n.bottom].rank;}
            result.nodes.push_back(std::move(output));
        }
        for(auto [proxy,edge]:proxies) {
            result.edges[edge].label_rank=g.nodes[proxy].rank;
            g.nodes[proxy].active=false;
        }
        g.nodes[nesting.root].active=false;
        for(auto& e:g.edges) if(e.nesting || e.v==nesting.root || e.w==nesting.root) e.active=false;
        for(auto& n:g.nodes) if(n.top!=absent) {
            n.min_rank=*g.nodes[n.top].rank; n.max_rank=*g.nodes[n.bottom].rank;
        }
        return RankedGraph{std::move(g),std::move(result)};
    } catch(const std::exception& error) {return std::unexpected(error.what());}
}
#include "dagre_order.inc"
#include "dagre_position.inc"
} // namespace

std::expected<FlowchartRanks,std::string> rank_flowchart(
    const FlowchartInput& input,std::span<const FlowchartEdgeLabelSize> labels) {
    auto ranked=build_ranked_graph(input,labels);
    if(!ranked) return std::unexpected(ranked.error());
    return std::move(ranked->summary);
}

std::expected<FlowchartLayout,std::string> layout_flowchart(
    const FlowchartInput& input,std::span<const FlowchartNodeBox> nodes,
    std::span<const FlowchartEdgeLabelSize> labels) {
    try {
        if(nodes.size()!=input.nodes.size()) return std::unexpected("Dagre needs one box per node");
        if(!std::isfinite(input.node_spacing)||input.node_spacing<0) return std::unexpected("Invalid Dagre node spacing");
        if(input.direction!=u"TB" && input.direction!=u"BT" && input.direction!=u"LR" && input.direction!=u"RL")
            return std::unexpected("Invalid Dagre direction");
        for(auto box:nodes) if(!std::isfinite(box.width)||!std::isfinite(box.height)||box.width<0||box.height<0)
            return std::unexpected("Invalid Dagre node box");
        auto ranked=build_ranked_graph(input,labels);
        if(!ranked) return std::unexpected(ranked.error());
        auto& g=ranked->graph;
        if(input.nodes.empty()) return FlowchartLayout{};
        for(Id i=0;i<nodes.size();++i) {g.nodes[i].width=nodes[i].width;g.nodes[i].height=nodes[i].height;}
        auto chains=normalize_edges(g,ranked->summary,labels);
        add_side_borders(g);
        order_nodes(g);
        return place_graph(g,input,ranked->summary,labels,chains);
    } catch(const std::exception& e) {return std::unexpected(e.what());}
}
} // namespace folia::mermaid
