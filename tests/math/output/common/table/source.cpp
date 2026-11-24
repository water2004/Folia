#include "math/support/common_table_test.hpp"

suite common_table_source = [] {
    "direct_MathML_table_attributes_match_pinned_Source_SVG"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        for (const auto &test : sourceTableCases()) {
            try {
                verifySourceMathmlTable(test, bundle);
            } catch (const std::exception &error) {
                expect(false) << "case " << test.name << " threw: " << error.what();
            }
        }
    };
};
