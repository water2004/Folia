#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

enum class LiteSourceFixtureKind { parse, query, mutation, style, serialization, measurement, entity };
struct LiteSourceFixture {
    std::u16string id;
    LiteSourceFixtureKind kind;
    std::u16string html;
    std::u16string format;
    std::u16string text;
    std::u16string path;
    std::u16string rawInput;
    std::u16string expectedText;
    std::u16string expectedDocument;
    std::size_t selectorOffset;
    std::size_t selectorCount;
    std::u16string expectedObservation;
};
struct LiteSourceSelector { std::u16string value; };
struct LiteSourceMutationStep { std::u16string name; std::u16string expected; };
struct LiteSourceMetric { double fontSize; std::u16string fontFamily; double width; double height; double left; double top; double right; double bottom; };
struct LiteSourceLoadStep { std::u16string recordId; std::size_t order; std::u16string file; };
struct LiteSourceTextError { bool throws; std::u16string name, message; };
struct LiteSourceCounts { std::size_t fixtures, mutations; };

inline LiteSourceFixtureKind liteKind(std::string_view name) {
    constexpr std::array names{"parse", "query", "mutation", "style", "serialization", "measurement", "entity"};
    for (std::size_t i{}; i < names.size(); ++i)
        if (name == names[i]) return static_cast<LiteSourceFixtureKind>(i);
    throw std::runtime_error("Unknown Lite reference case kind: " + std::string{name});
}

inline const auto& liteSourceSelectors() {
    static const auto result = records<LiteSourceSelector>(
        "math/fixtures/adaptors/lite/selectors.tsv", 1, 9, [](const auto& r) {
            return LiteSourceSelector{folia::test::data::decode_utf16(r[0])};
        });
    return result;
}

inline const auto& liteSourceFixtures() {
    static const auto result = records<LiteSourceFixture>(
        "math/fixtures/adaptors/lite/cases.tsv", 12, 41, [](const auto& r) {
            return LiteSourceFixture{folia::test::data::decode_utf16(r[0]),
                liteKind(r[1]),
                folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]),
                folia::test::data::decode_utf16(r[4]),
                folia::test::data::decode_utf16(r[5]),
                folia::test::data::decode_utf16(r[6]),
                folia::test::data::decode_utf16(r[7]),
                folia::test::data::decode_utf16(r[8]),
                integer<std::size_t>(r[9]),
                integer<std::size_t>(r[10]),
                folia::test::data::decode_utf16(r[11])};
        });
    return result;
}

inline const auto& liteSourceMutationSteps() {
    static const auto result = records<LiteSourceMutationStep>(
        "math/fixtures/adaptors/lite/mutations.tsv", 2, 14, [](const auto& r) {
            return LiteSourceMutationStep{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

inline const auto& liteSourceElementMetric() {
    static const auto result = records<LiteSourceMetric>(
        "math/fixtures/adaptors/lite/element-metric.tsv", 8, 1, [](const auto& r) {
            return LiteSourceMetric{std::bit_cast<double>(binary64(r[0])),
                folia::test::data::decode_utf16(r[1]),
                std::bit_cast<double>(binary64(r[2])),
                std::bit_cast<double>(binary64(r[3])),
                std::bit_cast<double>(binary64(r[4])),
                std::bit_cast<double>(binary64(r[5])),
                std::bit_cast<double>(binary64(r[6])),
                std::bit_cast<double>(binary64(r[7]))};
        });
    return result.front();
}

inline const auto& liteSourceEntityLoadSteps() {
    static const auto result = records<LiteSourceLoadStep>(
        "math/fixtures/adaptors/lite/entity-loads.tsv", 3, 4, [](const auto& r) {
            return LiteSourceLoadStep{folia::test::data::decode_utf16(r[0]),
                integer<std::size_t>(r[1]),
                folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

inline const auto& liteSourceTextError() {
    static const auto result = records<LiteSourceTextError>(
        "math/fixtures/adaptors/lite/text-error.tsv", 3, 1, [](const auto& r) {
            return LiteSourceTextError{boolean(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2])};
        });
    return result.front();
}

inline const auto& liteSourceCounts() {
    static const auto result = records<LiteSourceCounts>(
        "math/fixtures/adaptors/lite/counts.tsv", 2, 1, [](const auto& r) {
            return LiteSourceCounts{integer<std::size_t>(r[0]),
                integer<std::size_t>(r[1])};
        });
    return result.front();
}

inline std::size_t liteSourceFixtureCount() { return liteSourceCounts().fixtures; }
inline std::size_t liteSourceMutationStepCount() { return liteSourceCounts().mutations; }
inline bool liteSourceTextNodeSizeThrows() { return liteSourceTextError().throws; }
inline const auto& liteSourceTextNodeSizeErrorName() { return liteSourceTextError().name; }
inline const auto& liteSourceTextNodeSizeErrorMessage() { return liteSourceTextError().message; }

} // namespace math_test::fixtures
