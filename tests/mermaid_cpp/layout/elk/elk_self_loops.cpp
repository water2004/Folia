#include "support/folia_test.hpp"
import folia.mermaid.elk_compound_layout;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

std::vector<std::string_view> fields(std::string_view text, char delimiter) {
    std::vector<std::string_view> result;
    if (text == "-") return result;
    for (;;) {
        const auto end = text.find(delimiter);
        result.push_back(text.substr(0, end));
        if (end == text.npos) return result;
        text.remove_prefix(end + 1);
    }
}
double decimal(std::string_view text) {
    double value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(value))
        throw std::runtime_error("Invalid ELK self-loop fixture number");
    return value;
}
std::size_t integer(std::string_view text) {
    const auto value = decimal(text);
    if (value < 0 || value != std::floor(value)) throw std::runtime_error("Invalid ELK self-loop index");
    return static_cast<std::size_t>(value);
}
void near(double actual, double expected, std::string_view context) {
    expect(std::isfinite(actual) && std::abs(actual - expected) < 1e-4)
        << context << "actual" << actual << "expected" << expected;
}
void point(ElkRoutingPoint actual, std::string_view expected, std::string_view context) {
    const auto values = fields(expected, ',');
    expect(fatal(values.size() == 2_u));
    near(actual.x, decimal(values[0]), context);
    near(actual.y, decimal(values[1]), context);
}

std::expected<ElkFlatLayoutResult, std::string> layout(
    const ElkCompoundLayoutInput& input, const ElkFlatLayoutOptions& options) {
    if (std::ranges::any_of(input.hierarchy.nodes, &ElkCompoundInputNode::group)) {
        auto result = elk_layout_compound(input, options);
        if (!result) return std::unexpected(result.error());
        return std::move(static_cast<ElkFlatLayoutResult&>(*result));
    }
    ElkFlatLayoutInput flat{.nodes=input.nodes, .center_labels=input.center_labels, .end_labels=input.end_labels};
    for (const auto& edge : input.hierarchy.edges) flat.edges.push_back({edge.source, edge.target});
    return elk_layout_flat(flat, options);
}

suite elk_self_loops=[] {
    "mermaid_elk_self_loops_complete_geometry_matches_original"_test=[] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR}/"elk-self-loops.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count = 0, node_count = 0, edge_count = 0;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            const auto row = fields(line, '\t');
            expect(fatal(row.size() == 12_u));
            const auto name = std::string{row[0]};
            ElkCompoundLayoutInput graph;
            for (const auto item : fields(row[2], ',')) {
                const auto values = fields(item, ':');
                expect(fatal(values.size() == 6_u));
                ElkCompoundInputNode node;
                if (values[0] != "-1") node.parent = integer(values[0]);
                node.group = integer(values[1]) != 0;
                graph.hierarchy.nodes.push_back(node);
                ElkMeasuredNode measured{decimal(values[2]), decimal(values[3])};
                measured.self_loops.distribution = static_cast<ElkSelfLoopDistribution>(integer(values[4]));
                measured.self_loops.ordering = static_cast<ElkSelfLoopOrdering>(integer(values[5]));
                graph.nodes.push_back(measured);
            }
            for (const auto item : fields(row[3], ',')) {
                const auto values = fields(item, ':');
                expect(fatal(values.size() == 2_u));
                graph.hierarchy.edges.push_back({integer(values[0]), integer(values[1])});
            }
            for (const auto item : fields(row[4], ',')) {
                const auto values = fields(item, ':');
                expect(fatal(values.size() == 3_u));
                graph.center_labels.push_back({graph.center_labels.size(), integer(values[0]), decimal(values[1]), decimal(values[2])});
            }
            for (const auto item : fields(row[5], ',')) {
                const auto values = fields(item, ':');
                expect(fatal(values.size() == 4_u));
                graph.end_labels.push_back({graph.end_labels.size(), integer(values[0]), integer(values[1]) != 0,
                    decimal(values[2]), decimal(values[3])});
            }
            ElkFlatLayoutOptions options;
            options.direction = static_cast<ElkLayoutDirection>(integer(row[1]));
            const auto spacing = fields(row[6], ',');
            expect(fatal(spacing.size() == 4_u));
            options.node_self_loop_spacing = decimal(spacing[0]);
            options.placement.edge_edge = decimal(spacing[1]);
            options.edge_label_spacing = decimal(spacing[2]);
            options.label_label_spacing = decimal(spacing[3]);
            const auto result = layout(graph, options);
            ++count;
            node_count += graph.nodes.size();
            edge_count += graph.hierarchy.edges.size();
            expect(result.has_value()) << name << (result ? "" : result.error());
            if (!result) continue;
            const auto bounds = fields(row[7], ',');
            expect(fatal(bounds.size() == 2_u));
            near(result->width, decimal(bounds[0]), name + " width");
            near(result->height, decimal(bounds[1]), name + " height");
            const auto boxes = fields(row[8], ';');
            expect(boxes.size() == result->nodes.size()) << name << "node count";
            for (std::size_t n = 0; n < std::min(boxes.size(), result->nodes.size()); ++n) {
                const auto values = fields(boxes[n], ',');
                expect(fatal(values.size() == 4_u));
                const auto& box = result->nodes[n];
                const auto context = name + " node " + std::to_string(n);
                near(box.x, decimal(values[0]), context + " x");
                near(box.y, decimal(values[1]), context + " y");
                near(box.width, decimal(values[2]), context + " width");
                near(box.height, decimal(values[3]), context + " height");
            }
            for (const auto center : {true, false}) {
                const auto& labels = center ? result->center_labels : result->end_labels;
                const auto expected = fields(row[center ? 9 : 10], ';');
                expect(labels.size() == expected.size()) << name << "label count" << center;
                for (std::size_t i = 0; i < expected.size(); ++i) {
                    const auto label = std::ranges::find(labels, i, &ElkLabelPosition::id);
                    expect(label != labels.end()) << name << "missing label" << center << i;
                    if (label != labels.end()) point({label->x, label->y}, expected[i],
                        name + (center ? " center " : " terminal ") + std::to_string(i));
                }
            }
            const auto paths = fields(row[11], ';');
            expect(paths.size() == result->routes.size()) << name << "route count";
            for (std::size_t e = 0; e < std::min(paths.size(), result->routes.size()); ++e) {
                const auto values = fields(paths[e], '|');
                expect(fatal(values.size() == 3_u));
                const auto& route = result->routes[e];
                const auto context = name + " edge " + std::to_string(e);
                expect(route.original_edge == e && route.source_node == graph.hierarchy.edges[e].source &&
                    route.target_node == graph.hierarchy.edges[e].target) << context << "identity";
                point(route.start, values[0], context + " start");
                point(route.end, values[1], context + " end");
                const auto bends = fields(values[2], ':');
                expect(bends.size() == route.bends.size()) << context << "bend count" << bends.size() << route.bends.size();
                for (std::size_t p = 0; p < std::min(bends.size(), route.bends.size()); ++p)
                    point(route.bends[p], bends[p], context + " bend " + std::to_string(p));
            }
        }
        expect(count == 24_u);
        expect(node_count >= 50_u);
        expect(edge_count >= 200_u);
    };
};
}
