#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct StyleCase {
    std::u16string input;
    std::u16string tex;
    std::u16string texBold;
    std::u16string french;
};

inline const auto& styleCases() {
    static const auto result = records<StyleCase>(
        "math/fixtures/input/parse-options/cases.tsv", 4, 30, [](const auto& r) {
            return StyleCase{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3])};
        });
    return result;
}

} // namespace math_test::fixtures
