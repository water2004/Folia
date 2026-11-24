#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct TextcompCase {
    std::u16string name;
    std::u16string glyph;
    std::u16string font;
};

inline const auto& textcompCases() {
    static const auto result = records<TextcompCase>(
        "math/fixtures/packages/textcomp/commands.tsv", 3, 117, [](const auto& r) {
            return TextcompCase{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

} // namespace math_test::fixtures
