// SPDX-License-Identifier: EPL-2.0
// Port of ELK 0.9.3 PortListSorter, BarycenterHeuristic, port distributors and
// the flat LayerSweepCrossingMinimizer controller; see ../ELK-LICENSE.
module folia.mermaid.elk_order;
import std;

namespace folia::mermaid {
namespace {
std::expected<void, std::string> supported(const ElkLayeredGraph& g) {
    for (const auto& p : g.ports) {
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

// java.util.Random as used by the pinned GWT worker, including signed nextLong.
struct Random {
    std::uint64_t state;
    explicit Random(std::uint64_t seed) { set_seed(seed); }
    void set_seed(std::uint64_t seed) { state = (seed ^ 0x5deece66dULL) & ((1ULL << 48) - 1); }
    std::uint32_t next(int bits) {
        state = (state * 0x5deece66dULL + 11) & ((1ULL << 48) - 1);
        return static_cast<std::uint32_t>(state >> (48 - bits));
    }
    std::uint64_t next_long() {
        const auto high = static_cast<std::uint64_t>(next(32)) << 32;
        const auto low = static_cast<std::int64_t>(std::bit_cast<std::int32_t>(next(32)));
        return high + static_cast<std::uint64_t>(low);
    }
    double next_double() {
        const auto high = next(26); const auto low = next(27);
        return high * 0x1p-26 + low * 0x1p-53;
    }
    double next_float() { return next(24) * 0x1p-24; }
};

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
    Random random;
    bool node_relative;
    std::vector<double> ranks, port_barycenters;
    std::vector<std::optional<double>> barycenters;

    Ordering(ElkLayeredGraph& graph, std::int64_t seed)
        : g(graph), random(static_cast<std::uint64_t>(seed)),
          ranks(g.ports.size()), port_barycenters(g.ports.size()), barycenters(g.nodes.size()) {
        const auto sweep_seed = random.next_long();
        node_relative = random.next(1) != 0;
        random.set_seed(sweep_seed);
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
        if (node.fixed_port_positions) return;
        for (const auto p : node.ports) if (g.ports[p].side == side) {
            const auto& port = g.ports[p];
            double sum = 0;
            for (const auto e : port.outgoing) sum += ranks[g.edges[e].target_port];
            for (const auto e : port.incoming) sum -= ranks[g.edges[e].source_port];
            const auto degree = port.outgoing.size() + port.incoming.size();
            if (degree) port_barycenters[p] = sum / degree;
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
            for (const auto n : fixed) distribute(n, forward ? ElkPortSide::east : ElkPortSide::west);
        } else {
            for (const auto n : free) distribute(n, side);
        }
    }

    void sort_nodes(std::vector<std::size_t>& layer) {
        std::stable_sort(layer.begin(), layer.end(), [&](auto a, auto b) { return barycenters[a] < barycenters[b]; });
        // ForsterConstraintResolver is a no-op for normal/long-edge nodes with
        // no in-layer successors, layout units or hierarchical constraints.
    }

    void randomize_first(bool forward) {
        auto& layer = forward ? g.layers.front() : g.layers.back();
        for (const auto n : layer) barycenters[n] = random.next_double();
        sort_nodes(layer);
    }

    void minimize_layer(std::size_t current, bool forward, bool first_sweep) {
        calculate_ranks(g.layers[forward ? current - 1 : current + 1], forward);
        auto& layer = g.layers[current];
        for (const auto n : layer) {
            double sum = 0; std::size_t degree = 0;
            for (const auto p : g.nodes[n].ports) {
                const auto& edges = forward ? g.ports[p].incoming : g.ports[p].outgoing;
                for (const auto e : edges) {
                    sum += ranks[forward ? g.edges[e].source_port : g.edges[e].target_port];
                    ++degree;
                }
            }
            barycenters[n].reset();
            if (degree) {
                sum += random.next_float() * 0.07000000029802322 - 0.03500000014901161;
                barycenters[n] = sum / degree;
            }
        }
        if (!first_sweep) {
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

std::expected<void, std::string> elk_sort_flat_ports(ElkLayeredGraph& g) {
    if (const auto valid = supported(g); !valid) return valid;
    for (auto& node : g.nodes) {
        auto& ports = node.ports;
        std::stable_sort(ports.begin(), ports.end(), [&](auto a, auto b) {
            if (g.ports[a].side != g.ports[b].side) return g.ports[a].side < g.ports[b].side;
            if (!node.fixed_port_positions) return false;
            return g.ports[a].side == ElkPortSide::east ? g.ports[a].y < g.ports[b].y : g.ports[a].y > g.ports[b].y;
        });
        if (node.fixed_port_positions || ports.size() <= 1) continue;
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

std::expected<std::size_t, std::string> elk_order_flat_graph(ElkLayeredGraph& g, std::int64_t seed, int thoroughness) {
    if (const auto valid = supported(g); !valid) return std::unexpected(valid.error());
    if (thoroughness < 1) return std::unexpected("ELK ordering thoroughness must be positive");
    if (g.layers.empty() || (g.layers.size() == 1 && g.layers.front().size() <= 1)) return 0;
    Ordering ordering(g, seed);
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
}
