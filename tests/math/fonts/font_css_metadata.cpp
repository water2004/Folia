#include "support/folia_test.hpp"
import folia.math.fonts.newcm_font_module_provider;

namespace {
using namespace boost::ut;
using namespace folia::math::output::common;

struct ExpectedCssFont {
    std::string_view variant;
    std::string_view family;
    bool italic;
    bool bold;
};

constexpr std::array expectedCssFonts{
    ExpectedCssFont{"-bbold", "serif", false, false},
    ExpectedCssFont{"-ex-md", "serif", false, false},
    ExpectedCssFont{"-largeop", "serif", false, false},
    ExpectedCssFont{"-lf-tp", "serif", false, false},
    ExpectedCssFont{"-rt-bt", "serif", false, false},
    ExpectedCssFont{"-size3", "serif", false, false},
    ExpectedCssFont{"-size4", "serif", false, false},
    ExpectedCssFont{"-size5", "serif", false, false},
    ExpectedCssFont{"-size6", "serif", false, false},
    ExpectedCssFont{"-size7", "serif", false, false},
    ExpectedCssFont{"-smallop", "serif", false, false},
    ExpectedCssFont{"-tex-bold-calligraphic", "cursive", true, true},
    ExpectedCssFont{"-tex-bold-oldstyle", "serif", false, true},
    ExpectedCssFont{"-tex-calligraphic", "cursive", true, false},
    ExpectedCssFont{"-tex-mathit", "serif", true, false},
    ExpectedCssFont{"-tex-oldstyle", "serif", false, false},
    ExpectedCssFont{"-tex-variant", "serif", false, false},
    ExpectedCssFont{"-uplarge", "serif", false, false},
    ExpectedCssFont{"-upsmall", "serif", false, false},
    ExpectedCssFont{"bold", "serif", false, true},
    ExpectedCssFont{"bold-fraktur", "serif", false, true},
    ExpectedCssFont{"bold-italic", "serif", true, true},
    ExpectedCssFont{"bold-sans-serif", "sans-serif", false, true},
    ExpectedCssFont{"bold-script", "cursive", false, true},
    ExpectedCssFont{"double-struck", "serif", false, true},
    ExpectedCssFont{"fraktur", "serif", false, false},
    ExpectedCssFont{"italic", "serif", true, false},
    ExpectedCssFont{"monospace", "monospace", false, false},
    ExpectedCssFont{"normal", "serif", false, false},
    ExpectedCssFont{"sans-serif", "sans-serif", false, false},
    ExpectedCssFont{"sans-serif-bold-italic", "sans-serif", true, true},
    ExpectedCssFont{"sans-serif-italic", "sans-serif", true, false},
    ExpectedCssFont{"script", "cursive", false, false},
};

suite newcm_font_css_metadata = [] {
    "canonical NewCM provider publishes all locked CSS metadata"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        const auto &face = bundle.fonts.face("MathJaxNewcm");
        expect(face.cssFamilyPrefix.empty());
        expect(face.cssFontMap.size() == expectedCssFonts.size());
        expect(face.styles.empty());
        for (const auto &expected : expectedCssFonts) {
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
