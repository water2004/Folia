#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

struct EntityRow {
    std::u16string file;
    std::u16string name;
    std::u16string value;
};
struct EntityShard {
    std::u16string name;
    std::size_t tableSize;
};
struct EntityNumber {
    std::u16string input;
    std::u16string output;
    bool error;
};
struct EntityTranslation {
    std::size_t module;
    bool loadMissing;
    std::u16string input;
    std::u16string output;
    bool error;
    std::vector<std::u16string> loaded;
};
struct EntityOverrideCase {
    std::u16string name;
    std::u16string shard;
    std::u16string expected;
    std::u16string afterOverrideReload;
    std::u16string afterRemoveReload;
};
struct EntityRetryCase {
    std::u16string shard;
    std::u16string name;
    std::u16string trigger;
    std::u16string input;
    std::u16string expected;
    std::vector<std::u16string> files;
};
struct EntityNumericRetryCase {
    std::u16string trigger;
    std::u16string input;
    std::vector<std::u16string> files;
};

inline const auto& entityRows() {
    static const auto result = records<EntityRow>(
        "math/fixtures/entities/rows.tsv", 3, 2128, [](const auto& r) {
            return EntityRow{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2])};
        });
    return result;
}

inline const auto& entityShards() {
    static const auto result = records<EntityShard>(
        "math/fixtures/entities/shards.tsv", 2, 29, [](const auto& r) {
            return EntityShard{folia::test::data::decode_utf16(r[0]),
                integer<std::size_t>(r[1])};
        });
    return result;
}

inline const auto& entityOverrideCases() {
    static const auto result = records<EntityOverrideCase>(
        "math/fixtures/entities/overrides.tsv", 5, 3, [](const auto& r) {
            return EntityOverrideCase{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]),
                folia::test::data::decode_utf16(r[4])};
        });
    return result;
}

inline const auto& entityRetryCases() {
    static const auto result = records<EntityRetryCase>(
        "math/fixtures/entities/retries.tsv", 7, 3, [](const auto& r) {
            return EntityRetryCase{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]),
                folia::test::data::decode_utf16(r[4]),
                utf16List(r[6], integer<std::size_t>(r[5]))};
        });
    return result;
}

inline const auto& entityNumericRetryCases() {
    static const auto result = records<EntityNumericRetryCase>(
        "math/fixtures/entities/numeric-retries.tsv", 4, 2, [](const auto& r) {
            return EntityNumericRetryCase{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                utf16List(r[3], integer<std::size_t>(r[2]))};
        });
    return result;
}

inline const auto& entityNumbers() {
    static const auto result = records<EntityNumber>(
        "math/fixtures/entities/numbers.tsv", 3, 21, [](const auto& r) {
            return EntityNumber{folia::test::data::decode_utf16(r[0]),
                folia::test::data::decode_utf16(r[1]),
                boolean(r[2])};
        });
    return result;
}

inline const auto& entityTranslations() {
    static const auto result = records<EntityTranslation>(
        "math/fixtures/entities/translations.tsv", 7, 19, [](const auto& r) {
            return EntityTranslation{integer<std::size_t>(r[0]),
                boolean(r[1]),
                folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]),
                boolean(r[4]),
                utf16List(r[6], integer<std::size_t>(r[5]))};
        });
    return result;
}

} // namespace math_test::fixtures
