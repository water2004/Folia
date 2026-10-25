#include "support/folia_test.hpp"
import folia.mermaid.elk_layering;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::vector<std::string_view> elk_fields(std::string_view text, char delimiter) {
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
int elk_int(std::string_view text) {
    int result{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::runtime_error("Invalid ELK fixture integer");
    return result;
}
suite elk_layering = [] {
    "mermaid_elk_flat_layering_matches_original_processors"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR} / "elk-layering.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count = 0;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            const auto fields = elk_fields(line, '\t');
            expect(fatal(fields.size() == 7_u));
            ElkCycleGraph graph{.node_count=static_cast<std::size_t>(elk_int(fields[1]))};
            for (const auto owner : elk_fields(fields[2], ',')) graph.ports.push_back({static_cast<std::size_t>(elk_int(owner))});
            for (const auto edge : elk_fields(fields[3], ',')) {
                const auto endpoints = elk_fields(edge, ':');
                expect(fatal(endpoints.size() == 2_u));
                graph.edges.push_back({static_cast<std::size_t>(elk_int(endpoints[0])),static_cast<std::size_t>(elk_int(endpoints[1]))});
            }
            std::vector<int> priorities, layers;
            std::vector<std::size_t> reversed;
            for (const auto value : elk_fields(fields[4], ',')) priorities.push_back(elk_int(value));
            for (const auto value : elk_fields(fields[5], ',')) reversed.push_back(static_cast<std::size_t>(elk_int(value)));
            for (const auto value : elk_fields(fields[6], ',')) layers.push_back(elk_int(value));
            const auto actual = elk_layer_flat_graph(graph, priorities);
            expect(fatal(actual.has_value())) << fields[0] << (actual ? "" : actual.error());
            expect(actual->layers == layers) << fields[0];
            expect(actual->reversed_edges == reversed) << fields[0];
            ++count;
        }
        expect(count == 11_u);
    };
    "mermaid_elk_flat_layering_validates_graph_input"_test = [] {
        expect(!elk_layer_flat_graph({.node_count=1,.ports={{0}},.edges={{0,1}}}));
        const std::vector<int> priorities{1};
        expect(!elk_layer_flat_graph({},priorities));
    };
};
}
