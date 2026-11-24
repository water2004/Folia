#include "math/support/mhchem_test.hpp"

suite mhchem_behavior = [] {
    "Mhchem original Source MML and TexErrors match every package case"_test = [] {
        expect(std::size(mhchemCases()) == 220_u);
        for (const auto &entry : mhchemCases()) {
            MhchemFixture fixture(entry.ams);
            auto *tree = fixture.compile(entry.input);
            expect(tree != nullptr) << folia::math::utf16::encode(entry.name);
            if (tree) {
                const auto actual = snapshot(*tree);
                if (actual != entry.snapshot)
                    reportSnapshotDifference(entry.name, actual, entry.snapshot);
                expect(actual == entry.snapshot) << folia::math::utf16::encode(entry.name);
            }
            expect(fixture.error.name == narrow(entry.formattedErrorName))
                << folia::math::utf16::encode(entry.name);
            expect(fixture.error.id == narrow(entry.formattedErrorId))
                << folia::math::utf16::encode(entry.name);
            expect(fixture.error.message == entry.formattedErrorMessage)
                << folia::math::utf16::encode(entry.name);
            expect(fixture.jax->parseOptions().request().parsers.empty())
                << folia::math::utf16::encode(entry.name);
        }
    };

    "Mhchem six ordered replacements match every Source stage result"_test = [] {
        ConfigurationHandler registry;
        MapHandler maps;
        const auto config = MhchemConfiguration::initializeModule(registry, maps);
        const auto macros = maps.getMap(u"mhchem");
        const auto chars = maps.getMap(u"mhchem-chars");
        expect(config != nullptr);
        expect(macros != nullptr);
        expect(chars != nullptr);
        if (!macros || !chars)
            return;
        expect(std::size(mhchemReplacementCases()) == 63_u);
        const auto rules = mhchemReplacements();
        for (const auto &entry : mhchemReplacementCases()) {
            std::u16string stage(entry.input);
            for (std::size_t i = 0; i < rules.size(); ++i) {
                stage = applyMhchemReplacement(std::move(stage), rules[i], *macros, *chars);
                expect(stage == entry.stages[i])
                    << folia::math::utf16::encode(entry.name);
            }
            expect(stage == entry.result) << folia::math::utf16::encode(entry.name);
        }

        constexpr auto input = u"\\rightarrow+\\xrightarrow"sv;
        expect(applyMhchemReplacements(std::u16string(input), *macros, *chars) ==
               u"\\mhchemrightarrow+\\mhchemxrightarrow");

        const auto originalCommand = std::dynamic_pointer_cast<CommandMap>(macros);
        const auto originalCharacters = std::dynamic_pointer_cast<CharacterMap>(chars);
        expect(originalCommand != nullptr);
        expect(originalCharacters != nullptr);
        if (!originalCommand || !originalCharacters)
            return;
        const auto charValue = originalCharacters->lookup(u"mhchemrightarrow");
        const auto *charToken = std::get_if<Token const *>(&charValue);
        const auto macroValue = originalCommand->lookup(u"mhchemxrightarrow");
        const auto *macroToken = std::get_if<Macro const *>(&macroValue);
        expect(charToken && *charToken);
        expect(macroToken && *macroToken);
        if (!charToken || !*charToken || !macroToken || !*macroToken)
            return;
        const Token savedCharacter = **charToken;
        const Macro savedArrow = **macroToken;

        originalCharacters->remove(u"mhchemrightarrow");
        expect(applyMhchemReplacements(std::u16string(input), *originalCommand,
                                       *originalCharacters) ==
               u"\\rightarrow+\\mhchemxrightarrow");
        originalCharacters->add(savedCharacter);
        originalCommand->remove(u"mhchemxrightarrow");
        expect(applyMhchemReplacements(std::u16string(input), *originalCommand,
                                       *originalCharacters) ==
               u"\\mhchemrightarrow+\\xrightarrow");
        originalCommand->add(savedArrow);

        maps.registerMap(std::make_shared<CharacterMap>(
            CharacterMapInit{{u"mhchem-chars", std::nullopt}, {}}));
        maps.registerMap(std::make_shared<CommandMap>(
            CommandMapInit{MacroMapInit{{u"mhchem", std::nullopt}, {}}}));
        expect(applyMhchemReplacements(std::u16string(input), *originalCommand,
                                       *originalCharacters) ==
               u"\\mhchemrightarrow+\\mhchemxrightarrow");
        expect(applyMhchemReplacements(std::u16string(input), *maps.getMap(u"mhchem"),
                                       *maps.getMap(u"mhchem-chars")) == input);
    };

    "Mhchem error recovery succeeds on the same configured TeX instance"_test = [] {
        MhchemFixture fixture(true);
        expect(std::size(mhchemRecoveryCases()) == 2_u);
        for (const auto &entry : mhchemRecoveryCases()) {
            auto *tree = fixture.compile(entry.input);
            expect(tree != nullptr) << folia::math::utf16::encode(entry.input);
            if (tree)
                expect(snapshot(*tree) == entry.snapshot)
                    << folia::math::utf16::encode(entry.input);
            expect(fixture.error.name == narrow(entry.errorName))
                << folia::math::utf16::encode(entry.input);
            expect(fixture.error.id == narrow(entry.errorId))
                << folia::math::utf16::encode(entry.input);
            expect(fixture.error.message == entry.errorMessage)
                << folia::math::utf16::encode(entry.input);
            expect(fixture.jax->parseOptions().request().parsers.empty())
                << folia::math::utf16::encode(entry.input);
        }
    };
};
