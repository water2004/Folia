#include "support/folia_test.hpp"
import folia.mermaid.elk_components;
import folia.mermaid.directives;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"

std::vector<std::size_t> indices(const ConfigValue &value) {
    std::vector<std::size_t> result;
    for (const auto &item : value.sequence)
        result.push_back(static_cast<std::size_t>(item.number));
    return result;
}

suite elk_components = [] {
    "mermaid_elk_components_match_original_dfs_and_row_packing"_test = [] {
        const auto directory = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path(directory ? directory : FOLIA_MERMAID_FIXTURE_DIR) /
                            "elk-components.tsv"};
        expect(fatal(input.is_open()));
        std::size_t count{};
        std::string row;
        while (std::getline(input, row)) {
            if (!row.empty() && row.back() == '\r')
                row.pop_back();
            if (row.empty() || row.front() == '#')
                continue;
            const auto first = row.find('\t'), second = row.find('\t', first + 1),
                       third = row.find('\t', second + 1);
            expect(fatal(first != row.npos && second != row.npos && third != row.npos));
            const auto name = row.substr(0, first);
            const auto kind = row.substr(first + 1, second - first - 1);
            const auto parameters =
                json(decode(std::string_view(row).substr(second + 1, third - second - 1)));
            const auto expected = json(decode(std::string_view(row).substr(third + 1)));
            ++count;
            if (kind == "split") {
                std::vector<ElkFlatEdge> edges;
                for (const auto &edge : field(parameters, u"edges").sequence) {
                    edges.push_back({static_cast<std::size_t>(edge.sequence[0].number),
                                     static_cast<std::size_t>(edge.sequence[1].number)});
                }
                const auto result =
                    elk_split_components(static_cast<std::size_t>(field(parameters, u"nodes").number), edges);
                expect(fatal(result.has_value())) << name;
                expect(result->size() == expected.sequence.size()) << name;
                for (std::size_t i = 0; i < std::min(result->size(), expected.sequence.size()); ++i) {
                    expect((*result)[i].nodes == indices(field(expected.sequence[i], u"nodes"))) << name << i;
                    expect((*result)[i].edges == indices(field(expected.sequence[i], u"edges"))) << name << i;
                }
            } else {
                expect(fatal(kind == "pack"));
                std::vector<ElkComponentSize> sizes;
                for (const auto &size : field(parameters, u"sizes").sequence)
                    sizes.push_back({size.sequence[0].number, size.sequence[1].number});
                const auto result = elk_pack_components(sizes, field(parameters, u"spacing").number,
                                                        field(parameters, u"aspect").number);
                expect(fatal(result.has_value())) << name;
                expect(result->order == indices(field(expected, u"order"))) << name;
                expect(std::abs(result->width - field(expected, u"width").number) < 1e-9) << name;
                expect(std::abs(result->height - field(expected, u"height").number) < 1e-9) << name;
                const auto &offsets = field(expected, u"offsets").sequence;
                expect(result->offsets.size() == offsets.size()) << name;
                for (std::size_t i = 0; i < std::min(result->offsets.size(), offsets.size()); ++i) {
                    expect(std::abs(result->offsets[i].x - offsets[i].sequence[0].number) < 1e-9)
                        << name << i;
                    expect(std::abs(result->offsets[i].y - offsets[i].sequence[1].number) < 1e-9)
                        << name << i;
                }
            }
        }
        expect(count == 14_u);
    };
    "mermaid_elk_components_match_complete_original_layouts"_test = [] {
        const auto directory = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path(directory ? directory : FOLIA_MERMAID_FIXTURE_DIR) /
                            "elk-components-layout.tsv"};
        expect(fatal(input.is_open()));
        std::size_t count{};
        std::string row;
        while (std::getline(input, row)) {
            if (!row.empty() && row.back() == '\r')
                row.pop_back();
            if (row.empty() || row.front() == '#')
                continue;
            const auto first = row.find('\t'), second = row.find('\t', first + 1);
            expect(fatal(first != row.npos && second != row.npos));
            const auto name = row.substr(0, first);
            const auto parameters = json(decode(std::string_view(row).substr(first + 1, second - first - 1)));
            const auto expected = json(decode(std::string_view(row).substr(second + 1)));
            const auto near = [&](double actual, double target, std::string_view member) {
                expect(std::isfinite(actual) && std::abs(actual - target) < 1e-4)
                    << name << member << " actual " << actual << " expected " << target;
            };
            ElkCompoundLayoutInput graph;
            for (const auto &node : field(parameters, u"nodes").sequence) {
                expect(fatal(node.sequence.size() == 2_u));
                graph.nodes.push_back({node.sequence[0].number, node.sequence[1].number});
                graph.hierarchy.nodes.emplace_back();
            }
            for (const auto &edge : field(parameters, u"edges").sequence) {
                expect(fatal(edge.sequence.size() == 2_u));
                graph.hierarchy.edges.push_back({static_cast<std::size_t>(edge.sequence[0].number),
                                                 static_cast<std::size_t>(edge.sequence[1].number)});
            }
            graph.node_model_order = indices(field(parameters, u"nodeModelOrder"));
            const auto &configuration = field(parameters, u"options");
            expect(fatal(field(configuration, u"direction").string == u"RIGHT"));
            ElkFlatLayoutOptions options;
            options.padding = field(configuration, u"padding").number;
            options.consider_model_order = field(configuration, u"considerModelOrder").boolean;
            options.hierarchical_layout = field(configuration, u"hierarchicalLayout").boolean;
            options.two_sided_greedy_switch = field(configuration, u"twoSidedGreedySwitch").boolean;
            options.greedy_switch_activation_threshold =
                static_cast<std::size_t>(field(configuration, u"greedySwitchActivationThreshold").number);
            // These are the shared component processor's original defaults.
            near(field(configuration, u"componentSpacing").number, 24, "component spacing");
            near(field(configuration, u"aspectRatio").number, static_cast<double>(1.6f), "aspect ratio");
            const auto layout = elk_layout_components(graph, options);
            ++count;
            expect(fatal(layout.has_value())) << name << (layout ? "" : layout.error());
            const auto &bounds = field(expected, u"bounds").sequence;
            expect(fatal(bounds.size() == 2_u));
            near(layout->width, bounds[0].number, "width");
            near(layout->height, bounds[1].number, "height");
            const auto &nodes = field(expected, u"nodes").sequence;
            expect(fatal(layout->nodes.size() == nodes.size()));
            for (std::size_t n = 0; n < nodes.size(); ++n) {
                const auto &box = nodes[n].sequence;
                expect(fatal(box.size() == 4_u));
                const auto label = "node " + std::to_string(n);
                near(layout->nodes[n].x, box[0].number, label + " x");
                near(layout->nodes[n].y, box[1].number, label + " y");
                near(layout->nodes[n].width, box[2].number, label + " width");
                near(layout->nodes[n].height, box[3].number, label + " height");
            }
            const auto &routes = field(expected, u"routes").sequence;
            expect(fatal(layout->routes.size() == routes.size()));
            for (std::size_t e = 0; e < routes.size(); ++e) {
                const auto &route = layout->routes[e];
                const auto &reference = routes[e];
                const auto label = "edge " + std::to_string(e);
                expect(route.original_edge == static_cast<std::size_t>(field(reference, u"edge").number));
                expect(route.source_node == static_cast<std::size_t>(field(reference, u"source").number));
                expect(route.target_node == static_cast<std::size_t>(field(reference, u"target").number));
                std::vector<ElkRoutingPoint> points{route.start};
                points.insert(points.end(), route.bends.begin(), route.bends.end());
                points.push_back(route.end);
                const auto &reference_points = field(reference, u"points").sequence;
                expect(fatal(points.size() == reference_points.size())) << name << label;
                for (std::size_t p = 0; p < points.size(); ++p) {
                    const auto &point = reference_points[p].sequence;
                    expect(fatal(point.size() == 2_u));
                    near(points[p].x, point[0].number, label + " x");
                    near(points[p].y, point[1].number, label + " y");
                }
            }
            expect(layout->center_labels.size() == field(expected, u"centers").sequence.size());
            expect(layout->end_labels.size() == field(expected, u"ends").sequence.size());
        }
        expect(count == 7_u);
    };
};
} // namespace
