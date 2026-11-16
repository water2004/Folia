#include "support/folia_test.hpp"
import folia.mermaid.flowchart_elk;
import folia.mermaid.directives;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

const ConfigValue& field(const ConfigValue& object, std::u16string_view name) {
    const auto* value = object.find(name);
    if (!value) throw std::runtime_error("Missing ELK directed-scope fixture field");
    return *value;
}
double number(const ConfigValue& value) {
    if (value.kind != ConfigKind::number || !std::isfinite(value.number))
        throw std::runtime_error("Invalid ELK directed-scope fixture number");
    return value.number;
}
std::size_t index(const ConfigValue& value) {
    const auto result = number(value);
    if (result < 0 || std::floor(result) != result)
        throw std::runtime_error("Invalid ELK directed-scope fixture index");
    return static_cast<std::size_t>(result);
}
ConfigValue decode_json(std::string_view hex) {
    if (hex.size() % 4) throw std::runtime_error("Invalid ELK fixture UTF-16");
    std::u16string text;
    for (std::size_t offset = 0; offset < hex.size(); offset += 4) {
        unsigned value{};
        const auto parsed = std::from_chars(hex.data() + offset, hex.data() + offset + 4, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != hex.data() + offset + 4)
            throw std::runtime_error("Invalid ELK fixture hex digit");
        text += static_cast<char16_t>(value);
    }
    auto result = parse_json_value(text);
    if (!result) throw std::runtime_error(result.error());
    return std::move(*result);
}
void near(double actual, double expected, std::string_view case_name, std::string_view member) {
    expect(std::isfinite(actual) && std::abs(actual - expected) < 1e-4)
        << case_name << " / " << member << " actual " << actual << " expected " << expected;
}
void point(FlowchartPoint actual, const ConfigValue& expected, std::string_view name, std::string_view member) {
    expect(fatal(expected.sequence.size() == 2_u)) << name << member;
    near(actual.x, number(expected.sequence[0]), name, member);
    near(actual.y, number(expected.sequence[1]), name, member);
}

suite elk_directed_scopes = [] {
    "mermaid_elk_directed_scopes_match_complete_official_adapter"_test = [] {
        const auto override_dir = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream file{std::filesystem::path{override_dir ? override_dir : FOLIA_MERMAID_FIXTURE_DIR}
            / "elk-directed-scopes.tsv"};
        expect(fatal(file.is_open()));
        std::string line;
        std::size_t cases = 0, nodes_checked = 0, edges_checked = 0;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            const auto first = line.find('\t'), second = line.find('\t', first + 1);
            expect(fatal(first != line.npos && second != line.npos));
            const auto name = std::string_view{line}.substr(0, first);
            const auto input = decode_json(std::string_view{line}.substr(first + 1, second - first - 1));
            const auto expected = decode_json(std::string_view{line}.substr(second + 1));

            FlowchartInput source;
            source.direction = field(input, u"direction").string;
            source.effective_config = field(input, u"config");
            std::vector<FlowchartNodeBox> boxes;
            std::vector<FlowchartEdgeLabelSize> labels;
            FlowchartMeasurements measurements;
            for (const auto& node : field(input, u"nodes").sequence) {
                FlowchartNode value;
                value.id = field(node, u"id").string;
                value.label = value.id;
                value.parent = field(node, u"parent").string;
                value.direction = field(node, u"direction").string;
                value.group = field(node, u"group").boolean;
                value.shape = field(node, u"shape").string;
                value.padding = number(field(node, u"padding"));
                source.nodes.push_back(std::move(value));
                boxes.push_back({number(field(node, u"width")), number(field(node, u"height"))});
                measurements.nodes.push_back({.width = number(field(node, u"labelWidth")),
                    .height = number(field(node, u"labelHeight"))});
            }
            for (const auto& edge : field(input, u"edges").sequence) {
                FlowchartEdge value;
                value.id = field(edge, u"id").string;
                value.start = field(edge, u"start").string;
                value.end = field(edge, u"end").string;
                value.label = field(edge, u"label").string;
                source.edges.push_back(std::move(value));
                labels.push_back({number(field(edge, u"width")), number(field(edge, u"height"))});
            }
            std::vector<ElkEndLabel> end_labels;
            for (const auto& label : field(input, u"ends").sequence) {
                end_labels.push_back({index(field(label, u"id")), index(field(label, u"edge")),
                    field(label, u"head").boolean, number(field(label, u"width")), number(field(label, u"height"))});
            }
            // The fixture owns the fixed boxes at this algorithm boundary.
            // No typography provider, approximate font width, or SVG is used.
            const auto scene = layout_measured_elk_scene(source, boxes, labels, measurements, end_labels);
            ++cases;
            expect(scene.has_value()) << name << (scene ? "" : scene.error());
            if (!scene) continue;
            const auto& layout = scene->layout;
            const auto& bounds = field(expected, u"bounds").sequence;
            expect(fatal(bounds.size() == 2_u));
            near(layout.width, number(bounds[0]), name, "root width");
            near(layout.height, number(bounds[1]), name, "root height");
            const auto& nodes = field(expected, u"nodes").sequence;
            expect(fatal(layout.nodes.size() == nodes.size())) << name;
            for (const auto& item : nodes) {
                const auto& row = item.sequence;
                expect(fatal(row.size() == 5_u));
                const auto node = std::ranges::find(layout.nodes, row[0].string, &FlowchartPlacedNode::id);
                expect(fatal(node != layout.nodes.end())) << name;
                const auto member = "node " + std::string{node->id.begin(), node->id.end()};
                near(node->x, number(row[1]), name, member + " x");
                near(node->y, number(row[2]), name, member + " y");
                near(node->width, number(row[3]), name, member + " width");
                near(node->height, number(row[4]), name, member + " height");
                ++nodes_checked;
            }
            const auto& routes = field(expected, u"routes").sequence;
            expect(fatal(layout.edges.size() == routes.size())) << name;
            for (std::size_t e = 0; e < routes.size(); ++e) {
                const auto& route = layout.edges[e].points;
                const auto& points = routes[e].sequence;
                expect(route.size() == points.size()) << name << " edge " << e << " point count";
                for (std::size_t p = 0; p < std::min(route.size(), points.size()); ++p)
                    point(route[p], points[p], name, "edge " + std::to_string(e) + " point " + std::to_string(p));
                ++edges_checked;
            }
            std::size_t centers = 0;
            for (const auto& edge : layout.edges) centers += edge.label.has_value();
            const auto& expected_centers = field(expected, u"centers").sequence;
            expect(centers == expected_centers.size()) << name;
            for (const auto& item : expected_centers) {
                const auto& row = item.sequence;
                expect(fatal(row.size() == 3_u));
                const auto edge = index(row[0]);
                expect(fatal(edge < layout.edges.size() && layout.edges[edge].label.has_value())) << name;
                const auto member = "edge " + std::to_string(edge) + " center label";
                near(layout.edges[edge].label->x, number(row[1]), name, member + " x");
                near(layout.edges[edge].label->y, number(row[2]), name, member + " y");
            }
            const auto& expected_ends = field(expected, u"ends").sequence;
            expect(fatal(scene->end_labels.size() == expected_ends.size())) << name;
            for (const auto& item : expected_ends) {
                const auto& row = item.sequence;
                expect(fatal(row.size() == 6_u));
                const auto label = std::ranges::find(scene->end_labels, index(row[0]), &ElkLabelPosition::id);
                expect(fatal(label != scene->end_labels.end())) << name;
                expect(label->original_edge == index(row[1])) << name;
                const auto member = "end label " + std::to_string(label->id);
                near(label->x, number(row[2]), name, member + " x");
                near(label->y, number(row[3]), name, member + " y");
                near(label->width, number(row[4]), name, member + " width");
                near(label->height, number(row[5]), name, member + " height");
            }
        }
        expect(cases == 21_u);
        expect(nodes_checked >= 140_u);
        expect(edges_checked >= 130_u);
    };
};
}
