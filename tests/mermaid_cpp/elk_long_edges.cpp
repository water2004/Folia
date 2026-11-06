#include "support/folia_test.hpp"
import folia.mermaid.elk_long_edges;

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
template<class T> T elk_number(std::string_view text) {
    T result{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::runtime_error("Invalid ELK fixture number");
    return result;
}
std::string elk_list(const std::vector<std::size_t>& values) {
    std::string result;
    for (const auto value : values) { if (!result.empty()) result += ','; result += std::to_string(value); }
    return result.empty() ? "-" : result;
}
std::string elk_optional(std::optional<std::size_t> value) { return value ? std::to_string(*value) : "-1"; }
std::string elk_snapshot(const ElkLayeredGraph& graph) {
    std::string result = "N[";
    bool first = true;
    for (const auto& n : graph.nodes) {
        if (!first) result += ';'; first = false;
        result += std::format("{},{},{},{},{},{},{},{},{}:{}",static_cast<int>(n.kind),elk_optional(n.original_node),
            n.layer,n.width,n.height,n.fixed_port_positions?1:0,elk_optional(n.origin_edge),
            elk_optional(n.long_edge_source),elk_optional(n.long_edge_target),elk_list(n.ports));
    }
    result += "]P["; first = true;
    for (const auto& p : graph.ports) {
        if (!first) result += ';'; first = false;
        result += std::format("{},{},{},{}:{}:{}",p.node,static_cast<int>(p.side),p.x,p.y,elk_list(p.incoming),elk_list(p.outgoing));
    }
    result += "]E["; first = true;
    for (const auto& e : graph.edges) {
        if (!first) result += ';'; first = false;
        result += std::format("{},{},{},{},{}",e.original_edge,e.source_port,e.target_port,e.thickness,e.reversed?1:0);
    }
    result += "]L["; first = true;
    for (const auto& layer : graph.layers) {
        if (!first) result += ';'; first = false;
        result += elk_list(layer);
    }
    return result + ']';
}
suite elk_long_edges = [] {
    "mermaid_elk_long_edges_match_original_processor_graph"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR} / "elk-long-edges.tsv"};
        expect(fatal(input.is_open()));
        std::string line; std::size_t count = 0;
        while (std::getline(input,line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            const auto fields = elk_fields(line,'\t'); expect(fatal(fields.size() == 6_u));
            ElkCycleGraph source{.node_count=elk_number<std::size_t>(fields[1])};
            for (const auto owner : elk_fields(fields[2],',')) source.ports.push_back({elk_number<std::size_t>(owner)});
            for (const auto edge : elk_fields(fields[3],',')) {
                const auto ports = elk_fields(edge,':'); expect(fatal(ports.size() == 2_u));
                source.edges.push_back({elk_number<std::size_t>(ports[0]),elk_number<std::size_t>(ports[1])});
            }
            const auto layering = elk_layer_flat_graph(source); expect(fatal(layering.has_value())) << fields[0];
            auto graph = make_elk_layered_graph(source,*layering); expect(fatal(graph.has_value())) << fields[0];
            for (std::size_t i = 0; i < graph->nodes.size(); ++i) { graph->nodes[i].width=80+i*10; graph->nodes[i].height=32+i; }
            const auto thickness = elk_fields(fields[4],','); expect(fatal(thickness.size() == graph->edges.size()));
            for (std::size_t i = 0; i < thickness.size(); ++i) graph->edges[i].thickness=elk_number<double>(thickness[i]);
            elk_split_long_edges(*graph);
            expect(elk_snapshot(*graph) == fields[5]) << fields[0];
            ++count;
        }
        expect(count == 5_u);
    };
    "mermaid_elk_graph_requires_matching_layer_order"_test = [] {
        ElkCycleGraph source{.node_count=1};
        expect(!make_elk_layered_graph(source,{}));
        expect(!make_elk_layered_graph(source,{.layers={0}}));
    };
};
}
