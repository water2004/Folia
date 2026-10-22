#include "support/folia_test.hpp"
import folia.math.core.math_item;
import folia.math.input.tex.find_tex;

namespace {
using namespace boost::ut;
using folia::math::core::MathDisplay;
using folia::math::core::ProtoItem;
using folia::math::tex::FindTeX;
using folia::math::tex::FindTeXOptions;

struct FindTexExpected {
    std::u16string_view math, open, close;
    std::size_t n, start, end;
    MathDisplay display;
};
struct FindTexCase {
    std::u16string_view name;
    std::vector<std::u16string_view> strings;
    std::vector<std::array<std::u16string_view, 2>> inlineMath, displayMath;
    bool processEscapes, processEnvironments, processRefs;
    std::vector<FindTexExpected> expected;
};
#include "math/fixtures/find_tex.inc"

FindTeXOptions optionsFor(FindTexCase const &test) {
    FindTeXOptions options;
    options.inlineMath.clear();
    options.displayMath.clear();
    for (auto const &pair : test.inlineMath)
        options.inlineMath.push_back({std::u16string(pair[0]), std::u16string(pair[1])});
    for (auto const &pair : test.displayMath)
        options.displayMath.push_back({std::u16string(pair[0]), std::u16string(pair[1])});
    options.processEscapes = test.processEscapes;
    options.processEnvironments = test.processEnvironments;
    options.processRefs = test.processRefs;
    return options;
}

std::vector<std::u16string> ownStrings(FindTexCase const &test) {
    std::vector<std::u16string> strings;
    for (auto value : test.strings)
        strings.emplace_back(value);
    return strings;
}

void expectSourceRows(FindTexCase const &test, std::vector<ProtoItem> const &actual) {
    expect(actual.size() == test.expected.size());
    const auto count = std::min(actual.size(), test.expected.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto &got = actual[i];
        const auto &want = test.expected[i];
        expect(got.math == want.math);
        expect(got.open == want.open);
        expect(got.close == want.close);
        expect(got.n == want.n);
        expect(got.start.n == want.start);
        expect(got.end.n == want.end);
        // Source protoItem's null display (delimiters and refs/escapes) maps
        // explicitly to the typed native escaped value in the fixture.
        expect(got.display == want.display);
    }
}
} // namespace

suite find_tex = [] {
    "FindTeX matches every pinned Source ProtoItem field"_test = [] {
        expect(findTexCases.size() == 22_u);
        for (const auto &test : findTexCases) {
            FindTeX finder(optionsFor(test));
            const auto strings = ownStrings(test);
            const auto actual = finder.findMath(strings);
            expectSourceRows(test, actual);
        }
    };

    "FindTeX resets Source regex cursors between repeated calls"_test = [] {
        std::size_t checked = 0;
        for (const auto &test : findTexCases) {
            FindTeX finder(optionsFor(test));
            const auto strings = ownStrings(test);
            const auto first = finder.findMath(strings);
            const auto second = finder.findMath(strings);
            expectSourceRows(test, first);
            expectSourceRows(test, second);
            expect(first.size() == second.size());
            ++checked;
        }
        expect(checked == findTexCases.size());
    };

    "FindTeX rejects an empty opening delimiter in the typed API"_test = [] {
        // This is a native precondition; Source equivalence is not asserted.
        auto options = FindTeXOptions{};
        options.inlineMath = {{u"", u"x"}};
        bool rejected = false;
        try {
            FindTeX finder(std::move(options));
            (void)finder;
        } catch (std::invalid_argument const &) {
            rejected = true;
        }
        expect(rejected);
    };
};

namespace {
#include "math/fixtures/find-tex-retained-source.inc"
}
suite retained_find_tex_original_scan_corpus = [] {
    "FindTeX preserves all 56 directly typed original scan cases"_test = [] {
        expect(retainedFindTexSourceCases.size() == 56_u);
        for (auto const& test : retainedFindTexSourceCases) {
            FindTeX finder(optionsFor(test));
            auto strings = ownStrings(test);
            expectSourceRows(test, finder.findMath(strings));
            expectSourceRows(test, finder.findMath(strings));
        }
    };
};
