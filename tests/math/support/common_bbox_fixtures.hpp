#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct BBoxSourceCase {
    std::string name;
    bool display;
    double w;
    double h;
    double d;
    double scale;
    double rscale;
    double L;
    double R;
    double ic;
    double oc;
    double sk;
    double dx;
    std::string pwidth;
};

inline const auto& bboxSourceCases() {
    static const auto result = records<BBoxSourceCase>(
        "math/fixtures/output/common-bbox/cases.tsv", 14, 36, [](const auto& r) {
            return BBoxSourceCase{bytes(r[0]),
                boolean(r[1]),
                std::bit_cast<double>(binary64(r[2])),
                std::bit_cast<double>(binary64(r[3])),
                std::bit_cast<double>(binary64(r[4])),
                std::bit_cast<double>(binary64(r[5])),
                std::bit_cast<double>(binary64(r[6])),
                std::bit_cast<double>(binary64(r[7])),
                std::bit_cast<double>(binary64(r[8])),
                std::bit_cast<double>(binary64(r[9])),
                std::bit_cast<double>(binary64(r[10])),
                std::bit_cast<double>(binary64(r[11])),
                std::bit_cast<double>(binary64(r[12])),
                bytes(r[13])};
        });
    return result;
}

} // namespace math_test::fixtures
