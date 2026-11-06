#include "support/folia_test.hpp"
import folia.mermaid.swimlane_router;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

SwimlaneRoutingNode box(double x, double y, double width, double height) {
    return {.box = {.x = x, .y = y, .width = width, .height = height}};
}

SwimlaneRoutingEdge route(std::size_t start, std::size_t end,
                          std::vector<FlowchartPoint> points) {
    return {.start = start, .end = end, .placed = {.points = std::move(points)}};
}

void expect_points(const SwimlaneRoutingEdge& edge,
                   std::initializer_list<FlowchartPoint> expected) {
    expect(std::ranges::equal(edge.placed.points, expected, [](auto a, auto b) {
        return std::abs(a.x - b.x) < 1e-6 && std::abs(a.y - b.y) < 1e-6;
    }));
}

suite mermaid_swimlane_router = [] {
    // Coordinates below come from the complete pinned postProcessing.ts,
    // with consecutive renderer handoff duplicates removed.
    "swimlane_route_cleanup_moves_a_rail_off_an_intervening_node"_test = [] {
        std::array nodes{box(-166, 54, 232, 66), box(830, 54, 232, 66),
                         box(166, 54, 232, 108)};
        std::array edges{route(0, 1, {{-166, 21}, {-166, 1}, {830, 1}, {830, 21}})};
        finish_swimlane_routes(nodes, edges);
        expect_points(edges[0], {{-166, 21}, {-166, -20}, {830, -20}, {830, 21}});
    };

    "swimlane_route_cleanup_chooses_an_outer_channel_for_a_crossing"_test = [] {
        std::array nodes{box(-100, 0, 30, 20), box(100, 0, 30, 20),
                         box(0, -100, 30, 20), box(0, 100, 30, 20)};
        std::array edges{route(0, 1, {{-85, 0}, {85, 0}}),
                         route(2, 3, {{0, -90}, {0, 90}})};
        finish_swimlane_routes(nodes, edges);
        expect_points(edges[0], {{-100, -10}, {-100, -130}, {100, -130}, {100, -10}});
        expect_points(edges[1], {{0, -90}, {0, 90}});
    };

    "swimlane_route_cleanup_separates_two_terminal_rails_on_one_face"_test = [] {
        std::array nodes{box(-40, -30, 10, 10), box(0, 0, 10, 80)};
        std::array edges{route(0, 1, {{-30, 0}, {-5, 0}}),
                         route(0, 1, {{-30, 0}, {-5, 0}})};
        finish_swimlane_routes(nodes, edges);
        expect_points(edges[0], {{-30, -7}, {-5, -7}});
        expect_points(edges[1], {{-30, 0}, {-5, 0}});
    };
};
}
