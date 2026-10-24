// Port of prepareLayoutForDagre and mermaid-graphlib.js, Mermaid 12.2.0.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see ../LICENSE.
module folia.mermaid.flowchart_graph;
import std;

namespace folia::mermaid {
namespace {
struct Graph;
struct Node {
    FlowchartPreparedNode value;
    bool active = true;
    std::unique_ptr<Graph> nested;
};
struct Edge { FlowchartPreparedEdge value; bool active = true; };

// graphlib's ordinary ids preserve insertion order; integer property ids sort
// numerically. This is observable when selecting a cluster's first leaf.
std::uint64_t index_key(std::u16string_view s) {
    constexpr auto none = std::numeric_limits<std::uint64_t>::max();
    if(s.empty() || s.size()>10 || (s.size()>1 && s.front()==u'0')) return none;
    std::uint64_t value=0;
    for(auto c:s) { if(c<u'0'||c>u'9') return none; value=value*10+c-u'0'; }
    return value<4294967295ULL?value:none;
}
struct Graph {
    std::u16string direction=u"TB";
    double node_spacing=50,rank_spacing=50;
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    Node& node(std::u16string_view id) {
        for(auto& n:nodes) if(n.active && n.value.data.id==id) return n;
        throw std::runtime_error("Missing Mermaid graph node");
    }
    bool contains(std::u16string_view id) const {
        return std::ranges::any_of(nodes,[&](const Node& n){return n.active&&n.value.data.id==id;});
    }
    std::vector<std::u16string> order() const {
        std::vector<std::u16string> result;
        for(const auto& n:nodes) if(n.active) result.push_back(n.value.data.id);
        std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return index_key(a)<index_key(b);});
        return result;
    }
    std::vector<std::u16string> children(std::u16string_view id) const {
        auto result=order();
        std::erase_if(result,[&](const auto& child){
            return std::ranges::none_of(nodes,[&](const Node& n){return n.active&&n.value.data.id==child&&n.value.data.parent==id;});
        });
        return result;
    }
    void set_node(FlowchartPreparedNode value) {
        if(contains(value.data.id)) {
            auto& old=node(value.data.id);value.data.parent=old.value.data.parent;
            old.value=std::move(value);
        } else nodes.push_back({std::move(value)});
    }
    void set_edge(FlowchartPreparedEdge value) {
        for(const auto& id:{value.data.start,value.data.end}) if(!contains(id)) {
            FlowchartNode placeholder;placeholder.id=id;set_node({std::move(placeholder)});
        }
        for(auto& e:edges) if(e.active && e.value.data.start==value.data.start &&
            e.value.data.end==value.data.end && e.value.name==value.name) {e.value=std::move(value);return;}
        edges.push_back({std::move(value)});
    }
    void remove_node(std::u16string_view id) {
        if(!contains(id)) return;
        node(id).active=false;
        for(auto& n:nodes) if(n.active&&n.value.data.parent==id) n.value.data.parent.clear();
        for(auto& e:edges) if(e.value.data.start==id||e.value.data.end==id) e.active=false;
    }
};
struct Cluster {
    std::u16string id,anchor,direction;
    std::vector<std::u16string> descendants;
    bool external=false;
};
struct Adapter {
    std::vector<Cluster> clusters;
    Cluster* cluster(std::u16string_view id) {
        for(auto& c:clusters) if(c.id==id) return &c;
        return nullptr;
    }
    bool descendant(std::u16string_view id,std::u16string_view parent) {
        const auto c=cluster(parent);
        return c && std::ranges::find(c->descendants,id)!=c->descendants.end();
    }
    std::vector<std::u16string> descendants(Graph& g,std::u16string_view id) {
        const auto children=g.children(id);auto result=children;
        for(const auto& child:children) {auto nested=descendants(g,child);result.insert(result.end(),nested.begin(),nested.end());}
        return result;
    }
    bool common_edge(Graph& g,std::u16string_view id,std::u16string_view child) {
        for(const auto& e:g.edges) if(e.active&&(e.value.data.start==id||e.value.data.end==id)) {
            // Keep the original asymmetric target comparison in findCommonEdges.
            const auto& start=e.value.data.start==id?child:std::u16string_view(e.value.data.start);
            for(const auto& other:g.edges) if(other.active &&
                (other.value.data.start==child||other.value.data.end==child) &&
                other.value.data.start==start && other.value.data.end==e.value.data.end) return true;
        }
        return false;
    }
    std::u16string leaf(Graph& g,std::u16string_view id,std::u16string_view root) {
        const auto children=g.children(id);
        if(children.empty()) return std::u16string(id);
        std::u16string reserve;
        for(const auto& child:children) {
            auto candidate=leaf(g,child,root);
            if(common_edge(g,root,candidate)) reserve=std::move(candidate);
            else return candidate;
        }
        return reserve;
    }
    bool inside_extractable(Graph& g,std::u16string_view id,std::u16string_view root) {
        auto parent=g.node(id).value.data.parent;
        while(!parent.empty()&&parent!=root) {
            if(auto c=cluster(parent);c&&!c->external) return true;
            parent=g.node(parent).value.data.parent;
        }
        return false;
    }
    void adjust(Graph& g) {
        for(const auto& id:g.order()) if(!g.children(id).empty())
            clusters.push_back({id,leaf(g,id,id),g.node(id).value.data.direction,descendants(g,id)});
        for(auto& c:clusters) for(const auto& e:g.edges) if(e.active &&
            descendant(e.value.data.start,c.id)!=descendant(e.value.data.end,c.id)) c.external=true;
        for(auto& c:clusters) {
            const auto original=c.anchor;
            const auto parent=g.node(original).value.data.parent;
            if(auto p=cluster(parent);parent!=c.id&&p&&!p->external) c.anchor=parent;
            const bool outgoing=std::ranges::any_of(g.edges,[&](const Edge& e){return e.active&&e.value.data.start==c.id;});
            if(c.external&&outgoing&&inside_extractable(g,original,c.id)) {
                for(const auto& child:g.children(c.id)) {
                    if(child==parent||descendant(child,parent)) continue;
                    const auto candidate=leaf(g,child,c.id);
                    if(!inside_extractable(g,candidate,c.id)) {c.anchor=candidate;break;}
                }
            }
        }
        const auto edge_count=g.edges.size();
        for(std::size_t i=0;i<edge_count;++i) {
            auto value=g.edges[i].value;
            auto from=cluster(value.data.start),to=cluster(value.data.end);
            if(!from&&!to) continue;
            g.edges[i].active=false;
            const auto replace=[&](Cluster* c,std::u16string& endpoint,std::u16string& marker) {
                if(!c||!c->external||endpoint==c->anchor) return;
                marker=endpoint;endpoint=c->anchor;
                if(auto p=cluster(g.node(endpoint).value.data.parent)) p->external=true;
            };
            replace(from,value.data.start,value.from_cluster);replace(to,value.data.end,value.to_cluster);
            g.set_edge(std::move(value));
        }
        extract(g,0);
    }
    void copy(Graph& from,Graph& to,const std::u16string& id,const std::u16string& root) {
        auto nodes=from.children(id);if(id!=root) nodes.push_back(id);
        for(const auto& child:nodes) {
            if(!from.children(child).empty()) copy(from,to,child,root);
            else {
                auto n=from.node(child).value;
                if(n.data.parent==root) n.data.parent.clear();
                if(id!=root && child!=id) n.data.parent=id;
                const auto parent=n.data.parent;
                to.set_node(std::move(n));
                if(!parent.empty()&&!to.contains(parent)) {
                    FlowchartNode placeholder;placeholder.id=parent;to.set_node({std::move(placeholder)});
                }
                to.node(child).value.data.parent=parent;
                // graphlib.edges(node) returns all edges, not only incident ones.
                for(const auto& e:from.edges) if(e.active && e.value.data.start!=root && e.value.data.end!=root &&
                    (descendant(e.value.data.start,root)||descendant(e.value.data.end,root))) to.set_edge(e.value);
            }
            from.remove_node(child);
        }
    }
    void extract(Graph& g,std::size_t depth) {
        if(depth>10) throw std::runtime_error("Mermaid cluster extraction depth exceeds 10");
        const auto ids=g.order();
        for(const auto& id:ids) {
            const auto c=cluster(id);
            if(!c||c->external||!g.contains(id)||g.children(id).empty()) continue;
            auto nested=std::make_unique<Graph>();
            nested->direction=c->direction.empty()?(g.direction==u"TB"?u"LR":u"TB"):c->direction;
            copy(g,*nested,id,id);
            g.node(id).nested=std::move(nested);
        }
        for(const auto& id:g.order()) if(auto& n=g.node(id);n.nested) extract(*n.nested,depth+1);
    }
};
FlowchartPreparedGraph finish(Graph& g) {
    FlowchartPreparedGraph out;
    out.direction=g.direction;out.node_spacing=g.node_spacing;out.rank_spacing=g.rank_spacing;
    for(const auto& id:g.order()) {
        auto& n=g.node(id);auto value=n.value;
        if(n.nested) {value.subgraph=out.subgraphs.size();out.subgraphs.push_back(finish(*n.nested));}
        out.nodes.push_back(std::move(value));
    }
    for(auto& e:g.edges) if(e.active) out.edges.push_back(std::move(e.value));
    return out;
}
void validate(const FlowchartInput& source) {
    if(source.nodes.size()>10000||source.edges.size()>500) throw std::runtime_error("Mermaid graph size exceeds default limits");
    if(!std::isfinite(source.node_spacing)||source.node_spacing<0||!std::isfinite(source.rank_spacing)||source.rank_spacing<0)
        throw std::runtime_error("Invalid Mermaid graph spacing");
    const auto direction=[](std::u16string_view d){return d==u"TB"||d==u"BT"||d==u"LR"||d==u"RL";};
    if(!direction(source.direction)) throw std::runtime_error("Invalid Mermaid graph direction");
    std::unordered_map<std::u16string,const FlowchartNode*> nodes;
    for(const auto& n:source.nodes) {
        if(n.id.empty()||!nodes.emplace(n.id,&n).second) throw std::runtime_error("Empty or duplicate Mermaid node id");
        if(!n.direction.empty()&&!direction(n.direction)) throw std::runtime_error("Invalid Mermaid subgraph direction");
    }
    for(const auto& n:source.nodes) {
        auto current=&n;std::size_t depth=0;
        while(!current->parent.empty()) {
            auto p=nodes.find(current->parent);
            if(p==nodes.end()) throw std::runtime_error("Missing Mermaid parent");
            if(++depth>64) throw std::runtime_error("Cyclic or too deep Mermaid parent tree");
            current=p->second;
        }
    }
    for(const auto& e:source.edges) if(!nodes.contains(e.start)||!nodes.contains(e.end))
        throw std::runtime_error("Missing Mermaid edge endpoint");
}
}

std::expected<FlowchartGraphPreparation,std::string> prepare_flowchart_graph(const FlowchartInput& source) {
    try {
        validate(source);
        Graph g;g.direction=source.direction;g.node_spacing=source.node_spacing;g.rank_spacing=source.rank_spacing;
        for(std::size_t i=0;i<source.nodes.size();++i) {
            const auto& n=source.nodes[i];g.set_node({n,i});
            if(!n.parent.empty()&&!g.contains(n.parent)) {
                FlowchartNode placeholder;placeholder.id=n.parent;g.set_node({std::move(placeholder)});
            }
            g.node(n.id).value.data.parent=n.parent;
        }
        for(std::size_t i=0;i<source.edges.size();++i) {
            const auto& edge=source.edges[i];
            if(edge.start!=edge.end) {g.set_edge({edge,edge.id,{},{},i});continue;}
            const auto parent=g.node(edge.start).value.data.parent;
            const bool group=g.node(edge.start).value.data.group;
            const auto first=edge.start+u"---"+edge.start+u"---1",second=edge.start+u"---"+edge.start+u"---2";
            for(const auto& id:{first,second}) {
                FlowchartNode dummy;dummy.id=id;dummy.shape=u"labelRect";dummy.parent=parent;dummy.padding=0;
                g.set_node({std::move(dummy)});
            }
            for(int segment=0;segment<3;++segment) {
                FlowchartPreparedEdge e{edge,{}, {}, {},i,segment};
                e.name=edge.start+u"-cyclic-special-"+char16_t(u'0'+segment);
                if(segment==0) {e.data.end=first;e.data.label.clear();e.data.arrow_end=u"none";e.data.id=edge.start+u"-cyclic-special-1";}
                if(segment==1) {e.data.start=first;e.data.end=second;e.data.arrow_start=e.data.arrow_end=u"none";e.data.id=edge.start+u"-cyclic-special-mid";}
                if(segment==2) {e.data.start=second;e.data.label.clear();e.data.arrow_start=u"none";e.data.id=edge.start+u"-cyclic-special-2";}
                if(group&&segment==0) e.from_cluster=edge.start;
                if(group&&segment==2) e.to_cluster=edge.end;
                g.set_edge(std::move(e));
            }
        }
        Adapter adapter;adapter.adjust(g);
        return FlowchartGraphPreparation{source,finish(g)};
    } catch(const std::exception& e) {return std::unexpected(e.what());}
}
}
