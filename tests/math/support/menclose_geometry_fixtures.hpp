#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct MencloseLayoutRow {
    std::string case_name, notation;
    double w, h, d;
    std::array<double, 4> trbl;
};

inline const auto& mencloseLayoutRows() {
    static const auto result = records<MencloseLayoutRow>(
        "math/fixtures/output/menclose/geometry.tsv", 9, 56, [](const auto& r) {
            const auto number = [&](std::size_t i) {
                return std::bit_cast<double>(binary64(r[i]));
            };
            return MencloseLayoutRow{bytes(r[0]), bytes(r[1]), number(2), number(3), number(4),
                {number(5), number(6), number(7), number(8)}};
        });
    return result;
}

} // namespace math_test::fixtures
