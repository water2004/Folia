#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct TexHtmlDirectCase {
    std::u16string name;
    std::u16string input;
    std::u16string sourceSnapshot;
    std::u16string errorName;
    std::u16string errorId;
    std::u16string errorMessage;
    bool initialAllowTexHTML;
};

inline const auto& texhtmlDirectCases() {
    static const auto result = records<TexHtmlDirectCase>(
        "math/fixtures/packages/texhtml/direct.tsv", 7, 22, [](const auto& r) {
            return TexHtmlDirectCase{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]),
                folia::test::data::decode_utf16(r[4]),
                folia::test::data::decode_utf16(r[5]),
                boolean(r[6])};
        });
    return result;
}

struct TexHtmlPipelineCase {
    std::u16string name;
    std::u16string input;
    std::u16string actualTex;
    std::u16string serializedMml;
    std::u16string sourceSnapshot;
};

inline const auto& texhtmlPipelineCases() {
    static const auto result = records<TexHtmlPipelineCase>(
        "math/fixtures/packages/texhtml/pipeline.tsv", 5, 5, [](const auto& r) {
            return TexHtmlPipelineCase{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]),
                folia::test::data::decode_utf16(r[4])};
        });
    return result;
}

struct TexHtmlRecoveryCase {
    std::u16string input;
    std::u16string errorId;
    std::u16string errorMessage;
    std::u16string recoveredInput;
    std::u16string recoveredSnapshot;
};

inline const auto& texhtmlRecoveryCase() {
    static const auto result = records<TexHtmlRecoveryCase>(
        "math/fixtures/packages/texhtml/recovery.tsv", 5, 1, [](const auto& r) {
            return TexHtmlRecoveryCase{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]),
                folia::test::data::decode_utf16(r[4])};
        });
    return result.front();
}

inline const auto& texhtmlDomStringExpected() {
    static const auto result = records<std::u16string>(
        "math/fixtures/packages/texhtml/dom-strings.tsv", 1, 3,
        [](const auto& r) { return folia::test::data::decode_utf16(r[0]); });
    return result;
}
inline const auto& texhtmlSourceJsonSha256() {
    static const auto result = rows("math/fixtures/packages/texhtml/metadata.tsv", 1, 1)[0][0];
    return result;
}

} // namespace math_test::fixtures
