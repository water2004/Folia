#include "support/folia_test.hpp"
#include "math/support/unit_util_fixtures.hpp"
import folia.math.input.tex.unit_util;

using namespace boost::ut;
using namespace folia::math::tex;

namespace {
using UnitData = math_test::fixtures::UnitUtilFixtureData<DimensionMatch>;


// The utility's original public unit table is mutable. Each test restores its
// prior value so Source mutation cases cannot affect later parser tests.
struct UnitCasesScope {
    UnitUtil& util{UnitUtil::instance()};
    UnitMap previous{std::move(util.UNIT_CASES)};
    UnitCasesScope() { util.UNIT_CASES = defaultUnitCases(); }
    ~UnitCasesScope() { util.UNIT_CASES = std::move(previous); }
};

bool sameNumber(double actual, double expected) {
    if (std::isnan(expected)) return std::isnan(actual);
    return actual == expected && (expected != 0 || std::signbit(actual) == std::signbit(expected));
}
}

suite native_tex_unit_util = [] {
    "native units retain all nine Source conversion factors and rounded mu values"_test = [] {
        UnitCasesScope scope;
        for (auto const& row : UnitData::default_cases()) {
            expect(scope.util.UNIT_CASES.get(row.unit) == row.factor);
            auto text = std::u16string(u"1") + std::u16string(row.unit);
            expect(scope.util.matchDimen(text) == row.match);
            expect(sameNumber(scope.util.dimen2em(text), row.em));
        }
    };

    "native dimension grammar matches pinned Source lexical values and UTF16 lengths"_test = [] {
        UnitCasesScope scope;
        for (auto const& row : UnitData::match_cases()) {
            expect(scope.util.matchDimen(row.input, row.rest) == row.match);
            expect(sameNumber(scope.util.dimen2em(row.input), row.em));
        }
    };

    "native dimension em formatting preserves Source thresholds rounding and exponents"_test = [] {
        auto& util = UnitUtil::instance();
        for (auto const& row : UnitData::format_cases())
            expect(util.em(row.input) == row.output);
    };

    "native dimension trimming preserves escaped ASCII space and Unicode whitespace rules"_test = [] {
        auto& util = UnitUtil::instance();
        for (auto const& row : UnitData::trim_cases())
            expect(util.trimSpaces(row.input) == row.output);
    };

    "native UnitMap updates preserve insertion order fallback and compiled pattern ownership"_test = [] {
        UnitMap map({{u"x", 1}, {u"pt", .1}, {u"x", 2}});
        expect(map.unit == u"(x|pt)");
        expect(map.get(u"x") == 2.0);
        expect(map.set(u"x", 3) == &map);
        map.set(u"z", 4);
        expect(map.unit == u"(x|pt|z)");
        auto* unchanged = map.dimenEnd.get();
        expect(!map.erase(u"missing"));
        expect(map.dimenEnd.get() == unchanged);
        expect(map.erase(u"x"));
        map.set(u"x", 5);
        expect(map.unit == u"(pt|z|x)");
        expect(map.dimenEnd->source() == u"^\\s*([-+]?([.,]\\d+|\\d+([.,]\\d*)?))\\s*(pt|z|x)\\s*$");
        map.set(u"zero", 0);
        map.set(u"nan", std::numeric_limits<double>::quiet_NaN());
        map.set(u"neg", -2);
        expect(map.get(u"zero") == .1);
        expect(map.get(u"nan") == .1);
        expect(map.get(u"neg") == -2.0);
        expect(map.get(u"missing") == .1);
        expect(map.erase(u"pt"));
        expect(!map.get(u"zero"));
        expect(!map.get(u"nan"));
        expect(!map.get(u"missing"));
        map.set(u"pt", 0);
        expect(map.get(u"missing") == 0.0);
        map.set(u"pt", std::numeric_limits<double>::quiet_NaN());
        expect(std::isnan(map.get(u"missing").value()));
        UnitMap moved(std::move(map));
        expect(moved.get(u"x") == 5.0);
        expect(moved.dimenEnd->exec(u"2x").has_value());
    };

    "native unit matching retains ordered raw regex names empty keys and supplementary units"_test = [] {
        UnitCasesScope scope;
        scope.util.UNIT_CASES = UnitMap({{u"e", 1}, {u"em", 2}, {u"pt", .1}});
        expect(scope.util.matchDimen(u"2emtail", true) == DimensionMatch{u"2", u"e", 2});
        expect(scope.util.matchDimen(u"2em") == DimensionMatch{u"2", u"em", 3});
        scope.util.UNIT_CASES = UnitMap({{u"x.", 3}, {u"pt", .1}});
        expect(scope.util.matchDimen(u"2xy") == DimensionMatch{u"2", u"xy", 3});
        // The match is xy; the stored key x. is regex syntax, so conversion
        // uses Source's point fallback rather than its value 3.
        expect(sameNumber(scope.util.dimen2em(u"2xy"), .2));
        scope.util.UNIT_CASES = UnitMap({{u"𝛼", 2}, {u"pt", .1}});
        expect(scope.util.matchDimen(u"2𝛼tail", true) == DimensionMatch{u"2", u"𝛼", 3});
        scope.util.UNIT_CASES = UnitMap({{u"", 3}, {u"pt", .1}});
        expect(scope.util.matchDimen(u"2") == DimensionMatch{u"2", u"", 1});
        expect(sameNumber(scope.util.dimen2em(u"2"), 6));
        expect(sameNumber(scope.util.dimen2em(u"bad"), .1));
        scope.util.UNIT_CASES = UnitMap({});
        expect(scope.util.matchDimen(u"2") == DimensionMatch{u"2", u"", 1});
        expect(std::isnan(scope.util.dimen2em(u"2")));
        expect(std::isnan(scope.util.dimen2em(u"bad")));
    };

    "native mu conversion observes live factors before rounded em conversion"_test = [] {
        UnitCasesScope scope;
        scope.util.UNIT_CASES.set(u"mu", .25);
        expect(scope.util.matchDimen(u"2mu") == DimensionMatch{u"0.5", u"em", 3});
        expect(sameNumber(scope.util.dimen2em(u"2mu"), .5));
        scope.util.UNIT_CASES.set(u"em", 2);
        expect(sameNumber(scope.util.dimen2em(u"2mu"), 1));
        scope.util.UNIT_CASES.set(u"mu", 0);
        expect(scope.util.matchDimen(u"2mu") == DimensionMatch{u"0.2", u"em", 3});
        expect(sameNumber(scope.util.dimen2em(u"2mu"), .4));
        scope.util.UNIT_CASES.set(u"mu", std::numeric_limits<double>::infinity());
        expect(scope.util.matchDimen(u"2mu") == DimensionMatch{u"Infinity", u"em", 3});
        expect(std::isinf(scope.util.dimen2em(u"2mu")));
    };
};
