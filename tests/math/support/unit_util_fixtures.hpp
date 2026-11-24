#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

template <class DimensionMatch>
struct UnitUtilFixtureData {
    using Dimension = std::optional<DimensionMatch>;
    struct DefaultCase { std::u16string unit; double factor; Dimension match; double em; };
    struct MatchCase { std::u16string input; bool rest; Dimension match; double em; };
    struct FormatCase { double input; std::string output; };
    struct TrimCase { std::u16string input, output; };

private:
    static Dimension dimension(const auto& r) {
        if (boolean(r[2])) return DimensionMatch{folia::test::data::decode_utf16(r[3]),
            folia::test::data::decode_utf16(r[4]), integer<std::size_t>(r[5])};
        if (!r[3].empty() || !r[4].empty() || r[5] != "0")
            throw std::runtime_error("Absent dimension reference has a value");
        return std::nullopt;
    }
    static double number(std::string_view field) { return std::bit_cast<double>(binary64(field)); }

public:
    static const auto& default_cases() {
        static const auto result = records<DefaultCase>(
            "math/fixtures/input/unit-util/defaults.tsv", 7, 9, [](const auto& r) {
                return DefaultCase{folia::test::data::decode_utf16(r[0]), number(r[1]),
                    dimension(r), number(r[6])};
            });
        return result;
    }
    static const auto& match_cases() {
        static const auto result = records<MatchCase>(
            "math/fixtures/input/unit-util/matches.tsv", 7, 29, [](const auto& r) {
                return MatchCase{folia::test::data::decode_utf16(r[0]), boolean(r[1]),
                    dimension(r), number(r[6])};
            });
        return result;
    }
    static const auto& format_cases() {
        static const auto result = records<FormatCase>(
            "math/fixtures/input/unit-util/formats.tsv", 2, 23, [](const auto& r) {
                return FormatCase{number(r[0]), bytes(r[1])};
            });
        return result;
    }
    static const auto& trim_cases() {
        static const auto result = records<TrimCase>(
            "math/fixtures/input/unit-util/trims.tsv", 2, 37, [](const auto& r) {
                return TrimCase{folia::test::data::decode_utf16(r[0]),
                    folia::test::data::decode_utf16(r[1])};
            });
        return result;
    }
};

} // namespace math_test::fixtures
