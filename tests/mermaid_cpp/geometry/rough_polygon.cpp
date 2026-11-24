#include "support/folia_test.hpp"
import folia.mermaid.rough_polygon;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

// Coordinate fingerprints come from RoughJS 4.6.6 operations in the
// pinned Mermaid 12.2.0 bundle. The browser getBBox values use float output.
void check_path(std::string_view path, std::size_t size, double expected) {
    std::size_t count = 0;
    double weighted = 0;
    while (!path.empty()) {
        if (path.front() == ' ' || path.front() == ',' ||
            path.front() == 'M' || path.front() == 'L' || path.front() == 'C') {
            path.remove_prefix(1);
            continue;
        }
        double value = 0;
        const auto parsed = std::from_chars(path.data(), path.data() + path.size(), value);
        expect(fatal(parsed.ec == std::errc{}));
        path.remove_prefix(static_cast<std::size_t>(parsed.ptr - path.data()));
        weighted += static_cast<double>(++count) * value;
    }
    expect(count == size);
    expect(std::abs(weighted - expected) < 1e-7);
}

void check_bounds(const RoughPolygonResult& result, const RoughBounds& expected) {
    expect(fatal(result.bounds.has_value()));
    expect(std::abs(result.bounds->left - expected.left) < 1e-5);
    expect(std::abs(result.bounds->top - expected.top) < 1e-5);
    expect(std::abs(result.bounds->right - expected.right) < 1e-5);
    expect(std::abs(result.bounds->bottom - expected.bottom) < 1e-5);
}

suite mermaid_rough_polygon = [] {
    "mermaid_rough_polygon_seeded_rectangle_and_solid_fill"_test = [] {
        auto rectangle = sketch_rectangle(-4, -3, 8, 6, {.seed = 9});
        check_path(rectangle.fill_path, 80, 1465.1197479102482);
        check_path(rectangle.stroke_path, 64, -238.9623412591964);
        check_bounds(rectangle, {-4.435436725616455, -3.4151296615600586, 5.16485071182251, 3.549312114715576});

        const std::array points{RoughPoint{1, 0}, RoughPoint{3, 0}, RoughPoint{2, 4}};
        auto polygon = sketch_polygon(points, {.seed = 1, .fill = RoughFill::solid});
        check_path(polygon.fill_path, 6, 47.87680662963539);
        check_path(polygon.stroke_path, 48, 2143.248010935258);
        check_bounds(polygon, {0.815330445766449, -0.5044229030609131, 3.2217597365379333, 4.882537126541138});
    };
    "mermaid_rough_polygon_seeded_diamond_and_edge"_test = [] {
        const std::array diamond{
            RoughCommand{RoughVerb::move, {4, 0}}, RoughCommand{RoughVerb::line, {8, -4}},
            RoughCommand{RoughVerb::line, {4, -8}}, RoughCommand{RoughVerb::line, {0, -4}},
            RoughCommand{RoughVerb::close}};
        auto node = sketch_path(diamond, {.seed = 9});
        check_path(node.fill_path, 64, 1793.170809797886);
        check_path(node.stroke_path, 64, -1012.5381282276701);
        check_bounds(node, {-0.012359052896499634, -8.380331039428711, 8.362388223409653, 0.22675323486328125});

        const std::array edge{
            RoughCommand{RoughVerb::move}, RoughCommand{RoughVerb::line, {8, 0}},
            RoughCommand{RoughVerb::quadratic, {9, 1}, {9, 0}},
            RoughCommand{RoughVerb::cubic, {11, 3}, {9, 3}, {10, 3}}};
        auto stroke = sketch_path(edge, {.seed = 7, .roughness = 0.3, .fill = RoughFill::none});
        expect(stroke.fill_path.empty());
        check_path(stroke.stroke_path, 48, 6092.506937028248);
        check_bounds(stroke, {-0.2398987114429474, -0.15043818950653076, 11.066980332136154, 3.2430490255355835});
    };
};
}
