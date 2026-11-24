#pragma once

// Included after the Bboldx modules and the shared TeX fixture.
#include "math/support/fixture_values.hpp"

struct BboldxCharacterCase {
    std::u16string map, name, glyph;
    TokenInputHandler parser;
    NodeKind kind;
    std::array<std::u16string, 4> fonts;
    bool text;
};
struct BboldxCommandCase {
    std::u16string map, name;
    MacroHandler parser;
    std::array<std::u16string, 3> arguments;
    std::size_t count;
    std::u16string glyph;
    NodeKind kind;
    std::array<std::u16string, 4> fonts;
    bool text;
};

inline NodeKind bboldxKind(std::string_view name) {
    if (name == "mi") return NodeKind::mi;
    if (name == "mo") return NodeKind::mo;
    if (name == "mtext") return NodeKind::mtext;
    throw std::runtime_error("Unknown Bboldx fixture node kind");
}
inline TokenInputHandler bboldxToken(std::string_view name) {
    if (name == "mathchar0miNormal") return &BboldxMethods::mathchar0miNormal;
    if (name == "delimiterNormal") return &BboldxMethods::delimiterNormal;
    if (name == "mathchar0miBold") return &BboldxMethods::mathchar0miBold;
    if (name == "delimiterBold") return &BboldxMethods::delimiterBold;
    throw std::runtime_error("Unknown Bboldx fixture token callback");
}
inline MacroHandler bboldxMacro(std::string_view name) {
    if (name == "Macro") return BboldxMethods::Macro;
    if (name == "ChooseFont") return &BboldxMethods::ChooseFont;
    if (name == "ChooseTextFont") return &BboldxMethods::ChooseTextFont;
    throw std::runtime_error("Unknown Bboldx fixture macro callback");
}
inline const auto& bboldxCharacters() {
    static const auto result = math_test::fixtures::records<BboldxCharacterCase>(
        "math/fixtures/packages/bboldx/characters.tsv", 10, 168, [](const auto& r) {
            using folia::test::data::decode_utf16;
            return BboldxCharacterCase{decode_utf16(r[0]), decode_utf16(r[1]),
                decode_utf16(r[2]), bboldxToken(r[3]), bboldxKind(r[4]),
                {decode_utf16(r[5]), decode_utf16(r[6]), decode_utf16(r[7]), decode_utf16(r[8])},
                math_test::fixtures::boolean(r[9])};
        });
    return result;
}
inline const auto& bboldxCommands() {
    static const auto result = math_test::fixtures::records<BboldxCommandCase>(
        "math/fixtures/packages/bboldx/commands.tsv", 14, 12, [](const auto& r) {
            using folia::test::data::decode_utf16;
            const auto count = math_test::fixtures::integer<std::size_t>(r[6]);
            if (count > 3) throw std::runtime_error("Invalid Bboldx fixture argument count");
            return BboldxCommandCase{decode_utf16(r[0]), decode_utf16(r[1]), bboldxMacro(r[2]),
                {decode_utf16(r[3]), decode_utf16(r[4]), decode_utf16(r[5])}, count,
                decode_utf16(r[7]), bboldxKind(r[8]),
                {decode_utf16(r[9]), decode_utf16(r[10]), decode_utf16(r[11]), decode_utf16(r[12])},
                math_test::fixtures::boolean(r[13])};
        });
    return result;
}
