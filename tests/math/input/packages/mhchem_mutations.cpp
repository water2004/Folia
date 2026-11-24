#include "math/support/mhchem_test.hpp"

suite mhchem_mutations_behavior = [] {
    "Mhchem typed map mutations match Source before during and after"_test = [] {
        constexpr std::array<std::string_view, 8> exercised{
            "ce-macro-replaced", "character-parser-replaced", "character-arrow-removed",
            "character-arrow-custom", "macro-arrow-removed",
            "character-false-macro-fallback", "registry-maps-replaced",
            "registry-shadow-added-arrow"};
        const std::array<std::string_view, 6> sourceOnly{
            mhchemUntypedMapValueMutations()[0], mhchemUntypedMapValueMutations()[1],
            mhchemRetiredObjectApiMutations()[0], mhchemRetiredObjectApiMutations()[1],
            mhchemRetiredObjectApiMutations()[2], mhchemRetiredObjectApiMutations()[3]};
        expect(std::size(mhchemMutationCases()) == 14_u);
        const auto rules = mhchemReplacements();
        const auto stages = [&](TokenMap const &macros, TokenMap const &characters) {
            std::array<std::u16string, 6> output;
            std::u16string current(mhchemMutationInput());
            for (std::size_t i = 0; i < rules.size(); ++i) {
                current = applyMhchemReplacement(std::move(current), rules[i], macros, characters);
                output[i] = current;
            }
            return output;
        };
        const auto compare = [](auto const &actual, auto const &expected,
                                std::string_view name, std::string_view phase) {
            for (std::size_t i = 0; i < actual.size(); ++i)
                expect(actual[i] == expected[i])
                    << std::string(name) << " " << phase << " replacement stage " << i;
        };
        const auto run = [&](std::string_view name) {
            const auto entry = std::ranges::find(mhchemMutationCases(), name,
                                                 &MhchemMutationCase::name);
            expect(entry != std::end(mhchemMutationCases())) << std::string(name) << " fixture row";
            if (entry == std::end(mhchemMutationCases()))
                return;
            MhchemFixture fixture(true);
            auto originalMacros = std::dynamic_pointer_cast<CommandMap>(
                fixture.maps.getMap(u"mhchem"));
            auto originalCharacters = std::dynamic_pointer_cast<CharacterMap>(
                fixture.maps.getMap(u"mhchem-chars"));
            expect(originalMacros != nullptr) << std::string(name) << " original macro map";
            expect(originalCharacters != nullptr) << std::string(name) << " original character map";
            if (!originalMacros || !originalCharacters)
                return;
            compare(stages(*originalMacros, *originalCharacters), entry->before, name, "before");

            const auto oldParser = originalCharacters->parser();
            const auto oldCe = originalMacros->lookup(u"ce");
            const auto oldArrow = originalMacros->lookup(u"mhchemxrightarrow");
            const auto oldCharacter = originalCharacters->lookup(u"mhchemrightarrow");
            const auto macroFrom = [](TokenLookup const &value) -> Macro const * {
                if (const auto *macro = std::get_if<Macro const *>(&value))
                    return *macro;
                return nullptr;
            };
            const auto tokenFrom = [](TokenLookup const &value) -> Token const * {
                if (const auto *token = std::get_if<Token const *>(&value))
                    return *token;
                return nullptr;
            };
            const auto *ce = macroFrom(oldCe);
            const auto *arrow = macroFrom(oldArrow);
            const auto *character = tokenFrom(oldCharacter);
            expect(ce && arrow && character) << std::string(name) << " original map entries";
            if (!ce || !arrow || !character)
                return;
            const Macro savedCe = *ce;
            const Macro savedArrow = *arrow;
            const Token savedCharacter = *character;

            if (name == "ce-macro-replaced") {
                originalMacros->add(Macro(u"ce", MacroMethod{&BaseMethods::Macro},
                                          {std::u16string(u"observer replacement"), 0.0}));
            } else if (name == "character-parser-replaced") {
                originalCharacters->setParser(TokenMapParser{MacroHandler{&BaseMethods::Macro}});
            } else if (name == "character-arrow-removed") {
                originalCharacters->remove(u"mhchemrightarrow");
            } else if (name == "character-arrow-custom") {
                AttributeSet attributes;
                attributes.set(u"stretchy", false);
                originalCharacters->add(Token(u"mhchemrightarrow", u"Z", std::move(attributes)));
            } else if (name == "macro-arrow-removed") {
                originalMacros->remove(u"mhchemxrightarrow");
            } else if (name == "character-false-macro-fallback") {
                originalCharacters->remove(u"mhchemrightarrow");
                originalMacros->add(Macro(u"mhchemrightarrow", MacroMethod{&BaseMethods::Macro},
                                          {std::u16string(u"fallback"), 0.0}));
            } else if (name == "registry-maps-replaced") {
                std::vector<Macro> observerMacros{
                    Macro(u"observerOnly", MacroMethod{&BaseMethods::Macro},
                          {std::u16string(u"Q"), 0.0}),
                    Macro(u"mhchemxrightarrow", MacroMethod{&BaseMethods::Macro},
                          {std::u16string(u"R"), 0.0})};
                fixture.maps.registerMap(std::make_shared<CommandMap>(
                    CommandMapInit{MacroMapInit{{u"mhchem", std::nullopt},
                                                std::move(observerMacros)}}));
                std::vector<Token> observerCharacters{
                    Token(u"mhchemleftarrow", u"L"), Token(u"observerOnly", u"Y")};
                fixture.maps.registerMap(std::make_shared<CharacterMap>(
                    CharacterMapInit{{u"mhchem-chars", std::nullopt},
                                     std::move(observerCharacters)}));
            } else if (name == "registry-shadow-added-arrow") {
                originalCharacters->remove(u"mhchemrightarrow");
                originalMacros->remove(u"mhchemxrightarrow");
                std::vector<Macro> observerMacros{
                    Macro(u"mhchemxrightarrow", MacroMethod{&BaseMethods::Macro},
                          {std::u16string(u"R"), 0.0})};
                fixture.maps.registerMap(std::make_shared<CommandMap>(
                    CommandMapInit{MacroMapInit{{u"mhchem", std::nullopt},
                                                std::move(observerMacros)}}));
                std::vector<Token> observerCharacters{Token(u"mhchemrightarrow", u"R")};
                fixture.maps.registerMap(std::make_shared<CharacterMap>(
                    CharacterMapInit{{u"mhchem-chars", std::nullopt},
                                     std::move(observerCharacters)}));
            }
            const auto currentCe = macroFrom(originalMacros->lookup(u"ce"));
            const auto currentMacroArrow = macroFrom(originalMacros->lookup(u"mhchemxrightarrow"));
            const auto currentCharacter =
                tokenFrom(originalCharacters->lookup(u"mhchemrightarrow"));
            if (name == "ce-macro-replaced")
                expect(currentCe && std::get<MacroHandler>(currentCe->function) ==
                                         &BaseMethods::Macro)
                    << std::string(name) << " replacement handler";
            if (name == "character-parser-replaced") {
                const auto parser = originalCharacters->parser();
                const auto *handler = parser ? std::get_if<MacroHandler>(&*parser) : nullptr;
                expect(handler && *handler == &BaseMethods::Macro)
                    << std::string(name) << " replacement parser alias";
            }
            if (name == "character-arrow-removed" || name == "registry-shadow-added-arrow")
                expect(currentCharacter == nullptr) << std::string(name) << " removed token";
            if (name == "character-arrow-custom")
                expect(currentCharacter && currentCharacter->character == u"Z" &&
                       currentCharacter->attributes &&
                       attribute(*currentCharacter->attributes, u"stretchy") == u"false")
                    << std::string(name) << " custom token";
            if (name == "macro-arrow-removed" || name == "registry-shadow-added-arrow")
                expect(currentMacroArrow == nullptr) << std::string(name) << " removed macro";
            if (name == "character-false-macro-fallback") {
                expect(currentCharacter == nullptr) << std::string(name) << " character absent";
                expect(currentMacroArrow && currentMacroArrow->token == u"mhchemxrightarrow")
                    << std::string(name) << " source arrow macro";
                expect(macroFrom(originalMacros->lookup(u"mhchemrightarrow")) != nullptr)
                    << std::string(name) << " fallback macro";
            }
            if (name == "registry-maps-replaced" || name == "registry-shadow-added-arrow") {
                const auto registryMacros = fixture.maps.getMap(u"mhchem");
                const auto registryCharacters = fixture.maps.getMap(u"mhchem-chars");
                expect(registryMacros.get() != originalMacros.get())
                    << std::string(name) << " registry macro owner";
                expect(registryCharacters.get() != originalCharacters.get())
                    << std::string(name) << " registry character owner";
                if (name == "registry-maps-replaced") {
                    expect(macroFrom(registryMacros->lookup(u"observerOnly")) != nullptr)
                        << std::string(name) << " observer macro";
                    expect(macroFrom(registryMacros->lookup(u"mhchemxrightarrow")) != nullptr)
                        << std::string(name) << " shadow macro";
                    expect(tokenFrom(registryCharacters->lookup(u"mhchemleftarrow")) != nullptr)
                        << std::string(name) << " shadow token";
                } else {
                    const auto *shadowMacro = macroFrom(
                        registryMacros->lookup(u"mhchemxrightarrow"));
                    const auto *shadowToken = tokenFrom(
                        registryCharacters->lookup(u"mhchemrightarrow"));
                    expect(shadowMacro && shadowMacro->token == u"mhchemxrightarrow")
                        << std::string(name) << " registry shadow macro";
                    expect(shadowToken && shadowToken->character == u"R")
                        << std::string(name) << " registry shadow token";
                }
            }
            compare(stages(*originalMacros, *originalCharacters), entry->during, name, "during");

            if (name == "ce-macro-replaced")
                originalMacros->add(savedCe);
            else if (name == "character-parser-replaced")
                originalCharacters->setParser(oldParser);
            else if (name == "character-arrow-removed" || name == "character-arrow-custom" ||
                     name == "character-false-macro-fallback" ||
                     name == "registry-shadow-added-arrow")
                originalCharacters->add(savedCharacter);
            if (name == "macro-arrow-removed" || name == "registry-shadow-added-arrow")
                originalMacros->add(savedArrow);
            if (name == "character-false-macro-fallback")
                originalMacros->remove(u"mhchemrightarrow");
            if (name == "registry-maps-replaced" || name == "registry-shadow-added-arrow") {
                fixture.maps.registerMap(originalCharacters);
                fixture.maps.registerMap(originalMacros);
            }
            const auto restoredCe = macroFrom(originalMacros->lookup(u"ce"));
            const auto restoredParser = originalCharacters->parser();
            const auto *restoredTokenParser =
                restoredParser ? std::get_if<TokenInputHandler>(&*restoredParser) : nullptr;
            expect(restoredCe && std::get<MacroHandler>(restoredCe->function) ==
                                     &MhchemMethods::Machine)
                << std::string(name) << " restored ce alias";
            expect(restoredTokenParser && *restoredTokenParser == &MhchemUtils::relmo)
                << std::string(name) << " restored character parser";
            expect(macroFrom(originalMacros->lookup(u"mhchemxrightarrow")) != nullptr)
                << std::string(name) << " restored arrow macro";
            expect(tokenFrom(originalCharacters->lookup(u"mhchemrightarrow")) != nullptr)
                << std::string(name) << " restored arrow token";
            compare(stages(*originalMacros, *originalCharacters), entry->after, name, "after");
        };
        for (const auto name : exercised)
            run(name);
        for (const auto name : sourceOnly) {
            const auto entry = std::ranges::find(mhchemMutationCases(), name,
                                                 &MhchemMutationCase::name);
            expect(entry != std::end(mhchemMutationCases()))
                << std::string(name) << " Source-only fixture row";
            if (entry != std::end(mhchemMutationCases())) {
                expect(entry->before == entry->after)
                    << std::string(name) << " Source-only restored stages";
                if (name == "character-arrow-false" || name == "character-arrow-null")
                    expect(entry->during != entry->before)
                        << std::string(name) << " Source-only changed stages";
                else
                    expect(entry->during == entry->before)
                        << std::string(name) << " retired object API stages";
            }
        }
    };
};
