#include "math/support/mhchem_test.hpp"

suite mhchem_registry_behavior = [] {
    "Mhchem package configuration retains all commands characters and callback aliases"_test = [] {
        ConfigurationHandler registry;
        MapHandler maps;
        MacroCallbacks callbacks{mhchemCallbacks, mhchemTokenCallbacks};
        const auto configuration = MhchemConfiguration::initializeModule(registry, maps);
        expect(configuration != nullptr);
        const auto commandMap = std::dynamic_pointer_cast<CommandMap>(maps.getMap(u"mhchem"));
        const auto characterMap =
            std::dynamic_pointer_cast<CharacterMap>(maps.getMap(u"mhchem-chars"));
        expect(commandMap != nullptr);
        expect(characterMap != nullptr);
        if (!commandMap || !characterMap)
            return;

        expect(MhchemMethods::Macro == &BaseMethods::Macro);
        expect(MhchemMethods::xArrow == &AmsMethods::xArrow);
        expect(callbacks.macroBinding(&MhchemMethods::Machine).value != 0_u);
        expect(callbacks.macroBinding(MhchemMethods::Macro).value != 0_u);
        expect(callbacks.macroBinding(MhchemMethods::xArrow).value != 0_u);
        expect(callbacks.tokenBinding(&MhchemUtils::relmo).value != 0_u);

        constexpr std::array<std::u16string_view, 9> commands{
            u"ce", u"pu", u"mhchemxrightarrow", u"mhchemxleftarrow",
            u"mhchemxleftrightarrow", u"mhchemxleftrightarrows",
            u"mhchemxrightleftharpoons", u"mhchemxRightleftharpoons",
            u"mhchemxLeftrightharpoons"};
        for (const auto name : commands) {
            const auto entry = commandMap->lookup(name);
            const auto *macro = std::get_if<Macro const *>(&entry);
            const auto *handler = macro && *macro ? std::get_if<MacroHandler>(&(*macro)->function)
                                                  : nullptr;
            expect(handler != nullptr) << folia::math::utf16::encode(name);
            if (name == u"ce" || name == u"pu")
                expect(handler && *handler == &MhchemMethods::Machine)
                    << folia::math::utf16::encode(name);
            else
                expect(handler && *handler == MhchemMethods::xArrow)
                    << folia::math::utf16::encode(name);
        }
        constexpr std::array<std::u16string_view, 14> characters{
            u"tripledash", u"mhchemBondTD", u"mhchemBondTDD", u"mhchemBondDTD",
            u"mhchemlongleftarrow", u"mhchemlongrightarrow", u"mhchemlongleftrightarrow",
            u"mhchemlongrightleftharpoons", u"mhchemlongRightleftharpoons",
            u"mhchemlongLeftrightharpoons", u"mhchemlongleftrightarrows",
            u"mhchemrightarrow", u"mhchemleftarrow", u"mhchemleftrightarrow"};
        for (const auto name : characters)
            expect(std::holds_alternative<Token const *>(characterMap->lookup(name)))
                << folia::math::utf16::encode(name);
        const auto parser = characterMap->parser();
        const auto *relmo = parser ? std::get_if<TokenInputHandler>(&*parser) : nullptr;
        expect(relmo && *relmo == &MhchemUtils::relmo);

        const auto rules = mhchemReplacements();
        expect(rules.size() == 6_u);
        if (rules.size() == 6) {
            for (std::size_t i = 0; i < rules.size(); ++i)
                expect(rules[i].kind == (i == 5 ? MhchemReplacementKind::arrow
                                                : MhchemReplacementKind::text));
            expect(rules[0].replacement == u"\\mhchemx$3[$1]{$2}");
            expect(rules[1].replacement == u"\\mhchemx$2{$1}");
            expect(rules[2].replacement == u"\\mhchemBondTD");
            expect(rules[3].replacement == u"\\mhchemBondTDD");
            expect(rules[4].replacement == u"\\mhchemBondDTD");
            expect(rules[5].replacement.empty());
        }
    };

    "Mhchem uses real let aliases and preserves relmo creator overrides"_test = [] {
        expect(std::size(mhchemAliasCases()) == 2_u);
        for (const auto &entry : mhchemAliasCases()) {
            MhchemFixture fixture(true, true);
            auto *tree = fixture.compile(entry.input);
            expect(tree != nullptr);
            if (tree) {
                const auto actual = snapshot(*tree);
                if (actual != entry.snapshot)
                    reportSnapshotDifference(entry.input, actual, entry.snapshot);
                expect(actual == entry.snapshot)
                    << folia::math::utf16::encode(entry.input);
            }
            expect(fixture.error.id.empty());
        }

        MhchemFixture fixture(true);
        auto characters = std::dynamic_pointer_cast<CharacterMap>(
            fixture.maps.getMap(u"mhchem-chars"));
        expect(characters != nullptr);
        if (!characters)
            return;
        AttributeSet attributes;
        attributes.set(u"stretchy", false);
        attributes.set(u"mathvariant", u"-custom");
        attributes.set(u"texClass", 0.0);
        characters->add(Token(u"mhchemCustom", u"\u2192", std::move(attributes)));

        struct CreatorRequest {
            NodeKind kind;
            NodeDefinition definition;
            std::u16string text;
        };
        std::vector<CreatorRequest> requests;
        thread_local std::vector<CreatorRequest> *activeRequests{};
        const auto creator = [](NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                                std::optional<std::u16string_view> text) -> MmlNode & {
            if (activeRequests && kind == NodeKind::mo && text == u"\u2192")
                activeRequests->push_back({kind, definition,
                                           text ? std::u16string(*text) : std::u16string{}});
            return NodeFactory::createToken(factory, kind, std::move(definition), text);
        };
        activeRequests = &requests;
        fixture.jax->parseOptions().request().nodeFactory.setCreators(
            NodeCreators{.token = creator});
        auto *tree = fixture.compile(mhchemCreatorCase().input);
        activeRequests = nullptr;
        expect(tree != nullptr);
        expect(requests.size() == 1_u);
        if (tree)
            expect(snapshot(*tree) == mhchemCreatorCase().snapshot);
        if (requests.size() == 1) {
            expect(requests[0].kind == NodeKind::mo);
            expect(requests[0].text == mhchemCreatorCase().text);
            expect(attribute(requests[0].definition.attributes, u"stretchy") == u"false");
            expect(attribute(requests[0].definition.attributes, u"mathvariant") ==
                   mhchemCreatorCase().mathvariant);
            expect(attribute(requests[0].definition.attributes, u"texClass") == u"0");
            auto *arrow = findToken(*tree, NodeKind::mo, mhchemCreatorCase().text);
            expect(arrow != nullptr);
            if (arrow) {
                const auto classValue = arrow->texClass();
                expect(classValue.has_value());
                if (classValue)
                    expect(*classValue == TexClass::ord);
            }
        }
    };
};
