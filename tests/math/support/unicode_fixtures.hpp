#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct InputHostUtf16Case {
    std::u16string text;
    std::vector<std::optional<char32_t>> at;
    std::vector<char32_t> points;
    std::vector<unsigned> utf8;
};
struct InputHostDecodeCase {
    std::string bytes;
    std::u16string text;
    std::vector<std::size_t> offsets;
};
struct InputHostInvalidCase { std::string bytes; std::size_t byteOffset; };
struct SourceCaseGroup {
    char16_t canonical;
    unsigned count;
    std::array<char16_t, 4> members;
    std::array<unsigned, 4> indices, last_indices;
};
struct NormalizationSequence { std::u16string input, output; };

template <class Value, class Decode>
std::vector<Value> unicodeList(std::string_view value, std::string_view count, Decode decode) {
    std::vector<Value> result;
    if (!value.empty()) {
        std::size_t start{};
        for (;;) {
            const auto end = value.find(',', start);
            result.push_back(decode(value.substr(start, end == std::string_view::npos ? end : end - start)));
            if (end == std::string_view::npos) break;
            start = end + 1;
        }
    }
    if (result.size() != integer<std::size_t>(count))
        throw std::runtime_error("Unicode fixture list length changed");
    return result;
}

inline char16_t unicodeUnit(std::string_view value) {
    const auto unit = integer<unsigned>(value);
    if (unit > 0xffff) throw std::runtime_error("Fixture value is outside UTF-16");
    return static_cast<char16_t>(unit);
}

inline const auto& inputHostUtf16Cases() {
    static const auto result = records<InputHostUtf16Case>(
        "math/fixtures/text/input-utf16.tsv", 7, 112, [](const auto& r) {
            return InputHostUtf16Case{folia::test::data::decode_utf16(r[0]),
                unicodeList<std::optional<char32_t>>(r[2], r[1], [](std::string_view value) {
                    return value == "null" ? std::nullopt :
                        std::optional{static_cast<char32_t>(integer<unsigned>(value))};
                }),
                unicodeList<char32_t>(r[4], r[3], [](auto value) {
                    return static_cast<char32_t>(integer<unsigned>(value));
                }),
                unicodeList<unsigned>(r[6], r[5], [](auto value) { return integer<unsigned>(value); })};
        });
    return result;
}

inline const auto& inputHostDecodeCases() {
    static const auto result = records<InputHostDecodeCase>(
        "math/fixtures/text/input-decode.tsv", 4, 68, [](const auto& r) {
            return InputHostDecodeCase{bytes(r[0]), folia::test::data::decode_utf16(r[1]),
                unicodeList<std::size_t>(r[3], r[2], [](auto value) { return integer<std::size_t>(value); })};
        });
    return result;
}

inline const auto& inputHostInvalidCases() {
    static const auto result = records<InputHostInvalidCase>(
        "math/fixtures/text/input-invalid.tsv", 2, 60, [](const auto& r) {
            return InputHostInvalidCase{bytes(r[0]), integer<std::size_t>(r[1])};
        });
    return result;
}

inline const auto& regexpCaseGroups() {
    static const auto result = records<SourceCaseGroup>(
        "math/fixtures/text/regexp-case-groups.tsv", 14, 1141, [](const auto& r) {
            SourceCaseGroup group{unicodeUnit(r[0]), integer<unsigned>(r[1]), {}, {}, {}};
            if (group.count > group.members.size())
                throw std::runtime_error("RegExp fixture member count exceeds its fixed slots");
            for (std::size_t index{}; index < 4; ++index) {
                group.members[index] = unicodeUnit(r[2 + index]);
                group.indices[index] = integer<unsigned>(r[6 + index]);
                group.last_indices[index] = integer<unsigned>(r[10 + index]);
            }
            return group;
        });
    return result;
}

inline std::size_t normalizationCorpusRows() {
    const auto table = rows("math/fixtures/unicode/normalization-metadata.tsv", 2, 1);
    if (table[0][0] != "corpus_rows")
        throw std::runtime_error("Unknown normalization fixture metadata");
    return integer<std::size_t>(table[0][1]);
}

inline const auto& normalizationSequences() {
    static const auto result = records<NormalizationSequence>(
        "math/fixtures/unicode/normalization-sequences.tsv", 2, 85, [](const auto& r) {
            return NormalizationSequence{folia::test::data::decode_utf16(r[0]),
                                          folia::test::data::decode_utf16(r[1])};
        });
    return result;
}

inline const auto& normalizationPlaneDigests() {
    static const auto result = [] {
        std::vector<std::uint64_t> values;
        for (const auto& r : rows("math/fixtures/unicode/normalization-digests.tsv", 2, 17)) {
            if (integer<std::size_t>(r[0]) != values.size())
                throw std::runtime_error("Normalization fixture plane order changed");
            values.push_back(integer<std::uint64_t>(r[1], 16));
        }
        return values;
    }();
    return result;
}

} // namespace math_test::fixtures
