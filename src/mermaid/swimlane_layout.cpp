// Mermaid 12.2.0 swimlanes phase0-phase4, helpers and direction/lrTransform.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.swimlane_layout;
import std;
import folia.mermaid.swimlane_router;

namespace folia::mermaid {
namespace {

using Index = std::size_t;
constexpr Index none = std::numeric_limits<Index>::max();
using Indices = std::vector<Index>;
using Layers = std::vector<Indices>;

struct Node {
    FlowchartPlacedNode box;
    Index parent = none, lane = none;
    bool group = false, placed = false;
    double padding = 20;
};

struct Edge {
    std::u16string id;
    Index from, to;
};

struct Graph {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<Edge> original_edges;
    Indices label_nodes;
    Indices lanes;
    Index materialized_count = 0;
    Index output_count = 0;
    double node_gap, layer_gap;
    std::u16string_view direction;
};

struct Bounds {
    double left = std::numeric_limits<double>::infinity();
    double right = -left, top = left, bottom = -left;

    bool empty() const { return left > right; }
    double width() const { return right - left; }
    double height() const { return bottom - top; }
    void include(const FlowchartPlacedNode& node) {
        left = std::min(left, node.x - node.width / 2);
        right = std::max(right, node.x + node.width / 2);
        top = std::min(top, node.y - node.height / 2);
        bottom = std::max(bottom, node.y + node.height / 2);
    }
};

std::u16string index_string(Index index) {
    const auto text = std::to_string(index);
    return {text.begin(), text.end()};
}

// Match Chromium's en-US ASCII order and use code-unit order outside ASCII.
bool id_less(std::u16string_view a, std::u16string_view b) {
    const auto primary = [](char16_t value) {
        constexpr std::u16string_view ascii_order =
            u" _-,;:!?.'\"()[]{}@*/\\&#%`^+<=>|~$0123456789abcdefghijklmnopqrstuvwxyz";
        if (value >= u'A' && value <= u'Z') value = static_cast<char16_t>(value + 32);
        const auto weight = ascii_order.find(value);
        return weight == std::u16string_view::npos ? 128 + value : static_cast<int>(weight);
    };
    const auto size = std::min(a.size(), b.size());
    for (Index i = 0; i < size; ++i) {
        if (primary(a[i]) != primary(b[i])) return primary(a[i]) < primary(b[i]);
    }
    if (a.size() != b.size()) return a.size() < b.size();
    return a > b;
}

bool node_less(const Graph& graph, Index a, Index b) {
    return id_less(graph.nodes[a].box.id, graph.nodes[b].box.id);
}

std::expected<Graph, std::string> make_graph(const FlowchartInput& input,
    std::span<const FlowchartNodeBox> sizes, std::span<const FlowchartEdgeLabelSize> labels) {
    if (sizes.size() != input.nodes.size() || labels.size() != input.edges.size())
        return std::unexpected("Swimlane measurement count does not match the graph");
    const auto valid_dimension = [](double value) { return std::isfinite(value) && value >= 0; };
    const auto valid_box = [&](const auto& box) {
        return valid_dimension(box.width) && valid_dimension(box.height);
    };
    if (!std::ranges::all_of(sizes, valid_box) || !std::ranges::all_of(labels, valid_box) ||
        !valid_dimension(input.node_spacing) || !valid_dimension(input.rank_spacing))
        return std::unexpected("Invalid swimlane dimensions");
    Graph graph;
    graph.node_gap = input.node_spacing;
    graph.layer_gap = input.rank_spacing;
    graph.direction = input.direction;
    std::unordered_map<std::u16string, Index> ids;
    for (Index i = 0; i < input.nodes.size(); ++i) {
        if (!ids.emplace(input.nodes[i].id, i).second)
            return std::unexpected("Duplicate swimlane node id");
    }
    constexpr auto default_id = u"__swimlane_default__";
    Index default_lane = none;
    for (Index i = 0; i < input.nodes.size(); ++i) {
        const auto& data = input.nodes[i];
        auto& node = graph.nodes.emplace_back();
        node.box.id = data.id;
        node.group = data.group;
        node.padding = data.padding;
        if (!valid_dimension(node.padding)) return std::unexpected("Invalid swimlane node padding");
        if (!node.group) {
            node.box.width = sizes[i].width;
            node.box.height = sizes[i].height;
        }
        if (!data.parent.empty()) {
            const auto found = ids.find(data.parent);
            if (found == ids.end() || !input.nodes[found->second].group)
                return std::unexpected("Invalid swimlane parent");
            node.parent = found->second;
        } else if (!node.group) {
            default_lane = ids.try_emplace(default_id, input.nodes.size()).first->second;
            if (default_lane < input.nodes.size() && !input.nodes[default_lane].group)
                return std::unexpected("Swimlane implicit lane id is already a node");
            node.parent = default_lane;
        }
    }
    if (default_lane == input.nodes.size()) {
        auto& lane = graph.nodes.emplace_back();
        lane.box.id = default_id;
        lane.group = true;
    }
    graph.output_count = graph.nodes.size();
    for (Index i = 0; i < input.edges.size(); ++i) {
        const auto& edge = input.edges[i];
        const auto from = ids.find(edge.start), to = ids.find(edge.end);
        if (from == ids.end() || to == ids.end())
            return std::unexpected("Invalid swimlane edge endpoint");
        graph.original_edges.push_back({edge.id, from->second, to->second});
        graph.label_nodes.push_back(none);
        if (edge.label.empty()) {
            graph.edges.push_back(graph.original_edges.back());
            continue;
        }
        graph.label_nodes.back() = graph.nodes.size();
        const Index parent = graph.nodes[to->second].parent;
        auto& node = graph.nodes.emplace_back();
        node.box.id = u"edge-label-" + edge.start + u"-" + edge.end + u"-" + edge.id;
        node.box.width = labels[i].width;
        node.box.height = labels[i].height;
        node.parent = parent;
    }
    for (Index i = 0; i < input.edges.size(); ++i) {
        const auto label = graph.label_nodes[i];
        if (label == none) continue;
        const auto& edge = graph.original_edges[i];
        graph.edges.push_back({edge.id + u"-to-label", edge.from, label});
        graph.edges.push_back({edge.id + u"-from-label", label, edge.to});
    }
    graph.materialized_count = graph.nodes.size();
    std::set<std::tuple<std::u16string, Index, Index>> seen_edges;
    std::erase_if(graph.edges, [&](const Edge& edge) {
        return !seen_edges.emplace(edge.id, edge.from, edge.to).second;
    });
    for (Index i = 0; i < graph.nodes.size(); ++i) {
        auto& node = graph.nodes[i];
        for (Index parent = node.parent, depth = 0; parent != none;
             parent = graph.nodes[parent].parent, ++depth) {
            if (depth == graph.nodes.size()) return std::unexpected("Cyclic swimlane parents");
            node.lane = parent;
        }
        if (node.group && node.parent == none) graph.lanes.push_back(i);
    }
    std::ranges::reverse(graph.lanes);
    return graph;
}

void remove_cycles(Graph& graph) {
    Layers outgoing(graph.nodes.size());
    Indices indegree(graph.nodes.size());
    for (Index i = 0; i < graph.edges.size(); ++i) {
        const auto& edge = graph.edges[i];
        outgoing[edge.from].push_back(i);
        ++indegree[edge.to];
    }
    for (auto& edges : outgoing) {
        std::ranges::stable_sort(edges, [&](Index a, Index b) {
            const auto& first = graph.edges[a];
            const auto& second = graph.edges[b];
            return first.to == second.to ? id_less(first.id, second.id)
                                         : node_less(graph, first.to, second.to);
        });
    }
    std::vector<unsigned char> color(graph.nodes.size());
    std::vector<bool> reverse(graph.edges.size());
    const auto visit = [&](auto&& self, Index node) -> void {
        color[node] = 1;
        for (Index index : outgoing[node]) {
            const auto target = graph.edges[index].to;
            if (color[target] == 0) self(self, target);
            else if (color[target] == 1) reverse[index] = true;
        }
        color[node] = 2;
    };
    for (const bool sources : {true, false}) {
        for (Index i = 0; i < graph.nodes.size(); ++i)
            if ((indegree[i] == 0) == sources && color[i] == 0) visit(visit, i);
    }
    for (Index i = 0; i < graph.edges.size(); ++i)
        if (reverse[i]) std::swap(graph.edges[i].from, graph.edges[i].to);
}

struct Neighbors {
    Layers predecessors, successors;
    explicit Neighbors(const Graph& graph)
        : predecessors(graph.nodes.size()), successors(graph.nodes.size()) {
        for (const auto& edge : graph.edges) {
            predecessors[edge.to].push_back(edge.from);
            successors[edge.from].push_back(edge.to);
        }
    }
};

Indices topo_order(const Graph& graph, Neighbors& neighbors, bool generations) {
    const auto less = [&](Index a, Index b) { return node_less(graph, a, b); };
    Indices indegree, queue, isolated, order;
    for (Index i = 0; i < graph.nodes.size(); ++i) {
        indegree.push_back(neighbors.predecessors[i].size());
        if (indegree.back() != 0) continue;
        queue.push_back(i);
        if (neighbors.successors[i].empty() && !graph.nodes[i].group) isolated.push_back(i);
    }
    std::ranges::stable_sort(queue, less);
    Index next_isolated = 0;
    for (auto& id : queue)
        if (neighbors.successors[id].empty() && !graph.nodes[id].group) id = isolated[next_isolated++];
    for (auto& successors : neighbors.successors) std::ranges::stable_sort(successors, less);
    while (!queue.empty()) {
        if (generations) {
            Indices next;
            for (Index node : queue) {
                order.push_back(node);
                for (Index target : neighbors.successors[node])
                    if (--indegree[target] == 0) next.push_back(target);
            }
            std::ranges::stable_sort(next, less);
            queue = std::move(next);
        } else {
            const auto node = queue.front();
            queue.erase(queue.begin());
            order.push_back(node);
            for (Index target : neighbors.successors[node]) {
                if (--indegree[target] != 0) continue;
                const auto position = std::ranges::find_if(queue, [&](Index id) {
                    return graph.nodes[id].box.id >= graph.nodes[target].box.id;
                });
                queue.insert(position, target);
            }
        }
    }
    if (order.size() != graph.nodes.size()) {
        order.resize(graph.nodes.size());
        std::iota(order.begin(), order.end(), Index{0});
        std::ranges::sort(order, {}, [&](Index id) -> const auto& { return graph.nodes[id].box.id; });
    }
    return order;
}

std::vector<int> compact_ranks(const Graph& graph, const Neighbors& neighbors, const Indices& order) {
    std::vector<int> ranks(graph.nodes.size()), next_free(graph.nodes.size());
    const auto lane_of = [&](Index id) {
        const auto lane = graph.nodes[id].lane;
        return lane == none ? id : lane;
    };
    for (Index node : order) {
        if (graph.nodes[node].group) continue;
        const auto lane = lane_of(node);
        int base = 0;
        for (Index predecessor : neighbors.predecessors[node])
            base = std::max(base, ranks[predecessor] + (lane_of(predecessor) == lane));
        ranks[node] = std::max(base, next_free[lane]);
        next_free[lane] = ranks[node] + 1;
    }
    return ranks;
}

Layers make_proper_layers(Graph& graph, const Indices& order, std::vector<int> ranks,
                          bool skip_groups) {
    const auto max_rank = ranks.empty() ? 0 : *std::ranges::max_element(ranks);
    Layers layers(max_rank + 1);
    for (Index node : order)
        if (!skip_groups || !graph.nodes[node].group)
            layers[std::max(0, ranks[node])].push_back(node);
    std::ranges::stable_sort(graph.edges, [&](const Edge& a, const Edge& b) {
        if (a.id != b.id) return id_less(a.id, b.id);
        if (a.from != b.from) return node_less(graph, a.from, b.from);
        return node_less(graph, a.to, b.to);
    });
    std::vector<Edge> edges;
    Index sequence = 0;
    for (auto& edge : graph.edges) {
        if (ranks[edge.to] - ranks[edge.from] <= 1) {
            edges.push_back(std::move(edge));
            continue;
        }
        auto previous = edge.from;
        for (int rank = ranks[edge.from] + 1; rank < ranks[edge.to]; ++rank) {
            const auto dummy = graph.nodes.size();
            auto& node = graph.nodes.emplace_back();
            node.box.id = u"placeholder-" + index_string(sequence++);
            layers[rank].push_back(dummy);
            edges.push_back({{}, previous, dummy});
            previous = dummy;
        }
        edges.push_back({{}, previous, edge.to});
    }
    graph.edges = std::move(edges);
    return layers;
}

std::vector<int> gravity_ranks(const Graph& graph, const Neighbors& neighbors, const Indices& order) {
    std::vector<int> ranks(graph.nodes.size());
    for (Index node : order) {
        const auto& parents = neighbors.predecessors[node];
        for (Index parent : parents)
            ranks[node] = std::max(ranks[node], ranks[parent] +
                (parents.size() != 1 || graph.nodes[parent].lane == graph.nodes[node].lane));
    }
    // The optional crossing pass can only decrease a rank when its strict
    // predecessor lower bound is smaller than that rank. Longest-path ranks
    // equal this bound, and the compact single-predecessor case is below it.
    // Consequently there is no candidate rank to adjust at this stage.
    const auto lower_bound = [&](Index node) {
        int lower = 0;
        for (Index parent : neighbors.predecessors[node]) lower = std::max(lower, ranks[parent] + 1);
        return lower;
    };
    const auto upper_bound = [&](Index node, int bound) {
        for (Index child : neighbors.successors[node]) bound = std::min(bound, ranks[child] - 1);
        return bound;
    };
    const auto relax = [&](const auto& nodes) {
        bool changed = false;
        for (Index node : nodes) {
            const auto average = [&](const Indices& adjacent, int offset) {
                if (adjacent.empty()) return static_cast<double>(ranks[node]);
                double sum = 0;
                for (Index neighbor : adjacent) sum += ranks[neighbor] + offset;
                return sum / adjacent.size();
            };
            const double midpoint = (average(neighbors.predecessors[node], 1) +
                                     average(neighbors.successors[node], -1)) / 2;
            const int desired = static_cast<int>(std::floor(midpoint + 0.5));
            const int next = upper_bound(node, std::max(desired, lower_bound(node)));
            changed |= next != ranks[node];
            ranks[node] = next;
        }
        return changed;
    };
    for (int iteration = 0; iteration < 8; ++iteration) {
        const bool forward = relax(order);
        const bool backward = relax(order | std::views::reverse);
        if (!forward && !backward) break;
    }
    for (Index node : order) ranks[node] = std::max(ranks[node], lower_bound(node));
    for (Index node : order | std::views::reverse) ranks[node] = upper_bound(node, ranks[node]);
    return ranks;
}

void optimize_lane_order(Graph& graph) {
    if (graph.lanes.size() < 2) return;
    std::unordered_map<Index, int> source;
    for (Index i = 0; i < graph.lanes.size(); ++i)
        source.emplace(graph.lanes[i], static_cast<int>(i));
    std::map<std::pair<Index, Index>, int> counts;
    for (const auto& edge : graph.original_edges) {
        auto a = graph.nodes[edge.from].lane, b = graph.nodes[edge.to].lane;
        if (a == b || !source.contains(a) || !source.contains(b)) continue;
        if (source.at(b) < source.at(a)) std::swap(a, b);
        ++counts[{a, b}];
    }
    if (counts.empty()) return;
    struct Weight { Index from, to; int value; };
    std::vector<Weight> weights;
    for (const auto& [pair, count] : counts) weights.push_back({pair.first, pair.second, count});
    std::ranges::sort(weights, [&](const Weight& a, const Weight& b) {
        return a.from == b.from ? node_less(graph, a.to, b.to) : node_less(graph, a.from, b.from);
    });
    std::u16string signature;
    for (Index lane : graph.lanes) {
        if (!signature.empty()) signature += u'|';
        signature += graph.nodes[lane].box.id;
    }
    signature += u'#';
    for (Index i = 0; i < weights.size(); ++i) {
        if (i != 0) signature += u'|';
        const auto [from, to, value] = weights[i];
        signature += graph.nodes[from].box.id + u":" + graph.nodes[to].box.id + u":" + index_string(value);
    }
    signature += u'#';
    const auto greedy = [&](Indices order) {
        std::unordered_map<Index, int> positions;
        for (Index i = 0; i < order.size(); ++i)
            positions.emplace(order[i], static_cast<int>(i));
        const auto cost = [&] {
            std::int64_t result = 0;
            for (const auto& [from, to, value] : weights)
                result += std::int64_t{value} * std::abs(positions.at(from) - positions.at(to));
            return result;
        };
        auto best = cost();
        for (Index sweep = 0; sweep < order.size(); ++sweep) {
            bool changed = false;
            for (Index i = 1; i < order.size(); ++i) {
                auto& a = positions.at(order[i - 1]);
                auto& b = positions.at(order[i]);
                std::swap(a, b);
                const auto next = cost();
                if (next < best) {
                    std::swap(order[i - 1], order[i]);
                    best = next;
                    changed = true;
                } else {
                    std::swap(a, b);
                }
            }
            if (!changed) break;
        }
        std::int64_t distance = 0;
        for (Index lane : order) distance += std::abs(source.at(lane) - positions.at(lane));
        return std::pair{std::move(order), std::pair{best, distance}};
    };
    auto best = greedy(graph.lanes);
    for (Index restart = 0; restart < 8; ++restart) {
        std::uint32_t state = 2166136261u;
        for (char16_t value : signature + index_string(restart)) {
            state ^= value;
            state *= 16777619u;
        }
        auto shuffled = graph.lanes;
        for (Index i = shuffled.size() - 1; i > 0; --i) {
            state += 0x6d2b79f5u;
            std::uint32_t t = (state ^ (state >> 15)) * (state | 1u);
            t ^= t + (t ^ (t >> 7)) * (t | 61u);
            const auto sample = t ^ (t >> 14);
            const auto j = (std::uint64_t{sample} * (i + 1)) >> 32;
            std::swap(shuffled[i], shuffled[j]);
        }
        auto candidate = greedy(std::move(shuffled));
        if (candidate.second < best.second) best = std::move(candidate);
    }
    graph.lanes = std::move(best.first);
}

std::size_t crossing_count(const Graph& graph, const Indices& upper, const Indices& lower) {
    std::unordered_map<Index, Index> from, to;
    for (Index i = 0; i < upper.size(); ++i) from.emplace(upper[i], i);
    for (Index i = 0; i < lower.size(); ++i) to.emplace(lower[i], i);
    std::vector<std::pair<Index, Index>> pairs;
    for (const auto& edge : graph.edges) {
        const auto a = from.find(edge.from), b = to.find(edge.to);
        if (a != from.end() && b != to.end()) pairs.emplace_back(a->second, b->second);
    }
    std::ranges::sort(pairs);
    Indices tree(lower.size() + 1);
    Index inversions = 0, processed = 0;
    for (const auto& [source, destination] : pairs) {
        Index prefix = 0;
        for (Index i = destination + 1; i != 0; i &= i - 1) prefix += tree[i];
        inversions += processed++ - prefix;
        for (Index i = destination + 1; i < tree.size(); i += i & (0 - i)) ++tree[i];
    }
    return inversions;
}

void reorder_layer(const Graph& graph, const Indices& fixed, Indices& target, bool down) {
    std::unordered_map<Index, Index> positions;
    for (Index i = 0; i < fixed.size(); ++i) positions.emplace(fixed[i], i);
    std::unordered_map<Index, Indices> neighbors;
    for (Index id : target) neighbors[id] = {};
    for (const auto& edge : graph.edges) {
        const auto source = positions.find(down ? edge.from : edge.to);
        const auto destination = neighbors.find(down ? edge.to : edge.from);
        if (source != positions.end() && destination != neighbors.end())
            destination->second.push_back(source->second);
    }
    struct Score { double median, mean; };
    std::unordered_map<Index, Score> scores;
    const auto infinity = std::numeric_limits<double>::infinity();
    for (auto& [id, values] : neighbors) {
        if (values.empty()) {
            scores.emplace(id, Score{infinity, infinity});
            continue;
        }
        std::ranges::sort(values);
        const auto size = values.size();
        const auto median = (values[(size - 1) / 2] + values[size / 2]) / 2.0;
        const auto mean = std::accumulate(values.begin(), values.end(), 0.0) / size;
        scores.emplace(id, Score{median, mean});
    }
    const auto sort = [&](Indices& nodes) {
        std::ranges::stable_sort(nodes, {}, [&](Index id) { return scores.at(id).median; });
    };
    if (graph.lanes.empty()) {
        sort(target);
        return;
    }
    std::unordered_map<Index, Indices> partitions;
    for (Index id : target) partitions[graph.nodes[id].lane].push_back(id);
    target.clear();
    for (Index lane : graph.lanes) {
        auto& nodes = partitions[lane];
        sort(nodes);
        target.insert(target.end(), nodes.begin(), nodes.end());
    }
    auto& floating = partitions[none];
    sort(floating);
    for (Index id : floating) {
        const auto mean = scores.at(id).mean;
        const auto insert = std::ranges::find_if(target, [&](Index other) {
            return mean < scores.at(other).mean;
        });
        target.insert(insert, id);
    }
}

void transpose_layer(const Graph& graph, const Indices& fixed, Indices& layer,
                     const Indices* next) {
    const auto score = [&] {
        return crossing_count(graph, fixed, layer) +
            (next ? crossing_count(graph, layer, *next) : 0);
    };
    auto best = score();
    for (bool improved = true; improved;) {
        improved = false;
        for (Index i = 1; i < layer.size(); ++i) {
            if (graph.nodes[layer[i - 1]].lane != graph.nodes[layer[i]].lane) continue;
            std::swap(layer[i - 1], layer[i]);
            const auto next_score = score();
            if (next_score < best) {
                improved = true;
                best = next_score;
            } else {
                std::swap(layer[i - 1], layer[i]);
            }
        }
    }
}

void order_layers(const Graph& graph, Layers& layers) {
    for (int sweep = 0; sweep < 3; ++sweep) {
        for (Index i = 1; i < layers.size(); ++i) {
            reorder_layer(graph, layers[i - 1], layers[i], true);
            transpose_layer(graph, layers[i - 1], layers[i],
                i + 1 < layers.size() ? &layers[i + 1] : nullptr);
        }
        for (Index i = layers.size() - 1; i > 0; --i) {
            reorder_layer(graph, layers[i], layers[i - 1], false);
            transpose_layer(graph, layers[i], layers[i - 1], i > 1 ? &layers[i - 2] : nullptr);
        }
    }
}

void assign_coordinates(Graph& graph, const Layers& layers) {
    const bool horizontal = graph.direction == u"LR" || graph.direction == u"RL";
    const double lane_gap = 2 * graph.node_gap;
    std::vector<double> heights(layers.size()), max_widths(layers.size());
    std::unordered_map<Index, double> column_widths;
    const auto group_layer = [&](const Indices& layer) {
        std::unordered_map<Index, Indices> grouped;
        for (Index id : layer) grouped[graph.nodes[id].lane].push_back(id);
        return grouped;
    };
    const auto total_width = [&](const Indices& nodes) {
        double width = graph.node_gap * (nodes.size() - 1);
        for (Index id : nodes) width += graph.nodes[id].box.width;
        return width;
    };
    for (Index i = 0; i < layers.size(); ++i) {
        for (Index id : layers[i]) {
            const auto& box = graph.nodes[id].box;
            heights[i] = std::max(heights[i], box.height);
            max_widths[i] = std::max(max_widths[i], box.width);
        }
        for (const auto& [lane, nodes] : group_layer(layers[i]))
            column_widths[lane] = std::max(column_widths[lane], total_width(nodes));
    }
    Indices columns;
    if (column_widths.contains(none)) columns.push_back(none);
    for (Index lane : graph.lanes)
        if (column_widths.contains(lane)) columns.push_back(lane);
    if (columns.empty()) return;
    double width = lane_gap * (columns.size() - 1);
    for (Index lane : columns) width += column_widths.at(lane);
    std::unordered_map<Index, double> centers;
    double cursor = -width / 2;
    for (Index lane : columns) {
        const auto column_width = column_widths.at(lane);
        centers[lane] = cursor + column_width / 2;
        cursor += column_width + lane_gap;
    }
    double y = 0;
    for (Index i = 0; i < layers.size(); ++i) {
        for (const auto& [lane, nodes] : group_layer(layers[i])) {
            const auto center = centers.find(lane);
            if (center == centers.end()) continue;
            double x = center->second - total_width(nodes) / 2;
            for (Index id : nodes) {
                auto& node = graph.nodes[id];
                node.box.x = x + node.box.width / 2;
                node.box.y = y + heights[i] / 2;
                node.placed = true;
                x += node.box.width + graph.node_gap;
            }
        }
        for (Index order = 0; order < layers[i].size(); ++order) {
            auto& box = graph.nodes[layers[i][order]].box;
            box.rank = static_cast<int>(i);
            box.order = static_cast<int>(order);
        }
        double extra = 0;
        if (horizontal && i + 1 < layers.size()) {
            const double required = (max_widths[i] + max_widths[i + 1]) / 2;
            const double normal = (heights[i] + heights[i + 1]) / 2;
            extra = std::max(0.0, required - normal - graph.layer_gap);
        }
        y += heights[i] + graph.layer_gap + extra;
    }
}

void apply_bounds(FlowchartPlacedNode& box, const Bounds& bounds,
                  double horizontal_padding, double vertical_padding) {
    box.x = (bounds.left + bounds.right) / 2;
    box.y = (bounds.top + bounds.bottom) / 2;
    box.width = bounds.width() + horizontal_padding;
    box.height = bounds.height() + vertical_padding;
}

void write_back_groups(Graph& graph, std::vector<SwimlaneRoutingNode>& nodes) {
    Indices lanes_with_content;
    Bounds content;
    double maximum_padding = 36;
    for (Index i = 0; i < graph.materialized_count; ++i) {
        auto& group = graph.nodes[i];
        if (!group.group) continue;
        Bounds bounds;
        for (Index j = 0; j < graph.materialized_count; ++j) {
            const auto& child = graph.nodes[j];
            if (child.parent == i && child.placed) bounds.include(child.box);
        }
        const bool top_level = group.parent == none;
        if (top_level) maximum_padding = std::max(maximum_padding, group.padding);
        if (!bounds.empty()) {
            const auto horizontal_padding = top_level ? 2 * std::max(20.0, group.padding) : group.padding;
            apply_bounds(group.box, bounds, horizontal_padding, group.padding);
            if (top_level) {
                lanes_with_content.push_back(i);
                content.top = std::min(content.top, bounds.top);
                content.bottom = std::max(content.bottom, bounds.bottom);
            }
        }
        group.placed = true;
        nodes[i].box = group.box;
    }
    if (lanes_with_content.empty()) return;
    std::ranges::stable_sort(lanes_with_content, {}, [&](Index id) { return nodes[id].box.x; });
    if (lanes_with_content.size() > 1) {
        std::vector<double> residuals(lanes_with_content.size());
        for (Index i = 1; i < lanes_with_content.size(); ++i) {
            const auto current = nodes[lanes_with_content[i]].box.x;
            const auto previous = nodes[lanes_with_content[i - 1]].box.x;
            residuals[i] = 2 * (current - previous) - residuals[i - 1];
        }
        double lower = 0, upper = std::numeric_limits<double>::infinity();
        for (Index i = 0; i < lanes_with_content.size(); ++i) {
            const auto base_width = nodes[lanes_with_content[i]].box.width;
            if (i % 2 == 0) lower = std::max(lower, base_width - residuals[i]);
            else upper = std::min(upper, residuals[i] - base_width);
        }
        const double first_width = lower <= upper ? (lower + upper) / 2 : lower;
        for (Index i = 0; i < lanes_with_content.size(); ++i) {
            auto& width = nodes[lanes_with_content[i]].box.width;
            width = std::max(width, residuals[i] + (i % 2 == 0 ? first_width : -first_width));
        }
    }
    for (Index lane : graph.lanes) {
        auto& node = nodes[lane];
        node.box.y = (content.top + content.bottom) / 2;
        node.box.height = content.height() + 2 * maximum_padding;
        if (node.box.width > 0) {
            constexpr double title_height = 21;
            node.title_box = FlowchartPlacedNode{node.box.id, node.box.x,
                node.box.y - (node.box.height - title_height) / 2, node.box.width, title_height};
        }
    }
}

void mirror_direction(std::span<SwimlaneRoutingNode> nodes,
                      std::span<SwimlaneRoutingEdge> edges, bool horizontal) {
    double minimum = std::numeric_limits<double>::infinity(), maximum = -minimum;
    const auto axis = [&](auto& point) -> auto& { return horizontal ? point.x : point.y; };
    for (const auto& node : nodes) {
        if (node.group) continue;
        const auto position = axis(node.box);
        minimum = std::min(minimum, position);
        maximum = std::max(maximum, position);
    }
    if (minimum > maximum) return;
    const auto mirror = [&](auto& point) { axis(point) = minimum + maximum - axis(point); };
    for (auto& node : nodes) {
        mirror(node.box);
        if (node.title_box) mirror(*node.title_box);
    }
    for (auto& edge : edges)
        for (auto& point : edge.placed.points) mirror(point);
}

void transform_horizontal(const Graph& graph, std::span<SwimlaneRoutingNode> nodes,
                          std::span<SwimlaneRoutingEdge> edges) {
    double min_x = std::numeric_limits<double>::infinity(), min_y = min_x;
    double total_width = 0, total_height = 0;
    for (const auto& node : nodes) {
        if (node.group) continue;
        min_x = std::min(min_x, node.box.x);
        min_y = std::min(min_y, node.box.y);
        total_width += node.box.width;
        total_height += node.box.height;
    }
    if (!std::isfinite(min_x)) return;
    constexpr double title_band = 36;
    const double scale = total_height > 0 ? std::max(1.0, total_width / total_height) : 1;
    const auto rotate = [&](auto& point) {
        const double new_x = (point.y - min_y) * scale + title_band;
        point.y = point.x - min_x;
        point.x = new_x;
    };
    for (auto& node : nodes)
        if (!node.group) rotate(node.box);
    for (auto& edge : edges)
        for (auto& point : edge.placed.points) rotate(point);
    Indices nested;
    for (Index i = 0; i < nodes.size(); ++i)
        if (nodes[i].group && nodes[i].parent) nested.push_back(i);
    const auto depth = [&](Index index) {
        Index count = 0;
        for (auto parent = nodes[index].parent; parent; parent = nodes[*parent].parent) ++count;
        return count;
    };
    std::ranges::stable_sort(nested, std::ranges::greater{}, depth);
    for (Index group : nested) {
        Bounds bounds;
        for (const auto& child : nodes)
            if (child.parent == group) bounds.include(child.box);
        if (!bounds.empty()) apply_bounds(nodes[group].box, bounds, nodes[group].padding, nodes[group].padding);
    }
    struct LaneBounds { Index id; Bounds content; double center; };
    std::vector<LaneBounds> lanes;
    Bounds content;
    double max_padding = 0;
    for (Index lane : graph.lanes) {
        Bounds bounds;
        max_padding = std::max(max_padding, nodes[lane].padding);
        for (Index j = 0; j < nodes.size(); ++j)
            if (!nodes[j].group && graph.nodes[j].lane == lane) bounds.include(nodes[j].box);
        if (bounds.empty()) continue;
        content.left = std::min(content.left, bounds.left);
        content.right = std::max(content.right, bounds.right);
        lanes.push_back({lane, bounds, (bounds.top + bounds.bottom) / 2});
    }
    if (lanes.empty()) return;
    std::ranges::stable_sort(lanes, {}, &LaneBounds::center);
    const double horizontal_margin = std::max(max_padding, 10.0);
    const double vertical_margin = std::max(max_padding, title_band);
    const double left = content.left - horizontal_margin - title_band;
    const double right = content.right + horizontal_margin;
    for (Index i = 0; i < lanes.size(); ++i) {
        const auto& lane = lanes[i];
        const double top = i == 0 ? lane.content.top - vertical_margin :
            (lanes[i - 1].content.bottom + lane.content.top) / 2;
        const double bottom = i + 1 == lanes.size() ? lane.content.bottom + vertical_margin :
            (lane.content.bottom + lanes[i + 1].content.top) / 2;
        auto& node = nodes[lane.id];
        apply_bounds(node.box, Bounds{left, right, top, bottom}, 0, 0);
        node.title_box = FlowchartPlacedNode{node.box.id, left + title_band / 2,
            node.box.y, title_band, node.box.height};
    }
    if (graph.direction == u"RL") mirror_direction(nodes, edges, true);
}

} // namespace

std::expected<SwimlaneLayout, std::string> layout_swimlanes(const FlowchartInput& input,
    std::span<const FlowchartNodeBox> sizes, std::span<const FlowchartEdgeLabelSize> labels) {
    auto prepared = make_graph(input, sizes, labels);
    if (!prepared) return std::unexpected(prepared.error());
    auto& graph = *prepared;
    const auto* config = input.effective_config.find(u"swimlane");
    const auto* ignore = config ? config->find(u"ignoreCrossLaneEdges") : nullptr;
    const auto* automatic = config ? config->find(u"automaticLaneOrdering") : nullptr;
    const bool compact = !ignore || ignore->kind == ConfigKind::null || config_truthy(*ignore);
    if (automatic && config_truthy(*automatic)) optimize_lane_order(graph);
    remove_cycles(graph);
    Neighbors neighbors(graph);
    const auto order = topo_order(graph, neighbors, compact && graph.direction == u"LR");
    const auto ranks = compact ? compact_ranks(graph, neighbors, order) : gravity_ranks(graph, neighbors, order);
    auto layers = make_proper_layers(graph, order, ranks, compact);
    order_layers(graph, layers);
    assign_coordinates(graph, layers);

    std::vector<SwimlaneRoutingNode> routing_nodes;
    for (Index i = 0; i < graph.materialized_count; ++i) {
        const auto& node = graph.nodes[i];
        auto& routing = routing_nodes.emplace_back();
        routing.box = node.box;
        if (node.parent != none) routing.parent = node.parent;
        routing.group = node.group;
        routing.edge_label = i >= graph.output_count;
        routing.padding = node.padding;
        if (i < input.nodes.size()) {
            const auto shape = flowchart_classic_shape(input.nodes[i].shape);
            routing.diamond = shape == u"diamond" || shape == u"question";
        }
    }
    write_back_groups(graph, routing_nodes);
    std::vector<SwimlaneRoutingEdge> routing_edges;
    for (Index i = 0; i < graph.original_edges.size(); ++i) {
        const auto& source = graph.original_edges[i];
        auto& edge = routing_edges.emplace_back();
        edge.start = source.from;
        edge.end = source.to;
        edge.id = source.id;
        if (graph.label_nodes[i] != none) edge.label_node = graph.label_nodes[i];
    }
    route_swimlane_edges(routing_nodes, routing_edges, graph.direction);
    if (graph.direction == u"LR" || graph.direction == u"RL")
        transform_horizontal(graph, routing_nodes, routing_edges);
    else if (graph.direction == u"BT") mirror_direction(routing_nodes, routing_edges, false);
    finish_swimlane_routes(routing_nodes, routing_edges);
    adjust_swimlane_title_bands(routing_nodes, routing_edges, graph.direction);

    SwimlaneLayout result;
    Bounds bounds;
    for (Index i = 0; i < routing_nodes.size(); ++i) {
        auto& node = routing_nodes[i];
        bounds.include(node.box);
        if (i >= graph.output_count) continue;
        result.nodes.push_back(std::move(node.box));
        result.title_boxes.push_back(std::move(node.title_box));
    }
    for (auto& edge : routing_edges) {
        for (const auto& point : edge.placed.points)
            bounds.include(FlowchartPlacedNode{{}, point.x, point.y});
        result.edges.push_back(std::move(edge.placed));
    }
    if (!bounds.empty()) {
        result.width = bounds.width();
        result.height = bounds.height();
    }
    return result;
}

} // namespace folia::mermaid
