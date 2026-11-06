#include "support/folia_test.hpp"
import folia.mermaid.elk_order;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> fields(std::string_view text, char delimiter) {
    std::vector<std::string_view> result;
    if (text == "-") return result;
    for (;;) {
        const auto end = text.find(delimiter);
        result.push_back(text.substr(0, end));
        if (end == text.npos) break;
        text.remove_prefix(end + 1);
    }
    return result;
}
std::size_t number(std::string_view text) {
    std::size_t value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{}) throw std::runtime_error("Invalid ELK order fixture number");
    return value;
}
std::string list(const std::vector<std::size_t>& values) {
    std::string result;
    for (const auto n : values) { if (!result.empty()) result += ','; result += std::to_string(n); }
    return result.empty() ? "-" : result;
}
std::string snapshot(const ElkLayeredGraph& graph) {
    std::string result;
    for (const auto& layer : graph.layers) { if (!result.empty()) result += ';'; result += list(layer); }
    result += '|';
    for (std::size_t n = 0; n < graph.nodes.size(); ++n) {
        if (n) result += ';'; result += list(graph.nodes[n].ports);
    }
    return result;
}
ElkLayeredGraph make_graph(const std::vector<std::string_view>& row) {
    const auto assignments = fields(row[1], ',');
    ElkCycleGraph source{.node_count=assignments.size()};
    for (const auto pair : fields(row[2], ',')) {
        const auto ends = fields(pair, ':');
        const auto p = source.ports.size();
        source.ports.push_back({number(ends[0])}); source.ports.push_back({number(ends[1])});
        source.edges.push_back({p,p+1});
    }
    ElkLayeringResult layering;
    for (std::size_t n = 0; n < assignments.size(); ++n) {
        const auto l = number(assignments[n]);
        layering.layers.push_back(static_cast<int>(l));
        if (layering.layer_nodes.size() <= l) layering.layer_nodes.resize(l+1);
        layering.layer_nodes[l].push_back(n);
    }
    auto result = make_elk_layered_graph(source, layering);
    if (!result) throw std::runtime_error(result.error());
    auto graph = std::move(*result);
    for (std::size_t e = 0; e < source.edges.size(); ++e) {
        graph.ports[e*2].side=ElkPortSide::east; graph.ports[e*2+1].side=ElkPortSide::west;
        graph.ports[e*2].y=(e*7)%23; graph.ports[e*2+1].y=(e*11)%19;
    }
    for (const auto n : fields(row[3], ',')) graph.nodes[number(n)].fixed_port_positions=true;
    elk_split_long_edges(graph);
    return graph;
}
suite elk_order = [] {
    "mermaid_elk_order_matches_original_seeded_processor"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR} / "elk-order.tsv"};
        expect(fatal(input.is_open()));
        std::string line; std::size_t count = 0;
        while (std::getline(input,line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            const auto row = fields(line,'\t'); expect(fatal(row.size() == 8_u));
            auto graph = make_graph(row);
            expect(fatal(elk_sort_flat_ports(graph).has_value())) << row[0];
            expect(snapshot(graph) == row[5]) << row[0] << snapshot(graph);
            auto repeated = graph;
            const auto crossings = elk_order_flat_graph(graph,number(row[4]));
            expect(fatal(crossings.has_value())) << row[0];
            expect(snapshot(graph) == row[6]) << row[0] << snapshot(graph);
            expect(*crossings == number(row[7])) << row[0];
            const auto again = elk_order_flat_graph(repeated,number(row[4]));
            expect(again == crossings && snapshot(repeated) == snapshot(graph)) << row[0];
            ++count;
        }
        expect(count == 10_u);
    };
    "mermaid_elk_order_rejects_unported_preprocessing_without_mutation"_test = [] {
        ElkLayeredGraph graph;
        graph.nodes.resize(2); graph.nodes[0].ports={0}; graph.nodes[1].ports={1};
        graph.layers={{0,1}};
        graph.ports={{.node=0,.side=ElkPortSide::east,.outgoing={0}},
                     {.node=1,.side=ElkPortSide::west,.incoming={0}}};
        graph.edges={{.source_port=0,.target_port=1}};
        const auto before=snapshot(graph);
        expect(!elk_order_flat_graph(graph)); // In-layer edge needs its processor.
        graph.nodes[1].layer=1; graph.layers={{0},{1}};
        graph.ports[0].side=ElkPortSide::north;
        expect(!elk_sort_flat_ports(graph));
        graph.ports[0].side=ElkPortSide::east;
        graph.ports[0].outgoing.push_back(0);
        expect(!elk_order_flat_graph(graph));
        graph.ports[0].outgoing.pop_back();
        expect(!elk_order_flat_graph(graph,1,0));
        graph.nodes[1].layer=0; graph.layers={{0,1}};
        expect(snapshot(graph) == before);
    };
};
}
