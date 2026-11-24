#pragma once

#include "math/support/tex_binding_fixtures.hpp"

inline MacroHandler textMappingMethod(std::string_view name) {
    if (name == "Math") return &TextMacrosMethods::Math;
    if (name == "Comment") return &TextMacrosMethods::Comment;
    if (name == "MathModeOnly") return &TextMacrosMethods::MathModeOnly;
    if (name == "Misplaced") return &TextMacrosMethods::Misplaced;
    if (name == "Tilde") return &TextMacrosMethods::Tilde;
    if (name == "Space") return &TextMacrosMethods::Space;
    if (name == "OpenBrace") return &TextMacrosMethods::OpenBrace;
    if (name == "CloseBrace") return &TextMacrosMethods::CloseBrace;
    if (name == "OpenQuote") return &TextMacrosMethods::OpenQuote;
    if (name == "CloseQuote") return &TextMacrosMethods::CloseQuote;
    if (name == "SelfQuote") return &TextMacrosMethods::SelfQuote;
    if (name == "Macro") return TextMacrosMethods::Macro;
    if (name == "Accent") return &TextMacrosMethods::Accent;
    if (name == "Emph") return &TextMacrosMethods::Emph;
    if (name == "SetFont") return &TextMacrosMethods::SetFont;
    if (name == "TextFont") return &TextMacrosMethods::TextFont;
    if (name == "SetSize") return &TextMacrosMethods::SetSize;
    if (name == "Insert") return &TextMacrosMethods::Insert;
    if (name == "Spacer") return TextMacrosMethods::Spacer;
    if (name == "Hskip") return TextMacrosMethods::Hskip;
    if (name == "rule") return TextMacrosMethods::rule;
    if (name == "Rule") return TextMacrosMethods::Rule;
    if (name == "CheckAutoload") return &TextMacrosMethods::CheckAutoload;
    if (name == "HandleRef") return TextMacrosMethods::HandleRef;
    if (name == "UnderOver") return TextMacrosMethods::UnderOver;
    if (name == "Lap") return TextMacrosMethods::Lap;
    if (name == "Phantom") return TextMacrosMethods::Phantom;
    if (name == "Smash") return TextMacrosMethods::Smash;
    if (name == "MmlToken") return TextMacrosMethods::MmlToken;
    throw std::runtime_error("Unknown TextMacros fixture callback");
}
inline const auto& textMappingCases() {
    static const auto result = [] {
        auto arguments = math_test::fixtures::bindingArguments<TexArgument>(
            "math/fixtures/packages/textmacros/mapping-arguments.tsv", 74, 116);
        std::size_t index{};
        return math_test::fixtures::records<TextMappingCase>(
            "math/fixtures/packages/textmacros/mappings.tsv", 4, 116, [&](const auto& r) {
                if (math_test::fixtures::integer<std::size_t>(r[3]) != arguments.at(index).size())
                    throw std::runtime_error("TextMacros fixture argument count changed");
                return TextMappingCase{folia::test::data::decode_utf16(r[0]),
                    folia::test::data::decode_utf16(r[1]), textMappingMethod(r[2]),
                    std::move(arguments[index++])};
            });
    }();
    return result;
}
