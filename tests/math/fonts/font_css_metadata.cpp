#include "support/folia_test.hpp"
#include "math/support/fixture_values.hpp"
import folia.math.fonts.newcm_font_module_provider;

namespace {
using namespace boost::ut;
using namespace folia::math::output::common;

struct ExpectedCssFont {
    std::string variant;
    std::string family;
    bool italic;
    bool bold;
};

const auto& expectedCssFonts() {
    static const auto result = math_test::fixtures::records<ExpectedCssFont>(
        "math/fixtures/fonts/css-metadata.tsv", 4, 33, [](const auto& r) {
            return ExpectedCssFont{math_test::fixtures::bytes(r[0]), math_test::fixtures::bytes(r[1]),
                math_test::fixtures::boolean(r[2]), math_test::fixtures::boolean(r[3])};
        });
    return result;
}

suite newcm_font_css_metadata = [] {
    "canonical NewCM provider publishes all locked CSS metadata"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        const auto &face = bundle.fonts.face("MathJaxNewcm");
        expect(face.cssFamilyPrefix.empty());
        expect(face.cssFontMap.size() == expectedCssFonts().size());
        expect(face.styles.empty());
        for (const auto &expected : expectedCssFonts()) {
            const auto found = face.cssFontMap.find(expected.variant);
            expect(found != face.cssFontMap.end()) << expected.variant;
            if (found == face.cssFontMap.end())
                continue;
            expect(found->second.family == expected.family) << expected.variant;
            expect(found->second.italic == expected.italic) << expected.variant;
            expect(found->second.bold == expected.bold) << expected.variant;
            const auto query = face.getCssFont(expected.variant);
            expect(query.family == expected.family) << expected.variant;
            expect(query.italic == expected.italic) << expected.variant;
            expect(query.bold == expected.bold) << expected.variant;
        }
        const auto missing = face.getCssFont("not-a-newcm-variant");
        expect(missing.family == "serif");
        expect(!missing.italic);
        expect(!missing.bold);
        expect(face.getFamily("serif") == "serif");
        expect(face.getFamily("").empty());
    };
};
} // namespace
