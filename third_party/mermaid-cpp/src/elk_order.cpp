// SPDX-License-Identifier: EPL-2.0
// Port of ELK 0.9.3 PortListSorter, BarycenterHeuristic, port distributors and
// the flat LayerSweepCrossingMinimizer controller; see ../ELK-LICENSE.
module folia.mermaid.elk_order;
import std;

namespace folia::mermaid {
namespace {
std::expected<void, std::string> supported(const ElkLayeredGraph& g,const ElkNorthSouthPreparation* north_south=nullptr) {
    for (const auto& p : g.ports) {
        if (north_south&&(p.side==ElkPortSide::north||p.side==ElkPortSide::south)&&p.incoming.empty()&&p.outgoing.empty())continue;
        if (p.side != ElkPortSide::east && p.side != ElkPortSide::west)
            return std::unexpected("ELK flat ordering requires east/west port preprocessing");
        if (p.incoming.size() + p.outgoing.size() > 1)
            return std::unexpected("ELK shared-port hyperedge crossing count is not implemented");
    }
    for (const auto& e : g.edges) {
        const auto& s = g.ports[e.source_port]; const auto& t = g.ports[e.target_port];
        if (s.side != ElkPortSide::east || t.side != ElkPortSide::west ||
            g.nodes[t.node].layer != g.nodes[s.node].layer + 1)
            return std::unexpected("ELK flat ordering requires adjacent forward edges");
    }
    return {};
}

struct OrderCopy {
    std::vector<std::vector<std::size_t>> layers, ports;
    explicit OrderCopy(const ElkLayeredGraph& g) : layers(g.layers) {
        for (const auto& n : g.nodes) ports.push_back(n.ports);
    }
    void restore(ElkLayeredGraph& g) const {
        g.layers = layers;
        for (std::size_t n = 0; n < ports.size(); ++n) g.nodes[n].ports = ports[n];
    }
};

struct Ordering {
    ElkLayeredGraph& g;
    ElkRandom& random;
    bool node_relative;
    std::vector<double> ranks, port_barycenters;
    std::vector<std::optional<double>> barycenters;
    std::vector<bool> nested_nodes;
    const ElkNorthSouthPreparation* north_south;
    std::optional<ElkOrderConstraints> constraints;

    Ordering(ElkLayeredGraph& graph, ElkRandom& graph_random, bool flat_controller = true,const ElkNorthSouthPreparation* prepared = nullptr)
        : g(graph), random(graph_random),
          ranks(g.ports.size()), port_barycenters(g.ports.size()), barycenters(g.nodes.size()), nested_nodes(g.nodes.size()),north_south(prepared) {
        if(prepared)constraints=elk_make_order_constraints(g,*prepared);
        const auto sweep_seed = flat_controller ? random.next_long() : 0;
        node_relative = random.next(1) != 0;
        if (flat_controller) random.set_seed(sweep_seed);
    }

    void calculate_ranks(const std::vector<std::size_t>& layer, bool output) {
        double sum = 0;
        for (const auto n : layer) {
            const auto& ports = g.nodes[n].ports;
            const auto has_type = [&](std::size_t p) {
                return output ? !g.ports[p].outgoing.empty() : !g.ports[p].incoming.empty();
            };
            const auto count = std::ranges::count_if(ports, has_type);
            const auto increment = node_relative ? 1.0 / (count + 1) : 1.0;
            double pos = output ? sum + increment : sum + (node_relative ? 1.0 - increment : count);
            for (const auto p : ports) if (has_type(p)) {
                ranks[p] = pos;
                pos += output ? increment : -increment;
            }
            sum += node_relative ? 1 : count;
        }
    }

    void distribute(std::size_t n, ElkPortSide side) {
        auto& node = g.nodes[n];
        if (node.fixed_port_positions || node.fixed_port_order) return;
        for (const auto p : node.ports) if (g.ports[p].side == side) {
            const auto& port = g.ports[p];
            double sum = 0;
            for (const auto e : port.outgoing) sum += ranks[g.edges[e].target_port];
            for (const auto e : port.incoming) sum -= ranks[g.edges[e].source_port];
            const auto degree = port.outgoing.size() + port.incoming.size();
            if (degree) port_barycenters[p] = sum / degree;
        }
        if(north_south)for(const auto& dummy:north_south->dummies)if(dummy.origin_node==n) {
            const auto& layer=g.layers[g.nodes[n].layer];
            const auto position=std::ranges::find(layer,dummy.node)-layer.begin();
            const bool input=!g.ports[dummy.proxy_port].incoming.empty();
            port_barycenters[dummy.origin_port]=input?(dummy.side==ElkPortSide::north?-double(position):2.0*layer.size()+1-position):position+1;
        }
        std::stable_sort(node.ports.begin(), node.ports.end(), [&](auto a, auto b) {
            if (g.ports[a].side != g.ports[b].side) return g.ports[a].side < g.ports[b].side;
            const auto x = port_barycenters[a], y = port_barycenters[b];
            if (x == 0 && y == 0) return false;
            if (x == 0) return true;
            if (y == 0) return false;
            return x < y;
        });
    }

    void distribute_layer(std::size_t current, bool forward) {
        const auto side = forward ? ElkPortSide::west : ElkPortSide::east;
        auto& free = g.layers[current];
        if (forward ? current != 0 : current + 1 != g.layers.size()) {
            auto& fixed = g.layers[forward ? current - 1 : current + 1];
            calculate_ranks(fixed, forward);
            for (const auto n : free) distribute(n, side);
            calculate_ranks(free, !forward);
            for (const auto n : fixed) if (!nested_nodes[n]) distribute(n, forward ? ElkPortSide::east : ElkPortSide::west);
        } else {
            for (const auto n : free) distribute(n, side);
        }
    }

    void sort_nodes(std::vector<std::size_t>& layer) {
        std::stable_sort(layer.begin(), layer.end(), [&](auto a, auto b) { return barycenters[a] < barycenters[b]; });
        if(constraints)elk_resolve_order_constraints(*constraints,layer,barycenters);
    }

    void randomize_first(bool forward) {
        auto& layer = forward ? g.layers.front() : g.layers.back();
        for (const auto n : layer) barycenters[n] = random.next_double();
        sort_nodes(layer);
    }

    void minimize_layer(std::size_t current, bool forward, bool first_sweep) {
        calculate_ranks(g.layers[forward ? current - 1 : current + 1], forward);
        auto& layer = g.layers[current];
        std::vector<bool> visited(g.nodes.size());std::vector<double> weights(g.nodes.size());std::vector<std::size_t> degrees(g.nodes.size());
        const auto calculate=[&](auto&& self,std::size_t n)->void {
            if(visited[n])return;visited[n]=true;barycenters[n].reset();
            for(const auto p:g.nodes[n].ports)for(const auto e:forward?g.ports[p].incoming:g.ports[p].outgoing) {
                const auto fixed=forward?g.edges[e].source_port:g.edges[e].target_port;
                weights[n]+=ranks[fixed];++degrees[n];
            }
            if(north_south&&n<north_south->barycenter_associates.size())for(const auto associate:north_south->barycenter_associates[n]) {
                if(g.nodes[associate].layer!=g.nodes[n].layer)continue;
                self(self,associate);weights[n]+=weights[associate];degrees[n]+=degrees[associate];
            }
            if(degrees[n]){weights[n]+=random.next_float()*0.07000000029802322-0.03500000014901161;barycenters[n]=weights[n]/degrees[n];}
        };
        for(const auto n:layer)calculate(calculate,n);
        if (!first_sweep || g.nodes[layer.front()].kind==ElkLayerNodeKind::external_port) {
            double last = -1;
            for (std::size_t i = 0; i < layer.size(); ++i) {
                auto& value = barycenters[layer[i]];
                if (!value) {
                    double next = last + 1;
                    for (std::size_t j = i + 1; j < layer.size(); ++j) if (barycenters[layer[j]]) {
                        next = *barycenters[layer[j]]; break;
                    }
                    value = (last + next) / 2;
                }
                last = *value;
            }
        } else {
            double maximum = 0;
            for (const auto n : layer) if (barycenters[n]) maximum = std::max(maximum, *barycenters[n]);
            maximum += 2;
            for (const auto n : layer) if (!barycenters[n]) barycenters[n] = random.next_float() * maximum - 1;
        }
        sort_nodes(layer);
    }

    void sweep(bool forward, bool first) {
        distribute_layer(forward ? 0 : g.layers.size() - 1, forward);
        for (std::size_t step = 1; step < g.layers.size(); ++step) {
            const auto current = forward ? step : g.layers.size() - step - 1;
            minimize_layer(current, forward, first);
            distribute_layer(current, forward);
        }
    }

    std::size_t crossings() const {
        // CrossingsCounter's binary indexed tree for the adjacent E/W case:
        // clockwise port lists mean west ports run bottom-to-top on each node.
        std::vector<std::size_t> positions(g.ports.size());
        std::size_t total = 0;
        for (std::size_t l = 0; l + 1 < g.layers.size(); ++l) {
            std::size_t size = 0;
            for (const auto n : g.layers[l + 1]) for (const auto p : g.nodes[n].ports | std::views::reverse)
                if (g.ports[p].side == ElkPortSide::west) positions[p] = size++;
            std::vector<std::size_t> tree(size + 1);
            std::size_t seen = 0;
            for (const auto n : g.layers[l]) for (const auto p : g.nodes[n].ports) {
                for (const auto e : g.ports[p].outgoing) {
                    auto index = positions[g.edges[e].target_port] + 1;
                    std::size_t before = 0;
                    for (auto i = index; i; i -= i & -i) before += tree[i];
                    total += seen - before;
                    for (auto i = index; i < tree.size(); i += i & -i) ++tree[i];
                    ++seen;
                }
            }
        }
        if(north_south)for(const auto& layer:g.layers)total+=elk_count_north_south_crossings(g,*north_south,layer);
        return total;
    }

    std::pair<std::size_t, OrderCopy> attempt() {
        bool forward = random.next(1) != 0;
        randomize_first(forward);
        sweep(forward, true);
        auto count = crossings();
        for (;;) {
            OrderCopy best(g);
            if (!count) return {count, std::move(best)};
            forward = !forward;
            sweep(forward, false);
            const auto next = crossings();
            if (next >= count) return {count, std::move(best)};
            count = next;
        }
    }
};
}

static std::expected<void, std::string> sort_ports(ElkLayeredGraph& g,const ElkNorthSouthPreparation* prepared) {
    if (const auto valid = supported(g,prepared); !valid) return valid;
    for (auto& node : g.nodes) {
        auto& ports = node.ports;
        std::stable_sort(ports.begin(), ports.end(), [&](auto a, auto b) {
            if (g.ports[a].side != g.ports[b].side) return g.ports[a].side < g.ports[b].side;
            if (!node.fixed_port_positions) return false;
            const auto side=g.ports[a].side;
            if(side==ElkPortSide::north)return g.ports[a].x<g.ports[b].x;
            if(side==ElkPortSide::south)return g.ports[a].x>g.ports[b].x;
            return side == ElkPortSide::east ? g.ports[a].y < g.ports[b].y : g.ports[a].y > g.ports[b].y;
        });
        if (node.fixed_port_positions || node.fixed_port_order || ports.size() <= 1) continue;
        // Preserve the pinned PortListSorter's range and reversal behavior,
        // including its exclusive high index and short-range early return.
        std::size_t low = 0;
        while (low + 1 < ports.size() && g.ports[ports[low]].side < ElkPortSide::west) ++low;
        auto high = low;
        while (high + 1 < ports.size()) ++high;
        if (high > low + 2) std::reverse(ports.begin() + low, ports.begin() + high);
        // The original default portSortingStrategy is INPUT_ORDER.
    }
    return {};
}

std::expected<void,std::string> elk_sort_flat_ports(ElkLayeredGraph& g){return sort_ports(g,nullptr);}
std::expected<void,std::string> elk_sort_north_south_ports(ElkLayeredGraph& g,const ElkNorthSouthPreparation& prepared){return sort_ports(g,&prepared);}

std::expected<std::size_t, std::string> elk_order_flat_graph(ElkLayeredGraph& g, std::int64_t seed, int thoroughness) {
    ElkRandom random(static_cast<std::uint64_t>(seed));
    return elk_order_flat_graph(g,random,thoroughness);
}

static std::expected<std::size_t,std::string> order_graph(ElkLayeredGraph& g,ElkRandom& random,int thoroughness,const ElkNorthSouthPreparation* prepared) {
    if (const auto valid = supported(g,prepared); !valid) return std::unexpected(valid.error());
    if (thoroughness < 1) return std::unexpected("ELK ordering thoroughness must be positive");
    if (g.layers.empty() || (g.layers.size() == 1 && g.layers.front().size() <= 1)) return 0;
    Ordering ordering(g, random,true,prepared);
    std::size_t best_count = std::numeric_limits<std::size_t>::max();
    std::optional<OrderCopy> best;
    for (int i = 0; i < thoroughness; ++i) {
        auto [count, order] = ordering.attempt();
        if (count < best_count) {
            best_count = count; best = std::move(order);
            if (!count) break;
        }
    }
    best->restore(g);
    return best_count;
}

std::expected<std::size_t,std::string> elk_order_flat_graph(ElkLayeredGraph& g,ElkRandom& random,int thoroughness){return order_graph(g,random,thoroughness,nullptr);}
std::expected<std::size_t,std::string> elk_order_north_south_graph(ElkLayeredGraph& g,const ElkNorthSouthPreparation& prepared,ElkRandom& random,int thoroughness){return order_graph(g,random,thoroughness,&prepared);}

std::expected<std::size_t, std::string> elk_order_hierarchy(
    std::span<ElkHierarchyOrderGraph> graphs, int thoroughness) {
    if (graphs.empty()) return 0;
    for (const auto& entry : graphs) if (const auto valid = supported(*entry.graph,entry.north_south); !valid)
        return std::unexpected(valid.error());
    if (thoroughness < 1) return std::unexpected("ELK ordering thoroughness must be positive");
    if (graphs[0].graph->layers.empty()) return 0;
    struct State {
        std::unique_ptr<Ordering> order;
        std::vector<std::size_t> children;
        bool bottom_up = true, changed = false;
        std::optional<OrderCopy> current_best, best;
    };
    std::vector<State> states(graphs.size());
    auto& root_random = *graphs[0].random;
    const auto sweep_seed = root_random.next_long();
    std::vector<std::size_t> breadth{0}, bottom_up;
    for (std::size_t i = 0; i < breadth.size(); ++i) {
        const auto index = breadth[i];
        auto& entry = graphs[index]; auto& state = states[index];
        state.order = std::make_unique<Ordering>(*entry.graph, *entry.random, false,entry.north_south);
        // GraphInfoHolder discovers children in current layer/node order.
        for (const auto& layer : entry.graph->layers) for (const auto n : layer)
            for (std::size_t c = 1; c < graphs.size(); ++c)
                if (graphs[c].parent_graph == index && graphs[c].parent_node == n) {
                    state.children.push_back(c); breadth.push_back(c); state.order->nested_nodes[n] = true;
                }
        if (entry.parent_graph) {
            const auto& parent = *graphs[*entry.parent_graph].graph;
            state.bottom_up = elk_hierarchy_uses_bottom_up(*entry.graph, parent,
                entry.parent_node, entry.ports, parent.nodes[entry.parent_node].fixed_port_order);
        }
        if (state.bottom_up) bottom_up.push_back(index);
    }
    const auto count = [&](auto&& self, std::size_t index) -> std::size_t {
        auto total = states[index].order->crossings();
        for (const auto c : states[index].children) if (!states[c].bottom_up) total += self(self,c);
        return total;
    };
    const auto sweep = [&](auto&& self, std::size_t index, bool forward, bool first) -> void {
        auto& state = states[index]; auto& g = *graphs[index].graph;
        for (std::size_t step = 0; step < g.layers.size(); ++step) {
            const auto layer = forward ? step : g.layers.size() - 1 - step;
            if (step) state.order->minimize_layer(layer,forward,first);
            state.order->distribute_layer(layer,forward);
            for (const auto n : g.layers[layer]) for (const auto c : state.children)
                if (graphs[c].parent_node == n && !states[c].bottom_up) {
                    auto& child = graphs[c];
                    if (!elk_hierarchy_enter(*child.graph,g,n,child.ports,forward))
                        states[c].order->randomize_first(forward);
                    self(self,c,forward,first);
                    elk_hierarchy_leave(*child.graph,g,n,child.ports,forward);
                }
        }
        state.changed = true;
    };
    for (const auto index : bottom_up | std::views::reverse) {
        auto& entry = graphs[index]; auto& state = states[index];
        if (entry.graph->layers.empty()) continue;
        root_random.set_seed(sweep_seed);
        for (auto& s : states) s.changed = false;
        auto best_count = std::numeric_limits<std::size_t>::max();
        for (int attempt = 0; attempt < thoroughness; ++attempt) {
            bool forward = root_random.next(1) != 0;
            state.order->randomize_first(forward);
            sweep(sweep,index,forward,true);
            auto crossings = count(count,index);
            for (;;) {
                for (std::size_t s = 0; s < states.size(); ++s) if (states[s].changed)
                    states[s].current_best.emplace(*graphs[s].graph);
                if (!crossings) break;
                forward = !forward;
                sweep(sweep,index,forward,false);
                const auto next = count(count,index);
                if (next >= crossings) break;
                crossings = next;
            }
            if (crossings < best_count) {
                best_count = crossings;
                for (auto& s : states) if (s.changed) s.best = s.current_best;
                if (!crossings) break;
            }
        }
        if (entry.parent_graph && !entry.ports.empty()) {
            // Transfer best child layers, not the final unsuccessful sweep.
            auto best_graph = *entry.graph; state.best->restore(best_graph);
            auto& parent = *graphs[*entry.parent_graph].graph;
            elk_hierarchy_leave(best_graph,parent,entry.parent_node,entry.ports,true);
            elk_hierarchy_leave(best_graph,parent,entry.parent_node,entry.ports,false);
            parent.nodes[entry.parent_node].fixed_port_order = true;
        }
    }
    std::size_t total = 0;
    for (std::size_t i = 0; i < states.size(); ++i) {
        if (states[i].best) states[i].best->restore(*graphs[i].graph);
        total += states[i].order->crossings();
    }
    return total;
}
}
