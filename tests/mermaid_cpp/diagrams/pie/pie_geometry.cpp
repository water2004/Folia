#include "support/folia_test.hpp"

import folia.mermaid.pie_geometry;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

std::vector<std::string_view> geometry_fields(std::string_view input, char separator) {
    std::vector<std::string_view> result;
    for (;;) {
        auto const end = input.find(separator);
        result.push_back(input.substr(0, end));
        if (end == std::string_view::npos) return result;
        input.remove_prefix(end + 1);
    }
}

double geometry_number(std::string_view input) {
    double value{};
    auto const result = std::from_chars(input.data(), input.data() + input.size(), value);
    if (result.ec != std::errc{} || result.ptr != input.data() + input.size()) {
        throw std::runtime_error("Malformed Pie geometry number fixture");
    }
    return value;
}

bool same_centroid(double actual, double expected) {
    // JS Math and the platform CRT may differ by a few ulps. Paths themselves
    // are compared byte-for-byte after D3's three-place coordinate rounding.
    double const tolerance = 1e-12 * std::max(1.0, std::abs(expected));
    return std::isfinite(actual) && std::abs(actual - expected) <= tolerance;
}

suite pie_geometry = [] {
    "mermaid_pie_geometry_matches_actual_d3"_test = [] {
        auto const fixture_override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{
            fixture_override ? fixture_override : FOLIA_MERMAID_FIXTURE_DIR} / "pie-geometry.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t cases{};
        std::size_t sectors{};
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            auto const fields = geometry_fields(line, '\t');
            expect(fatal(fields.size() == 5_u));
            auto const name = fields[0];
            std::vector<double> values;
            if (fields[3] != "-") {
                for (auto value : geometry_fields(fields[3], ',')) values.push_back(geometry_number(value));
            }
            PieGeometryConfig config{geometry_number(fields[1]), geometry_number(fields[2])};
            auto actual = layout_pie_geometry(values, config);
            expect(fatal(actual.has_value())) << name;
            expect(fatal(actual->size() == values.size())) << name;
            if (fields[4] == "-") {
                expect(actual->empty()) << name;
            } else {
                auto const expected = geometry_fields(fields[4], ';');
                expect(fatal(actual->size() == expected.size())) << name;
                for (std::size_t index = 0; index < expected.size(); ++index) {
                    auto const observed = geometry_fields(expected[index], '|');
                    expect(fatal(observed.size() == 6_u));
                    auto const& sector = (*actual)[index];
                    expect(sector.value == geometry_number(observed[0])) << name << index << " value";
                    expect(sector.start_angle == geometry_number(observed[1])) << name << index << " start";
                    expect(sector.end_angle == geometry_number(observed[2])) << name << index << " end";
                    expect(same_centroid(sector.label_x, geometry_number(observed[3]))) << name << index << " label x";
                    expect(same_centroid(sector.label_y, geometry_number(observed[4]))) << name << index << " label y";
                    expect(sector.path == observed[5]) << name << index << " exact D3 path";
                    ++sectors;
                }
            }
            ++cases;
        }
        expect(cases == 86_u);
        expect(sectors > 400_u);
    };

    "mermaid_pie_geometry_rejects_nonfinite_values_and_arithmetic"_test = [] {
        auto const infinity = std::numeric_limits<double>::infinity();
        auto const nan = std::numeric_limits<double>::quiet_NaN();
        auto const maximum = std::numeric_limits<double>::max();
        for (double invalid : {infinity, -infinity, nan}) {
            std::array values{1.0, invalid};
            auto result = layout_pie_geometry(values);
            expect(fatal(!result.has_value()));
            expect(result.error().index == 1_u);
            expect(!result.error().message.empty());
        }
        for (auto values : {std::vector<double>{maximum, maximum},
                            std::vector<double>{std::numeric_limits<double>::denorm_min()}}) {
            auto result = layout_pie_geometry(values);
            expect(fatal(!result.has_value()));
            expect(result.error().index == values.size());
            expect(!result.error().message.empty());
        }
        std::array values{1.0, 2.0};
        for (auto config : {PieGeometryConfig{nan, 0.75}, PieGeometryConfig{infinity, 0.75},
                            PieGeometryConfig{0, nan}, PieGeometryConfig{0, infinity},
                            PieGeometryConfig{0, maximum}, PieGeometryConfig{0, maximum / 200}}) {
            auto result = layout_pie_geometry(values, config);
            expect(fatal(!result.has_value()));
            expect(result.error().index == values.size());
            expect(!result.error().message.empty());
        }
    };
};
}
