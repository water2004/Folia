#pragma once

#include "math/support/fixture_values.hpp"

namespace math_test::fixtures {

template <class HandlerKind>
struct SourceRegistration {
    HandlerKind kind;
    std::u16string map, token, owner;
    bool character;
    std::u16string text;
};

struct TexvcSourceSvg {
    std::string name;
    std::u16string tex;
    bool display;
    std::string svg;
};

template <class HandlerKind>
HandlerKind registryHandlerKind(std::string_view value) {
    if (value == "macro") return HandlerKind::macro;
    if (value == "delimiter") return HandlerKind::delimiter;
    if (value == "environment") return HandlerKind::environment;
    if (value == "character") return HandlerKind::character;
    throw std::runtime_error("Unknown fixture handler kind: " + std::string{value});
}

template <class HandlerKind>
const auto& sourceRegistrations() {
    static const auto result = records<SourceRegistration<HandlerKind>>(
        "math/fixtures/packages/registry/registrations.tsv", 6, 1763, [](const auto& r) {
            return SourceRegistration<HandlerKind>{registryHandlerKind<HandlerKind>(r[0]),
                folia::test::data::decode_utf16(r[1]), folia::test::data::decode_utf16(r[2]),
                folia::test::data::decode_utf16(r[3]), boolean(r[4]),
                folia::test::data::decode_utf16(r[5])};
        });
    return result;
}

inline const auto& sourcePackages() {
    static const auto result = [] {
        std::vector<std::u16string> values;
        for (const auto& r : rows("math/fixtures/packages/registry/packages.tsv", 2, 39)) {
            if (integer<std::size_t>(r[0]) != values.size())
                throw std::runtime_error("Package fixture profile order changed");
            values.push_back(folia::test::data::decode_utf16(r[1]));
        }
        return values;
    }();
    return result;
}

inline const auto& texvcSourceSvgs() {
    static const auto result = records<TexvcSourceSvg>(
        "math/fixtures/packages/registry/texvc-svg.tsv", 4, 216, [](const auto& r) {
            return TexvcSourceSvg{bytes(r[0]), folia::test::data::decode_utf16(r[1]),
                                   boolean(r[2]), bytes(r[3])};
        });
    return result;
}

} // namespace math_test::fixtures
