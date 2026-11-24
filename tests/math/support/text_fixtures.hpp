#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct ErrorFormatCase {
    std::string name;
    std::u16string format, expected;
    std::vector<std::u16string> args;
};
struct ErrorFormatObservation { std::u16string format, expected; };
struct LowercaseMapping { char32_t point; std::u16string output; };
struct LowercaseContext { char32_t first, last; unsigned flags; };
struct LowercaseExample { std::u16string input, output; };

inline const auto& focusedErrorCases() {
    static const auto result = [] {
        std::vector<ErrorFormatCase> values;
        std::vector<std::size_t> counts;
        for (const auto& r : rows("math/fixtures/text/tex-error-focused.tsv", 4, 14)) {
            values.push_back({bytes(r[0]), folia::test::data::decode_utf16(r[1]),
                              folia::test::data::decode_utf16(r[2]), {}});
            counts.push_back(integer<std::size_t>(r[3]));
        }
        const auto arguments = rows("math/fixtures/text/tex-error-focused-arguments.tsv", 3, 147);
        std::size_t cursor{};
        for (std::size_t index{}; index < values.size(); ++index) {
            for (std::size_t argument{}; argument < counts[index]; ++argument) {
                const auto& r = arguments.at(cursor++);
                if (integer<std::size_t>(r[0]) != index ||
                    integer<std::size_t>(r[1]) != argument)
                    throw std::runtime_error("TeX error fixture argument order changed");
                values[index].args.push_back(folia::test::data::decode_utf16(r[2]));
            }
        }
        if (cursor != arguments.size())
            throw std::runtime_error("Unreferenced TeX error fixture arguments");
        return values;
    }();
    return result;
}

inline const auto& exhaustiveErrorCases() {
    static const auto result = records<ErrorFormatObservation>(
        "math/fixtures/text/tex-error-exhaustive.tsv", 2, 1981, [](const auto& r) {
            return ErrorFormatObservation{folia::test::data::decode_utf16(r[0]),
                                          folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

inline const auto& exhaustiveErrorArguments() {
    static const auto result = [] {
        std::vector<std::u16string> values;
        for (const auto& r : rows("math/fixtures/text/tex-error-exhaustive-arguments.tsv", 2, 10)) {
            if (integer<std::size_t>(r[0]) != values.size())
                throw std::runtime_error("TeX exhaustive fixture argument order changed");
            values.push_back(folia::test::data::decode_utf16(r[1]));
        }
        return values;
    }();
    return result;
}

inline const auto& lowercaseMappings() {
    static const auto result = records<LowercaseMapping>(
        "math/fixtures/text/lowercase-mappings.tsv", 2, 1460, [](const auto& r) {
            return LowercaseMapping{static_cast<char32_t>(integer<unsigned>(r[0])),
                                     folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

inline const auto& lowercaseContexts() {
    static const auto result = records<LowercaseContext>(
        "math/fixtures/text/lowercase-contexts.tsv", 3, 1162, [](const auto& r) {
            return LowercaseContext{static_cast<char32_t>(integer<unsigned>(r[0])),
                                     static_cast<char32_t>(integer<unsigned>(r[1])),
                                     integer<unsigned>(r[2])};
        });
    return result;
}

inline const auto& lowercaseExamples() {
    static const auto result = records<LowercaseExample>(
        "math/fixtures/text/lowercase-examples.tsv", 2, 474, [](const auto& r) {
            return LowercaseExample{folia::test::data::decode_utf16(r[0]),
                                     folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

} // namespace math_test::fixtures
