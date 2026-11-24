#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct UnicodeCreatorInput {
    std::u16string source;
    std::u16string snapshot;
};

inline const auto& unicodeCreatorInput() {
    static const auto result = records<UnicodeCreatorInput>(
        "math/fixtures/packages/unicode/creator-input.tsv", 2, 1, [](const auto& r) {
            return UnicodeCreatorInput{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1])};
        });
    return result.front();
}

struct UnicodeCreatorRequest {
    std::u16string kind;
    std::u16string text;
    std::u16string fontfamily;
};

inline const auto& unicodeCreatorRequests() {
    static const auto result = records<UnicodeCreatorRequest>(
        "math/fixtures/packages/unicode/creator-requests.tsv", 3, 1, [](const auto& r) {
            return UnicodeCreatorRequest{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

struct UnicodeCase {
    std::u16string name;
    std::u16string source;
    std::u16string snapshot;
    std::u16string error;
    std::u16string message;
};

inline const auto& unicodeCases() {
    static const auto result = records<UnicodeCase>(
        "math/fixtures/packages/unicode/cases.tsv", 5, 51, [](const auto& r) {
            return UnicodeCase{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]),
                folia::test::data::decode_utf16(r[4])};
        });
    return result;
}

struct UnicodeRegistryCase {
    std::u16string source;
    std::u16string snapshot;
    std::u16string error;
};

inline const auto& unicodeRegistryCases() {
    static const auto result = records<UnicodeRegistryCase>(
        "math/fixtures/packages/unicode/registry.tsv", 3, 6, [](const auto& r) {
            return UnicodeRegistryCase{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

struct UnicodeRecoveryCase {
    int group;
    std::u16string source;
    std::u16string snapshot;
    std::u16string error;
    std::u16string message;
};

inline const auto& unicodeRecoveryCases() {
    static const auto result = records<UnicodeRecoveryCase>(
        "math/fixtures/packages/unicode/recovery.tsv", 5, 6, [](const auto& r) {
            return UnicodeRecoveryCase{integer<int>(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]),
                folia::test::data::decode_utf16(r[4])};
        });
    return result;
}

inline const auto& unicodeCreatorSource() { return unicodeCreatorInput().source; }
inline const auto& unicodeCreatorSnapshot() { return unicodeCreatorInput().snapshot; }

} // namespace math_test::fixtures
