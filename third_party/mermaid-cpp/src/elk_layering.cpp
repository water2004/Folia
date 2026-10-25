// SPDX-License-Identifier: EPL-2.0
// Derived from ELK 0.9.3 NetworkSimplex / NetworkSimplexLayerer, not Dagre.
module folia.mermaid.elk_layering;
import std;

namespace folia::mermaid {
namespace {
using Index = std::size_t;
constexpr Index absent = std::numeric_limits<Index>::max();
struct Node {
    std::vector<Index> incoming, outgoing, unknown;
    int layer = 0, post = 0, low = 0;
    bool tree = false;
};
struct Edge {
    Index source = 0, target = 0;
    double weight = 1, cut = 0;
    bool tree = false, visited = false;
};
struct NetworkSimplex {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<Index> order, edge_order, tree_edges;
    int post_order = 1;

    std::vector<Index> connected(Index node) const {
        auto result = nodes[node].incoming;
        result.insert(result.end(), nodes[node].outgoing.begin(), nodes[node].outgoing.end());
        return result;
    }
    Index other(Index edge, Index node) const {
        return edges[edge].source == node ? edges[edge].target : edges[edge].source;
    }
    int slack(Index e) const { return nodes[edges[e].target].layer - nodes[edges[e].source].layer - 1; }
    void clear_visits() { for (auto& e : edges) e.visited = false; }
    void initialize() {
        for (const auto n : order) {
            nodes[n].tree = false;
            edge_order.insert(edge_order.end(), nodes[n].outgoing.begin(), nodes[n].outgoing.end());
        }
        for (const auto e : edge_order) edges[e].tree = edges[e].visited = false;
    }
    void topological_numbering() {
        std::vector<Index> incident(nodes.size());
        std::deque<Index> roots;
        for (const auto n : order) {
            incident[n] = nodes[n].incoming.size();
            if (!incident[n]) roots.push_back(n);
        }
        while (!roots.empty()) {
            const auto n = roots.front(); roots.pop_front();
            for (const auto e : nodes[n].outgoing) {
                const auto target = edges[e].target;
                nodes[target].layer = std::max(nodes[target].layer, nodes[n].layer + 1);
                if (--incident[target] == 0) roots.push_back(target);
            }
        }
    }
    Index tight_tree(Index node) {
        Index count = 1;
        nodes[node].tree = true;
        for (const auto e : connected(node)) {
            if (edges[e].visited) continue;
            edges[e].visited = true;
            const auto opposite = other(e, node);
            if (edges[e].tree) count += tight_tree(opposite);
            else if (!nodes[opposite].tree && slack(e) == 0) {
                edges[e].tree = true;
                tree_edges.push_back(e);
                count += tight_tree(opposite);
            }
        }
        return count;
    }
    int postorder(Index node) {
        int lowest = std::numeric_limits<int>::max();
        for (const auto e : connected(node)) {
            if (edges[e].tree && !edges[e].visited) {
                edges[e].visited = true;
                lowest = std::min(lowest, postorder(other(e, node)));
            }
        }
        nodes[node].post = post_order;
        nodes[node].low = std::min(lowest, post_order++);
        return nodes[node].low;
    }
    void cutvalues() {
        std::vector<Index> leaves;
        for (const auto n : order) {
            nodes[n].unknown.clear();
            for (const auto e : connected(n)) if (edges[e].tree) nodes[n].unknown.push_back(e);
            if (nodes[n].unknown.size() == 1) leaves.push_back(n);
        }
        for (auto n : leaves) {
            while (nodes[n].unknown.size() == 1) {
                const auto selected = nodes[n].unknown.front();
                auto& cut = edges[selected].cut;
                cut = edges[selected].weight;
                const auto source = edges[selected].source, target = edges[selected].target;
                for (const auto e : connected(n)) {
                    if (e == selected) continue;
                    const auto& edge = edges[e];
                    if (edge.tree) {
                        if (source == edge.source || target == edge.target) cut -= edge.cut - edge.weight;
                        else cut += edge.cut - edge.weight;
                    } else if (n == source) cut += edge.source == n ? edge.weight : -edge.weight;
                    else cut += edge.source == n ? -edge.weight : edge.weight;
                }
                std::erase(nodes[source].unknown, selected);
                std::erase(nodes[target].unknown, selected);
                n = source == n ? target : source;
            }
        }
    }
    bool in_head(Index node, Index e) const {
        const auto& source = nodes[edges[e].source];
        const auto& target = nodes[edges[e].target];
        const auto post = nodes[node].post;
        if (source.low <= post && post <= source.post && target.low <= post && post <= target.post)
            return source.post >= target.post;
        return source.post < target.post;
    }
    void feasible_tree() {
        topological_numbering();
        if (edge_order.empty()) return;
        clear_visits();
        while (tight_tree(order.front()) < order.size()) {
            Index selected = absent;
            int minimum = std::numeric_limits<int>::max();
            for (const auto e : edge_order) {
                if (nodes[edges[e].source].tree != nodes[edges[e].target].tree && slack(e) < minimum) {
                    minimum = slack(e); selected = e;
                }
            }
            if (selected == absent) throw std::runtime_error("Disconnected ELK simplex component");
            if (nodes[edges[selected].target].tree) minimum = -minimum;
            for (const auto n : order) if (nodes[n].tree) nodes[n].layer += minimum;
            clear_visits();
        }
        clear_visits(); postorder(order.front()); cutvalues();
    }
    Index leave_edge() const {
        for (const auto e : tree_edges) if (edges[e].tree && edges[e].cut < -1e-10) return e;
        return absent;
    }
    Index enter_edge(Index leave) const {
        Index result = absent;
        int minimum = std::numeric_limits<int>::max();
        for (const auto e : edge_order) {
            if (in_head(edges[e].source, leave) && !in_head(edges[e].target, leave) && slack(e) < minimum) {
                result = e; minimum = slack(e);
            }
        }
        return result;
    }
    void exchange(Index leave, Index enter) {
        if (enter == absent) throw std::runtime_error("No ELK simplex entering edge");
        edges[leave].tree = false; std::erase(tree_edges, leave);
        edges[enter].tree = true; tree_edges.push_back(enter);
        auto delta = slack(enter);
        if (!in_head(edges[enter].target, leave)) delta = -delta;
        for (const auto n : order) if (!in_head(n, leave)) nodes[n].layer += delta;
        post_order = 1; clear_visits(); postorder(order.front()); cutvalues();
    }
    std::vector<std::pair<Index, Index>> remove_subtrees() {
        std::vector<std::pair<Index, Index>> stack;
        std::deque<Index> leaves;
        for (const auto n : order) if (connected(n).size() == 1) leaves.push_back(n);
        while (!leaves.empty()) {
            const auto n = leaves.front(); leaves.pop_front();
            const auto incident = connected(n);
            if (incident.empty()) continue;
            const auto e = incident.front(), opposite = other(e, n);
            if (!nodes[n].outgoing.empty()) std::erase(nodes[opposite].incoming, e);
            else std::erase(nodes[opposite].outgoing, e);
            if (connected(opposite).size() == 1) leaves.push_back(opposite);
            stack.emplace_back(n, e); std::erase(order, n);
        }
        return stack;
    }
    void reattach_subtrees(const std::vector<std::pair<Index, Index>>& stack) {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            const auto [n, e] = *it;
            const auto placed = other(e, n);
            if (edges[e].target == n) {
                nodes[placed].outgoing.push_back(e); nodes[n].layer = nodes[placed].layer + 1;
            } else {
                nodes[placed].incoming.push_back(e); nodes[n].layer = nodes[placed].layer - 1;
            }
            order.push_back(n);
        }
    }
    void normalize_and_balance(std::span<const int> previous) {
        int lowest = std::numeric_limits<int>::max(), highest = std::numeric_limits<int>::min();
        for (const auto n : order) {
            lowest = std::min(lowest, nodes[n].layer); highest = std::max(highest, nodes[n].layer);
        }
        std::vector<int> filling(static_cast<Index>(highest - lowest + 1));
        for (const auto n : order) { nodes[n].layer -= lowest; ++filling[nodes[n].layer]; }
        for (Index i = 0; i < std::min(filling.size(), previous.size()); ++i) filling[i] += previous[i];
        for (const auto n : order) {
            if (nodes[n].incoming.size() != nodes[n].outgoing.size()) continue;
            int min_in = std::numeric_limits<int>::max(), min_out = min_in;
            for (const auto e : connected(n)) {
                const auto span = nodes[edges[e].target].layer - nodes[edges[e].source].layer;
                // Preserve the original minimalSpan conditional and iteration order.
                if (edges[e].target == n && span < min_in) min_in = span;
                else if (span < min_out) min_out = span;
            }
            if (min_in == std::numeric_limits<int>::max()) min_in = -1;
            if (min_out == std::numeric_limits<int>::max()) min_out = -1;
            auto selected = nodes[n].layer;
            for (auto i = nodes[n].layer - min_in + 1; i < nodes[n].layer + min_out; ++i)
                if (filling[i] < filling[selected]) selected = i;
            if (filling[selected] < filling[nodes[n].layer]) {
                --filling[nodes[n].layer]; ++filling[selected]; nodes[n].layer = selected;
            }
        }
    }
    void execute(int iteration_limit, std::span<const int> previous) {
        if (order.empty()) return;
        const auto detached = order.size() >= 40 ? remove_subtrees() : std::vector<std::pair<Index, Index>>{};
        initialize(); feasible_tree();
        for (int i = 0; i < iteration_limit; ++i) {
            const auto leave = leave_edge(); if (leave == absent) break;
            exchange(leave, enter_edge(leave));
        }
        reattach_subtrees(detached); normalize_and_balance(previous);
    }
};
}

std::expected<ElkLayeringResult, std::string> elk_layer_flat_graph(
    const ElkCycleGraph& graph, std::span<const int> shortness) {
    if (!shortness.empty() && shortness.size() != graph.edges.size())
        return std::unexpected("ELK shortness priorities do not match edges");
    const auto reverse = elk_depth_first_feedback_edges(graph);
    if (!reverse) return std::unexpected(reverse.error());
    try {
        auto edges = graph.edges;
        std::vector<std::vector<Index>> ports(graph.node_count), incoming(graph.ports.size()), outgoing(graph.ports.size());
        for (Index p = 0; p < graph.ports.size(); ++p) ports[graph.ports[p].node].push_back(p);
        for (Index e = 0; e < edges.size(); ++e) {
            outgoing[edges[e].source_port].push_back(e); incoming[edges[e].target_port].push_back(e);
        }
        for (const auto e : *reverse) {
            std::erase(outgoing[edges[e].source_port], e); std::erase(incoming[edges[e].target_port], e);
            std::swap(edges[e].source_port, edges[e].target_port);
            outgoing[edges[e].source_port].push_back(e); incoming[edges[e].target_port].push_back(e);
        }
        auto owner = [&](Index p) { return graph.ports[p].node; };
        std::vector<bool> visited(graph.node_count, false);
        std::deque<std::vector<Index>> components;
        std::vector<Index> component;
        const auto visit = [&](this auto&& dfs, Index n) -> void {
            visited[n] = true; component.push_back(n);
            for (const auto p : ports[n]) {
                for (const auto* list : {&incoming[p], &outgoing[p]}) for (const auto e : *list) {
                    const auto opposite = owner(edges[e].source_port == p ? edges[e].target_port : edges[e].source_port);
                    if (!visited[opposite]) dfs(opposite);
                }
            }
        };
        for (Index n = 0; n < graph.node_count; ++n) if (!visited[n]) {
            component.clear(); visit(n);
            if (components.empty() || components.front().size() < component.size()) components.push_front(component);
            else components.push_back(component);
        }
        ElkLayeringResult result{.layers=std::vector<int>(graph.node_count), .reversed_edges=*reverse};
        std::vector<int> previous;
        for (const auto& group : components) {
            NetworkSimplex ns;
            ns.nodes.resize(group.size());
            std::vector<Index> local(graph.node_count, absent);
            for (Index i = 0; i < group.size(); ++i) { local[group[i]] = i; ns.order.push_back(i); }
            for (const auto n : group) for (const auto p : ports[n]) for (const auto e : outgoing[p]) {
                const auto source = local[n], target = local[owner(edges[e].target_port)];
                if (source == target) continue;
                const auto index = ns.edges.size();
                ns.edges.push_back({.source=source,.target=target,.weight=static_cast<double>(shortness.empty()?1:std::max(1,shortness[e]))});
                ns.nodes[source].outgoing.push_back(index); ns.nodes[target].incoming.push_back(index);
            }
            ns.execute(28 * static_cast<int>(std::floor(std::sqrt(static_cast<double>(group.size())) + 0.5)), previous);
            for (Index i = 0; i < group.size(); ++i) {
                result.layers[group[i]] = ns.nodes[i].layer;
                if (previous.size() <= static_cast<Index>(ns.nodes[i].layer)) previous.resize(ns.nodes[i].layer + 1);
                ++previous[ns.nodes[i].layer];
            }
        }
        return result;
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
