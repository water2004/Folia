#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

template <class MathDisplay>
struct FindTexFixtureData {
    struct FindTexExpected {
        std::u16string math, open, close;
        std::size_t n, start, end;
        MathDisplay display;
    };
    struct FindTexCase {
        std::u16string name;
        std::vector<std::u16string> strings;
        std::vector<std::array<std::u16string, 2>> inlineMath, displayMath;
        bool processEscapes, processEnvironments, processRefs;
        std::vector<FindTexExpected> expected;
    };

private:
    static auto pairs(std::string_view field, std::string_view count) {
        const auto size = integer<std::size_t>(count);
        const auto parts = utf16List(field, size * 2);
        std::vector<std::array<std::u16string, 2>> result;
        for (std::size_t i{}; i < size; ++i)
            result.push_back({parts[i * 2], parts[i * 2 + 1]});
        return result;
    }
    static MathDisplay display(std::string_view value) {
        if (value == "escaped") return MathDisplay::escaped;
        if (value == "inline_math") return MathDisplay::inline_math;
        if (value == "display_math") return MathDisplay::display_math;
        throw std::runtime_error("Unknown FindTeX reference display mode");
    }
    static auto load(std::string_view prefix, std::size_t count, std::size_t expectedCount) {
        const auto base = "math/fixtures/input/find-tex/" + std::string{prefix};
        std::vector<FindTexCase> result;
        std::vector<std::size_t> sizes;
        for (const auto& r : rows(base + "-cases.tsv", 11, count)) {
            result.push_back({folia::test::data::decode_utf16(r[0]),
                utf16List(r[2], integer<std::size_t>(r[1])), pairs(r[4], r[3]), pairs(r[6], r[5]),
                boolean(r[7]), boolean(r[8]), boolean(r[9]), {}});
            sizes.push_back(integer<std::size_t>(r[10]));
        }
        for (const auto& r : rows(base + "-expected.tsv", 9, expectedCount)) {
            const auto index = integer<std::size_t>(r[0]);
            if (index >= result.size() || integer<std::size_t>(r[1]) != result[index].expected.size())
                throw std::runtime_error("FindTeX reference output order changed");
            result[index].expected.push_back({folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]), folia::test::data::decode_utf16(r[4]),
                integer<std::size_t>(r[5]), integer<std::size_t>(r[6]), integer<std::size_t>(r[7]),
                display(r[8])});
        }
        for (std::size_t i{}; i < result.size(); ++i)
            if (result[i].expected.size() != sizes[i])
                throw std::runtime_error("Incomplete FindTeX reference output");
        return result;
    }

public:
    static const auto& findTexCases() {
        static const auto result = load("source", 22, 59);
        return result;
    }
    static const auto& retainedFindTexSourceCases() {
        static const auto result = load("retained", 56, 60);
        return result;
    }
};

} // namespace math_test::fixtures
