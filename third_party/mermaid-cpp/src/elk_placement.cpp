// SPDX-License-Identifier: EPL-2.0
// Port of ELK 0.9.3 BKNodePlacer conflict marking and BKAligner.
module folia.mermaid.elk_placement;
import std;

namespace folia::mermaid {
namespace {
struct Neighbor { std::size_t node, edge; };
struct Neighborhood {
    std::vector<std::size_t> index;
    std::vector<std::vector<Neighbor>> left, right;
    explicit Neighborhood(const ElkLayeredGraph& g)
        : index(g.nodes.size()), left(g.nodes.size()), right(g.nodes.size()) {
        for (const auto& layer : g.layers)
            for (std::size_t i = 0; i < layer.size(); ++i) index[layer[i]] = i;
        for (std::size_t n = 0; n < g.nodes.size(); ++n) {
            for (const auto p : g.nodes[n].ports) {
                for (const auto e : g.ports[p].incoming)
                    left[n].push_back({g.ports[g.edges[e].source_port].node, e});
                for (const auto e : g.ports[p].outgoing)
                    right[n].push_back({g.ports[g.edges[e].target_port].node, e});
            }
            const auto less = [&](const Neighbor& a, const Neighbor& b) { return index[a.node] < index[b.node]; };
            std::stable_sort(left[n].begin(), left[n].end(), less);
            std::stable_sort(right[n].begin(), right[n].end(), less);
        }
    }
};
bool inner_segment(const ElkLayeredGraph& g, const Neighborhood& ni, std::size_t n) {
    if (g.nodes[n].kind != ElkLayerNodeKind::long_edge) return false;
    return std::ranges::any_of(ni.left[n], [&](const Neighbor& p) {
        return g.nodes[p.node].kind == ElkLayerNodeKind::long_edge;
    });
}
std::vector<bool> mark_conflicts(const ElkLayeredGraph& g, const Neighborhood& ni) {
    std::vector<bool> marked(g.edges.size());
    for (std::size_t i = 1; i + 1 < g.layers.size(); ++i) {
        const auto& layer = g.layers[i + 1];
        std::size_t k0 = 0, l = 0;
        for (std::size_t l1 = 0; l1 < layer.size(); ++l1) {
            const auto n = layer[l1];
            const bool inner = inner_segment(g, ni, n);
            if (l1 + 1 != layer.size() && !inner) continue;
            const auto k1 = inner ? ni.index[ni.left[n].front().node] : g.layers[i].size() - 1;
            for (; l <= l1; ++l) {
                const auto current = layer[l];
                if (inner_segment(g, ni, current)) continue;
                for (const auto neighbor : ni.left[current]) {
                    const auto k = ni.index[neighbor.node];
                    if (k < k0 || k > k1) marked[neighbor.edge] = true;
                }
            }
            k0 = k1;
        }
    }
    return marked;
}
ElkAlignedBlockLayout align_blocks(const ElkLayeredGraph& g, const Neighborhood& ni,
    const std::vector<bool>& marked, bool right, bool down) {
    ElkAlignedBlockLayout b{.right=right, .down=down};
    const auto count = g.nodes.size();
    b.root.resize(count); b.align.resize(count); b.inner_shift.resize(count);
    b.block_size.resize(count); b.only_dummies.resize(count, true);
    std::iota(b.root.begin(), b.root.end(), 0); b.align = b.root;
    for (std::size_t li = 0; li < g.layers.size(); ++li) {
        const auto& layer = g.layers[right ? li : g.layers.size() - li - 1];
        std::ptrdiff_t r = down ? -1 : std::numeric_limits<std::ptrdiff_t>::max();
        for (std::size_t vi = 0; vi < layer.size(); ++vi) {
            const auto v = layer[down ? vi : layer.size() - vi - 1];
            const auto& neighbors = right ? ni.left[v] : ni.right[v];
            if (neighbors.empty()) continue;
            const auto low = (neighbors.size() - 1) / 2, high = neighbors.size() / 2;
            for (std::size_t j = 0; j <= high - low; ++j) {
                if (b.align[v] != v) break;
                const auto u = neighbors[down ? low + j : high - j];
                const auto position = static_cast<std::ptrdiff_t>(ni.index[u.node]);
                if (marked[u.edge] || (down ? r >= position : r <= position)) continue;
                b.align[u.node] = v;
                b.root[v] = b.root[u.node];
                b.align[v] = b.root[v];
                b.only_dummies[b.root[v]] = b.only_dummies[b.root[v]] &&
                    g.nodes[v].kind == ElkLayerNodeKind::long_edge;
                r = position;
            }
        }
    }
    for (const auto& layer : g.layers) for (const auto root : layer) {
        if (b.root[root] != root) continue;
        double above = g.nodes[root].margin_top;
        double below = g.nodes[root].height + g.nodes[root].margin_bottom;
        auto current = root;
        for (auto next = b.align[current]; next != root; next = b.align[current]) {
            // BK's getEdge uses the first connected edge in current port order.
            std::optional<std::size_t> connection;
            for (const auto p : g.nodes[current].ports) {
                for (const auto e : g.ports[p].incoming)
                    if (g.ports[g.edges[e].source_port].node == next) { connection = e; break; }
                if (connection) break;
                for (const auto e : g.ports[p].outgoing)
                    if (g.ports[g.edges[e].target_port].node == next) { connection = e; break; }
                if (connection) break;
            }
            const auto& edge = g.edges[*connection];
            const double difference = g.ports[edge.source_port].y - g.ports[edge.target_port].y;
            b.inner_shift[next] = b.inner_shift[current] + (right ? difference : -difference);
            above = std::max(above, g.nodes[next].margin_top - b.inner_shift[next]);
            below = std::max(below, b.inner_shift[next] + g.nodes[next].height + g.nodes[next].margin_bottom);
            current = next;
        }
        current = root;
        do { b.inner_shift[current] += above; current = b.align[current]; } while (current != root);
        b.block_size[root] = above + below;
    }
    return b;
}
struct Compactor {
    const ElkLayeredGraph& g;
    const Neighborhood& ni;
    ElkAlignedBlockLayout& b;
    ElkPlacementSpacing spacing;
    std::vector<double> y, shift;
    std::vector<std::size_t> sink;
    std::vector<bool> assigned, finished, used;
    struct Pending { std::size_t node; bool root; std::optional<std::size_t> edge; };
    std::deque<Pending> pending;
    struct ClassEdge { std::size_t target; double separation; };
    std::vector<std::vector<ClassEdge>> classes;
    std::vector<std::size_t> indegree;
    std::vector<bool> active;
    static constexpr double infinity = std::numeric_limits<double>::infinity();
    Compactor(const ElkLayeredGraph& graph, const Neighborhood& neighbors,
        ElkAlignedBlockLayout& layout, ElkPlacementSpacing distances)
        : g(graph), ni(neighbors), b(layout), spacing(distances), y(g.nodes.size()),
          shift(g.nodes.size(), b.down ? infinity : -infinity), sink(g.nodes.size()),
          assigned(g.nodes.size()), finished(g.nodes.size()), used(g.nodes.size()),
          classes(g.nodes.size()), indegree(g.nodes.size()), active(g.nodes.size()) {
        std::iota(sink.begin(),sink.end(),0);
    }
    double separation(std::size_t a, std::size_t c) const {
        const bool ad=g.nodes[a].kind==ElkLayerNodeKind::long_edge;
        const bool cd=g.nodes[c].kind==ElkLayerNodeKind::long_edge;
        if(g.nodes[a].kind==ElkLayerNodeKind::label&&g.nodes[c].kind==ElkLayerNodeKind::label)
            return spacing.edge_edge;
        return ad&&cd ? spacing.edge_edge : ad||cd ? spacing.edge_node : spacing.node_node;
    }
    std::size_t other(std::size_t e, std::size_t n) const {
        const auto& edge=g.edges[e];
        const auto source=g.ports[edge.source_port].node;
        return source==n ? g.ports[edge.target_port].node : source;
    }
    std::pair<Pending,bool> pick(Pending p) const {
        bool has_edges=false;
        if (!used[b.root[p.node]]) {
            const bool incoming=p.root ? b.right : !b.right;
            for (const auto port : g.nodes[p.node].ports) {
                const auto& edges=incoming ? g.ports[port].incoming : g.ports[port].outgoing;
                for (const auto e : edges) {
                    has_edges=true;
                    if (finished[b.root[other(e,p.node)]]) { p.edge=e; return {p,true}; }
                }
            }
        }
        p.edge.reset(); return {p,has_edges};
    }
    double bound(std::size_t n, bool is_root) {
        auto [p,has_edges]=pick({n,is_root,{}});
        if (!p.edge) {
            if (has_edges) pending.push_back(p);
            return b.down ? -infinity : infinity;
        }
        const auto& edge=g.edges[*p.edge];
        const auto& source=g.ports[edge.source_port]; const auto& target=g.ports[edge.target_port];
        const auto& root_port=(is_root ? b.right : !b.right) ? target : source;
        const auto& other_port=(is_root ? b.right : !b.right) ? source : target;
        used[b.root[source.node]]=true; used[b.root[target.node]]=true;
        return y[b.root[other_port.node]]+b.inner_shift[other_port.node]+other_port.y-
            b.inner_shift[root_port.node]-root_port.y;
    }
    double threshold(double old, std::size_t root, std::size_t current) {
        const bool is_root=root==current, last=b.align[current]==root;
        if (!is_root&&!last) return old;
        double value=is_root ? bound(root,true) : old;
        if (std::isinf(value)&&last) value=bound(current,false);
        return value;
    }
    void place_block(std::size_t root) {
        if (assigned[root]) return;
        assigned[root]=true;
        bool initial=true;
        auto current=root;
        double thresh=b.down ? -infinity : infinity;
        do {
            const auto index=ni.index[current];
            const auto& layer=g.layers[g.nodes[current].layer];
            const bool has_neighbor=b.down ? index>0 : index+1<layer.size();
            if (has_neighbor) {
                const auto neighbor=layer[b.down?index-1:index+1], nr=b.root[neighbor];
                place_block(nr);
                thresh=threshold(thresh,root,current);
                if (sink[root]==root) sink[root]=sink[nr];
                const auto& cn=g.nodes[current]; const auto& nn=g.nodes[neighbor];
                if (sink[root]==sink[nr]) {
                    const auto space=separation(current,neighbor);
                    const auto position=b.down
                        ? y[nr]+b.inner_shift[neighbor]+nn.height+nn.margin_bottom+space+cn.margin_top-b.inner_shift[current]
                        : y[nr]+b.inner_shift[neighbor]-nn.margin_top-space-cn.margin_bottom-cn.height-b.inner_shift[current];
                    const auto bounded=b.down?std::max(position,thresh):std::min(position,thresh);
                    y[root]=initial?bounded:b.down?std::max(y[root],bounded):std::min(y[root],bounded);
                    initial=false;
                } else {
                    // Original class separation uses node-node spacing for all types.
                    const auto distance=b.down
                        ? y[root]+b.inner_shift[current]-cn.margin_top-y[nr]-b.inner_shift[neighbor]-nn.height-nn.margin_bottom-spacing.node_node
                        : y[root]+b.inner_shift[current]+cn.height+cn.margin_bottom+spacing.node_node-y[nr]-b.inner_shift[neighbor]+nn.margin_top;
                    classes[sink[root]].push_back({sink[nr],distance});
                    ++indegree[sink[nr]]; active[sink[root]]=active[sink[nr]]=true;
                }
            } else thresh=threshold(thresh,root,current);
            current=b.align[current];
        } while (current!=root);
        finished[root]=true;
    }
    void place_classes() {
        std::deque<std::size_t> ready;
        std::vector<std::optional<double>> offsets(g.nodes.size());
        for (std::size_t n=0;n<active.size();++n) if (active[n]&&indegree[n]==0) ready.push_back(n);
        while (!ready.empty()) {
            const auto n=ready.front(); ready.pop_front();
            if (!offsets[n]) offsets[n]=0;
            for (const auto e : classes[n]) {
                const auto position=*offsets[n]+e.separation;
                if (!offsets[e.target]) offsets[e.target]=position;
                else offsets[e.target]=b.down?std::min(*offsets[e.target],position):std::max(*offsets[e.target],position);
                if (--indegree[e.target]==0) ready.push_back(e.target);
            }
        }
        for (std::size_t n=0;n<active.size();++n) if (active[n]) shift[n]=*offsets[n];
    }
    bool straighten(const Pending& p) {
        const auto& edge=g.edges[*p.edge];
        const auto& source=g.ports[edge.source_port]; const auto& target=g.ports[edge.target_port];
        const auto& block=source.node==p.node?source:target;
        const auto& fix=source.node==p.node?target:source;
        const double delta=y[block.node]+b.inner_shift[block.node]+block.y-
            y[fix.node]-b.inner_shift[fix.node]-fix.y;
        if (delta==0||std::abs(delta)>=std::numeric_limits<double>::max()) return false;
        const bool above=delta>0;
        double available=std::abs(delta);
        auto current=block.node;
        do {
            current=b.align[current];
            const auto& cn=g.nodes[current]; const auto& layer=g.layers[cn.layer];
            const auto index=ni.index[current];
            if (above?index>0:index+1<layer.size()) {
                const auto neighbor=layer[above?index-1:index+1]; const auto& nn=g.nodes[neighbor];
                const auto current_y=y[b.root[current]]+b.inner_shift[current];
                const auto neighbor_y=y[b.root[neighbor]]+b.inner_shift[neighbor];
                const auto space=above
                    ? current_y-cn.margin_top-neighbor_y-nn.height-nn.margin_bottom-separation(current,neighbor)
                    : neighbor_y-nn.margin_top-current_y-cn.height-cn.margin_bottom-separation(current,neighbor);
                available=std::min(available,space);
            }
        } while (current!=block.node);
        current=block.node;
        do { y[current]+=above?-available:available; current=b.align[current]; } while (current!=block.node);
        return available>0;
    }
    std::vector<double> run() {
        for (std::size_t li=0;li<g.layers.size();++li) {
            const auto& layer=g.layers[b.right?li:g.layers.size()-li-1];
            for (std::size_t vi=0;vi<layer.size();++vi) {
                const auto n=layer[b.down?vi:layer.size()-vi-1];
                if (b.root[n]==n) place_block(n);
            }
        }
        place_classes();
        // Copy/shift roots in the original directional layer traversal order.
        for (std::size_t li=0;li<g.layers.size();++li) {
            const auto& layer=g.layers[b.right?li:g.layers.size()-li-1];
            for (const auto n : layer) {
                y[n]=y[b.root[n]];
                if (n==b.root[n]&&std::isfinite(shift[sink[n]])) y[n]+=shift[sink[n]];
            }
        }
        std::vector<Pending> stack;
        while (!pending.empty()) {
            auto [p,has_edges]=pick(pending.front()); pending.pop_front();
            if (p.edge&&!straighten(p)) stack.push_back(p);
        }
        while (!stack.empty()) { straighten(stack.back()); stack.pop_back(); }
        return y;
    }
};
double layout_size(const ElkAlignedBlockLayout& b, const std::vector<double>& y) {
    double minimum=std::numeric_limits<double>::infinity(), maximum=-minimum;
    for (std::size_t n=0;n<y.size();++n) {
        minimum=std::min(minimum,y[n]); maximum=std::max(maximum,y[n]+b.block_size[b.root[n]]);
    }
    return maximum-minimum;
}
bool ordered(const ElkLayeredGraph& g, const std::vector<double>& y) {
    for (const auto& layer : g.layers) {
        double previous=-std::numeric_limits<double>::infinity();
        for (const auto n : layer) {
            const auto top=y[n]-g.nodes[n].margin_top, bottom=y[n]+g.nodes[n].height+g.nodes[n].margin_bottom;
            if (top<=previous||bottom<=previous) return false;
            previous=bottom;
        }
    }
    return true;
}
}
std::expected<ElkBlockAlignment, std::string> elk_align_flat_blocks(const ElkLayeredGraph& g) {
    for (const auto& edge : g.edges) {
        const auto& source = g.ports[edge.source_port]; const auto& target = g.ports[edge.target_port];
        if (source.side != ElkPortSide::east || target.side != ElkPortSide::west ||
            g.nodes[target.node].layer != g.nodes[source.node].layer + 1)
            return std::unexpected("ELK block alignment requires adjacent east/west edges");
    }
    const Neighborhood ni(g);
    ElkBlockAlignment result;
    result.marked_edges = mark_conflicts(g, ni);
    for (std::size_t i = 0; i < 4; ++i)
        result.layouts[i] = align_blocks(g, ni, result.marked_edges, i < 2, i % 2 == 0);
    return result;
}
std::expected<std::vector<double>, std::string> elk_place_flat_nodes(
    const ElkLayeredGraph& g, ElkPlacementSpacing spacing, bool favor_straight_edges) {
    auto blocks=elk_align_flat_blocks(g);
    if (!blocks) return std::unexpected(blocks.error());
    if (g.nodes.empty()) return std::vector<double>{};
    const Neighborhood ni(g);
    std::array<std::vector<double>,4> positions;
    std::array<double,4> sizes{},minimum{},maximum{};
    std::size_t narrowest=0;
    for (std::size_t d=0;d<4;++d) {
        auto& b=blocks->layouts[d];
        auto y=Compactor(g,ni,b,spacing).run();
        sizes[d]=layout_size(b,y);
        if (sizes[d]<sizes[narrowest]) narrowest=d;
        minimum[d]=std::numeric_limits<double>::infinity(); maximum[d]=-minimum[d];
        for (std::size_t n=0;n<y.size();++n) {
            y[n]+=b.inner_shift[n];
            minimum[d]=std::min(minimum[d],y[n]); maximum[d]=std::max(maximum[d],y[n]+g.nodes[n].height);
        }
        positions[d]=std::move(y);
    }
    if (!favor_straight_edges) {
        std::vector<double> balanced(g.nodes.size());
        for (std::size_t n=0;n<balanced.size();++n) {
            std::array<double,4> candidates;
            for (std::size_t d=0;d<4;++d)
                candidates[d]=positions[d][n]+(blocks->layouts[d].down
                    ? minimum[narrowest]-minimum[d] : maximum[narrowest]-maximum[d]);
            std::ranges::sort(candidates); balanced[n]=(candidates[1]+candidates[2])/2;
        }
        if (ordered(g,balanced)) return balanced;
    }
    std::optional<std::size_t> chosen;
    for (std::size_t d=0;d<4;++d)
        if (ordered(g,positions[d])&&(!chosen||sizes[d]<sizes[*chosen])) chosen=d;
    return std::move(positions[chosen.value_or(0)]);
}
}
