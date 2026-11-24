#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct NumberFormatRow {
    std::uint64_t bits;
    unsigned digits;
    std::string shortest, fixed;
};
struct TextLengthRow {
    std::string input;
    std::uint64_t size, scale, pixels, expected;
};
struct NumberLengthRow { std::uint64_t input, size, scale, pixels, expected; };
struct FormattedLengthRow { bool percent; std::uint64_t input; std::string expected; };
struct PixelRow { std::uint64_t input, minimum, pixels; std::string expected; };
struct UnitRow { std::string input; bool physical; std::uint64_t expected; };
struct ReductionRow {
    std::vector<std::uint64_t> input;
    bool sumNan;
    std::uint64_t sum;
    bool maxNan;
    std::uint64_t max;
};

template <class Row, class Make>
std::vector<Row> numericRows(std::string_view file, std::size_t columns,
                             std::size_t count, Make make) {
    std::vector<Row> result;
    for (const auto& row : rows("math/fixtures/numeric/" + std::string{file}, columns, count))
        result.push_back(make(row));
    return result;
}

inline auto numberFormats() {
    return numericRows<NumberFormatRow>("number-format.tsv", 4, 6412, [](const auto& r) {
        return NumberFormatRow{binary64(r[0]), integer<unsigned>(r[1]), bytes(r[2]), bytes(r[3])};
    });
}

inline auto textLengths() {
    return numericRows<TextLengthRow>("text-length.tsv", 5, 520, [](const auto& r) {
        return TextLengthRow{bytes(r[0]), binary64(r[1]), binary64(r[2]), binary64(r[3]), binary64(r[4])};
    });
}

inline auto numberLengths() {
    return numericRows<NumberLengthRow>("number-length.tsv", 5, 88, [](const auto& r) {
        return NumberLengthRow{binary64(r[0]), binary64(r[1]), binary64(r[2]), binary64(r[3]), binary64(r[4])};
    });
}

inline auto formattedLengths() {
    return numericRows<FormattedLengthRow>("formatted-length.tsv", 3, 512, [](const auto& r) {
        return FormattedLengthRow{boolean(r[0]), binary64(r[1]), bytes(r[2])};
    });
}

inline auto pixels() {
    return numericRows<PixelRow>("pixel.tsv", 4, 1024, [](const auto& r) {
        return PixelRow{binary64(r[0]), binary64(r[1]), binary64(r[2]), bytes(r[3])};
    });
}

inline auto units() {
    return numericRows<UnitRow>("unit.tsv", 3, 30, [](const auto& r) {
        return UnitRow{bytes(r[0]), boolean(r[1]), binary64(r[2])};
    });
}

inline auto reductions() {
    return numericRows<ReductionRow>("reduction.tsv", 5, 145, [](const auto& r) {
        ReductionRow result{{}, boolean(r[1]), binary64(r[2]), boolean(r[3]), binary64(r[4])};
        std::string_view values{r[0]};
        while (!values.empty()) {
            const auto end = values.find(',');
            result.input.push_back(binary64(values.substr(0, end)));
            if (end == std::string_view::npos) break;
            values.remove_prefix(end + 1);
            if (values.empty()) throw std::runtime_error("Trailing comma in numeric fixture");
        }
        return result;
    });
}

} // namespace math_test::fixtures
