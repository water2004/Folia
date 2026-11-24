#include "support/folia_test.hpp"
import folia.mermaid.elk_cycle;

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
std::size_t elk_index(std::string_view text) {
    std::size_t result{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::runtime_error("Invalid ELK fixture index");
    return result;
}
suite elk_cycle = [] {
    "mermaid_elk_depth_first_feedback_matches_original_processor"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR} / "elk-cycle.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count = 0;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            const auto fields = elk_fields(line, '\t');
            expect(fatal(fields.size() == 5_u));
            ElkCycleGraph graph{.node_count = elk_index(fields[1])};
            for (const auto owner : elk_fields(fields[2], ','))
                graph.ports.push_back({elk_index(owner)});
            for (const auto edge : elk_fields(fields[3], ',')) {
                const auto endpoints = elk_fields(edge, ':');
                expect(fatal(endpoints.size() == 2_u));
                graph.edges.push_back({elk_index(endpoints[0]), elk_index(endpoints[1])});
            }
            std::vector<std::size_t> expected;
            for (const auto edge : elk_fields(fields[4], ',')) expected.push_back(elk_index(edge));
            const auto actual = elk_depth_first_feedback_edges(graph);
            expect(fatal(actual.has_value())) << fields[0];
            expect(*actual == expected) << fields[0];
            ++count;
        }
        expect(count == 7_u);
    };
    "mermaid_elk_cycle_rejects_unresolved_graph_endpoints"_test = [] {
        expect(!elk_depth_first_feedback_edges({.node_count=1, .ports={{1}}}));
        expect(!elk_depth_first_feedback_edges({.node_count=1, .ports={{0}}, .edges={{0,1}}}));
    };
};
}
