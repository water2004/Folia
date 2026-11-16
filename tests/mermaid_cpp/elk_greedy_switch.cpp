#include "support/folia_test.hpp"
import folia.mermaid.elk_greedy_switch;
import folia.mermaid.directives;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

const ConfigValue& field(const ConfigValue& object, std::u16string_view key) {
    const auto* value = object.find(key);
    if (!value) throw std::runtime_error("Missing ELK greedy-switch fixture field");
    return *value;
}
double decimal(const ConfigValue& value) {
    if (value.kind != ConfigKind::number || !std::isfinite(value.number))
        throw std::runtime_error("Invalid ELK greedy-switch fixture number");
    return value.number;
}
std::size_t index(const ConfigValue& value) {
    const auto result = decimal(value);
    if (result < 0 || result != std::floor(result))
        throw std::runtime_error("Invalid ELK greedy-switch fixture index");
    return static_cast<std::size_t>(result);
}
std::vector<std::size_t> indices(const ConfigValue& value) {
    if (value.kind != ConfigKind::sequence)
        throw std::runtime_error("Invalid ELK greedy-switch fixture array");
    std::vector<std::size_t> result;
    for (const auto& item : value.sequence) result.push_back(index(item));
    return result;
}
ConfigValue decode_json(std::string_view hex) {
    if (hex.size() % 4) throw std::runtime_error("Invalid ELK greedy-switch fixture UTF-16");
    std::u16string text;
    for (std::size_t offset = 0; offset < hex.size(); offset += 4) {
        unsigned value{};
        const auto parsed = std::from_chars(hex.data() + offset, hex.data() + offset + 4, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != hex.data() + offset + 4)
            throw std::runtime_error("Invalid ELK greedy-switch fixture hex digit");
        text += static_cast<char16_t>(value);
    }
    auto result = parse_json_value(text);
    if (!result) throw std::runtime_error(result.error());
    return std::move(*result);
}
ElkLayerNodeKind kind(std::u16string_view value) {
    if (value == u"normal") return ElkLayerNodeKind::normal;
    if (value == u"long_edge") return ElkLayerNodeKind::long_edge;
    if (value == u"label") return ElkLayerNodeKind::label;
    if (value == u"external_port") return ElkLayerNodeKind::external_port;
    if (value == u"north_south") return ElkLayerNodeKind::north_south;
    throw std::runtime_error("Invalid ELK greedy-switch node kind");
}
ElkPortSide side(std::u16string_view value) {
    if (value == u"east") return ElkPortSide::east;
    if (value == u"west") return ElkPortSide::west;
    if (value == u"north") return ElkPortSide::north;
    if (value == u"south") return ElkPortSide::south;
    if (value == u"undefined") return ElkPortSide::undefined;
    throw std::runtime_error("Invalid ELK greedy-switch port side");
}
ElkLayeredGraph read_graph(const ConfigValue& input) {
    ElkLayeredGraph graph;
    for (const auto& item : field(input, u"nodes").sequence) {
        ElkLayerNode node;
        node.kind = kind(field(item, u"kind").string);
        node.layer = index(field(item, u"layer"));
        node.width = decimal(field(item, u"width"));
        node.height = decimal(field(item, u"height"));
        node.fixed_port_positions = field(item, u"fixedPositions").boolean;
        node.fixed_port_order = field(item, u"fixedOrder").boolean;
        const auto& source = field(item, u"longSource");
        const auto& target = field(item, u"longTarget");
        if (source.kind == ConfigKind::number) node.long_edge_source = index(source);
        if (target.kind == ConfigKind::number) node.long_edge_target = index(target);
        graph.nodes.push_back(std::move(node));
    }
    for (const auto& layer : field(input, u"layers").sequence) graph.layers.push_back(indices(layer));
    const auto& orders = field(input, u"ports").sequence;
    if (orders.size() != graph.nodes.size()) throw std::runtime_error("Invalid ELK greedy-switch port orders");
    for (std::size_t n = 0; n < orders.size(); ++n) graph.nodes[n].ports = indices(orders[n]);
    for (const auto& item : field(input, u"portData").sequence) {
        graph.ports.push_back({.node=index(field(item, u"node")), .side=side(field(item, u"side").string),
            .x=decimal(field(item, u"x")), .y=decimal(field(item, u"y")),
            .incoming=indices(field(item, u"incoming")), .outgoing=indices(field(item, u"outgoing"))});
    }
    for (const auto& item : field(input, u"edges").sequence)
        graph.edges.push_back({.original_edge=graph.edges.size(), .source_port=index(field(item, u"source")),
            .target_port=index(field(item, u"target"))});
    return graph;
}
void check_order(const std::vector<std::size_t>& actual, const ConfigValue& expected,
    std::string_view name, std::string_view member) {
    const auto values = indices(expected);
    expect(actual.size() == values.size()) << name << member << "size";
    for (std::size_t i = 0; i < std::min(actual.size(), values.size()); ++i)
        expect(actual[i] == values[i]) << name << member << i << actual[i] << values[i];
}

suite elk_greedy_switch_regressions = [] {
    "mermaid_elk_greedy_switch_matches_original_stage_and_rng"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream file{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR}
            / "elk-greedy-switch.tsv"};
        expect(fatal(file.is_open()));
        std::string line;
        std::size_t cases = 0, improvements = 0, port_changes = 0;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            const auto first = line.find('\t'), second = line.find('\t', first + 1);
            expect(fatal(first != line.npos && second != line.npos));
            const auto name = std::string_view{line}.substr(0, first);
            const auto input = decode_json(std::string_view{line}.substr(first + 1, second - first - 1));
            const auto expected = decode_json(std::string_view{line}.substr(second + 1));
            auto graph = read_graph(input);
            ElkRandom random;
            random.state = index(field(input, u"randomState"));
            const auto result = elk_greedy_switch(graph, random);
            ++cases;
            expect(result.has_value()) << name << (result ? "" : result.error());
            if (!result) continue;
            expect(*result == index(field(expected, u"crossings"))) << name << "crossings";
            expect(random.state == index(field(expected, u"randomState"))) << name << "random state";
            const auto& layers = field(expected, u"layers").sequence;
            const auto& ports = field(expected, u"ports").sequence;
            expect(fatal(layers.size() == graph.layers.size())) << name;
            expect(fatal(ports.size() == graph.nodes.size())) << name;
            for (std::size_t l = 0; l < layers.size(); ++l)
                check_order(graph.layers[l], layers[l], name, "layer " + std::to_string(l));
            bool moved_ports = false;
            for (std::size_t n = 0; n < ports.size(); ++n) {
                check_order(graph.nodes[n].ports, ports[n], name, "node ports " + std::to_string(n));
                moved_ports |= graph.nodes[n].ports != indices(field(input, u"ports").sequence[n]);
            }
            port_changes += moved_ports;
            improvements += *result < index(field(expected, u"beforeCrossings"));
        }
        expect(cases == 25_u);
        expect(improvements >= 8_u);
        expect(port_changes >= 4_u);
    };
};
}
