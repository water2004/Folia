#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

using folia::test::data::decode_utf16;

struct MhchemCase {
    std::u16string name, input, snapshot;
    std::u16string errorName, errorId, errorMessage;
    std::u16string formattedErrorName, formattedErrorId, formattedErrorMessage;
    bool ams;
};

struct MhchemReplacementCase {
    std::u16string name, input, rules;
    std::array<std::u16string, 6> stages;
    std::u16string result;
};

struct MhchemMutationCase {
    std::string name;
    std::array<std::u16string, 6> before, during, after;
};

struct MhchemAliasCase { std::u16string input, snapshot; };

struct MhchemCreatorCase {
    std::u16string input, snapshot, kind, text, mathvariant;
    bool stretchy;
    int texClass;
};

struct MhchemRecoveryCase {
    std::u16string input, snapshot, errorName, errorId, errorMessage;
};

inline std::array<std::u16string, 6> mhchemStages(
    const std::vector<std::string>& fields, std::size_t start) {
    std::array<std::u16string, 6> result;
    for (std::size_t index = 0; index < result.size(); ++index)
        result[index] = decode_utf16(fields[start + index]);
    return result;
}

inline const auto& mhchemCases() {
    static const auto result = records<MhchemCase>(
        "math/fixtures/packages/mhchem/cases.tsv", 10, 220, [](const auto& r) {
            return MhchemCase{
                decode_utf16(r[0]), decode_utf16(r[1]), decode_utf16(r[2]), decode_utf16(r[3]),
                decode_utf16(r[4]), decode_utf16(r[5]), decode_utf16(r[6]), decode_utf16(r[7]),
                decode_utf16(r[8]), boolean(r[9])};
        });
    return result;
}

inline const auto& mhchemReplacementCases() {
    static const auto result = records<MhchemReplacementCase>(
        "math/fixtures/packages/mhchem/replacements.tsv", 10, 63, [](const auto& r) {
            return MhchemReplacementCase{
                decode_utf16(r[0]), decode_utf16(r[1]), decode_utf16(r[2]), mhchemStages(r, 3),
                decode_utf16(r[9])};
        });
    return result;
}

inline const auto& mhchemMutationInput() {
    static const auto result = records<std::u16string>(
        "math/fixtures/packages/mhchem/mutation-input.tsv", 1, 1, [](const auto& r) {
            return decode_utf16(r[0]);
        });
    return result.front();
}

inline const auto& mhchemMutationCases() {
    static const auto result = records<MhchemMutationCase>(
        "math/fixtures/packages/mhchem/mutations.tsv", 19, 14, [](const auto& r) {
            return MhchemMutationCase{bytes(r[0]), mhchemStages(r, 1), mhchemStages(r, 7), mhchemStages(r, 13)};
        });
    return result;
}

inline const auto& mhchemUntypedMapValueMutations() {
    static const auto result = records<std::string>(
        "math/fixtures/packages/mhchem/untyped-mutations.tsv", 1, 2, [](const auto& r) {
            return bytes(r[0]);
        });
    return result;
}

inline const auto& mhchemRetiredObjectApiMutations() {
    static const auto result = records<std::string>(
        "math/fixtures/packages/mhchem/retired-mutations.tsv", 1, 4, [](const auto& r) {
            return bytes(r[0]);
        });
    return result;
}

inline const auto& mhchemAliasCases() {
    static const auto result = records<MhchemAliasCase>(
        "math/fixtures/packages/mhchem/aliases.tsv", 2, 2, [](const auto& r) {
            return MhchemAliasCase{decode_utf16(r[0]), decode_utf16(r[1])};
        });
    return result;
}

inline const auto& mhchemCreatorCase() {
    static const auto result = records<MhchemCreatorCase>(
        "math/fixtures/packages/mhchem/creator.tsv", 7, 1, [](const auto& r) {
            return MhchemCreatorCase{
                decode_utf16(r[0]), decode_utf16(r[1]), decode_utf16(r[2]), decode_utf16(r[3]),
                decode_utf16(r[4]), boolean(r[5]), integer<int>(r[6])};
        });
    return result.front();
}

inline const auto& mhchemRecoveryCases() {
    static const auto result = records<MhchemRecoveryCase>(
        "math/fixtures/packages/mhchem/recovery.tsv", 5, 2, [](const auto& r) {
            return MhchemRecoveryCase{
                decode_utf16(r[0]), decode_utf16(r[1]), decode_utf16(r[2]), decode_utf16(r[3]),
                decode_utf16(r[4])};
        });
    return result;
}

} // namespace math_test::fixtures
