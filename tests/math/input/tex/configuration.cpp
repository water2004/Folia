#include "support/folia_test.hpp"
import folia.math.input.tex.parser;
import folia.math.input.tex.boldsymbol.configuration;

using namespace boost::ut;
using namespace folia::math::tex;

namespace {
struct Warnings final : WarningSink {
    std::vector<std::u16string> messages;
    void warn(std::u16string_view message) override { messages.emplace_back(message); }
};
struct Fixture {
    ConfigurationHandler registry;
    MapHandler maps;
    TexSessionState session;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    ParserConfiguration compose(std::initializer_list<PackageSelection> packages = {},
                                std::vector<std::u16string> parsers = {u"tex"}) {
        return ParserConfiguration(registry, maps, session, callbacks, warnings,
            std::span<PackageSelection const>(packages.begin(), packages.size()), std::move(parsers));
    }
    void map(std::u16string name, std::u16string character) {
        maps.registerMap(std::make_shared<CharacterMap>(CharacterMapInit{
            {std::move(name), std::nullopt}, {Token(u"x", std::move(character))}}));
    }
};
void initA(ParserConfiguration& configuration) {
    configuration.options.maxMacros = configuration.options.maxMacros.value_or(0) * 10 + 1;
}
void initB(ParserConfiguration& configuration) {
    configuration.options.maxMacros = configuration.options.maxMacros.value_or(0) * 10 + 2;
}
void configMethod(ParserConfiguration&, TeX&) {}
void filter(TexFilterData&) {}
ConfigurationDefinition package(std::u16string map, std::size_t buffer, InitMethod init) {
    ConfigurationDefinition definition;
    definition.handler = {{HandlerKind::macro, {std::move(map)}}};
    definition.options.maxBuffer = buffer;
    definition.init = ProcessorSpec<InitMethod>{init};
    return definition;
}
std::u16string selected(ParserConfiguration const& configuration) {
    auto value = configuration.handlers.get(HandlerKind::macro)->lookup(u"x");
    return std::get<Token const*>(value)->character.value();
}
void appendInit(ParserConfiguration& configuration) {
    if (configuration.options.maxMacros) return;
    configuration.options.maxMacros = 1;
    ConfigurationDefinition local;
    local.init = ProcessorSpec<InitMethod>{initB};
    configuration.append(Configuration::local(std::move(local)));
}
void installMap(ParserConfiguration& configuration) {
    configuration.maps().registerMap(std::make_shared<CharacterMap>(CharacterMapInit{
        {u"installed", std::nullopt}, {Token(u"x", std::u16string(u"installed"))}}));
    ConfigurationDefinition local;
    local.handler = {{HandlerKind::macro, {u"installed"}}};
    configuration.append(Configuration::local(std::move(local)), 1);
    configuration.session().defineMacro(u"owned", MacroDefinition{.replacement_source = u"value"});
}
}

suite configuration = [] {
    "configuration local data preserve four handler domains and normalize processor priorities"_test = [] {
        ConfigurationDefinition definition;
        definition.priority = 7;
        definition.parser = u"";
        definition.init = ProcessorSpec<InitMethod>{initA};
        definition.config = ProcessorSpec<ConfigMethod>{configMethod, 3};
        definition.preprocessors = {ProcessorSpec<FilterList::Callback>{FilterList::Callback{filter}},
            ProcessorSpec<FilterList::Callback>{FilterList::Callback{filter}, 0}};
        auto local = Configuration::local(std::move(definition));
        expect(local->name.empty());
        expect(local->priority == 7.0);
        expect(local->parser == u"tex");
        expect(local->handler.size() == 4_u);
        const std::array domains{HandlerKind::character, HandlerKind::delimiter, HandlerKind::macro, HandlerKind::environment};
        for (std::size_t i = 0; i < domains.size(); ++i) expect(local->handler[i].kind == domains[i]);
        expect(local->init() == initA);
        expect(local->config() == configMethod);
        expect(local->initMethod->priority == 7.0);
        expect(local->configMethod->priority == 3.0);
        expect(local->preprocessors[0].priority == 7.0);
        expect(local->preprocessors[1].priority == 0.0);
    };

    "configuration registry replacement preserves key order and releases unselected records"_test = [] {
        ConfigurationHandler registry;
        auto original = Configuration::create(registry, u"a");
        Configuration::create(registry, u"b");
        std::weak_ptr<Configuration const> previous = original;
        auto replacement = Configuration::create(registry, u"a");
        expect(std::ranges::equal(registry.keys(), std::array{u"a", u"b"}));
        expect(registry.get(u"a") == replacement);
        original.reset();
        expect(previous.expired());
        for (int i = 0; i < 256; ++i) {
            previous = replacement;
            replacement = Configuration::create(registry, u"a");
            expect(previous.expired());
        }
        expect(!registry.get(u"missing"));
        auto local = Configuration::local();
        expect(local->name.empty());
        expect(registry.keys().size() == 2_u);
    };

    "configuration constructor reverses ties while options and handler precedence keep their Source roles"_test = [] {
        Fixture fixture;
        fixture.map(u"a-map", u"A"); fixture.map(u"b-map", u"B");
        auto a = package(u"a-map", 10, initA);
        a.nodes.token = NodeFactory::createToken;
        auto b = package(u"b-map", 20, initB);
        b.nodes.token = createBoldToken;
        Configuration::create(fixture.registry, u"a", std::move(a));
        Configuration::create(fixture.registry, u"b", std::move(b));
        auto configuration = fixture.compose({{u"a"}, {u"b"}});
        expect(configuration.options.maxBuffer.value() == 10_u);
        expect(selected(configuration) == u"B");
        expect(configuration.nodes.token.value() == NodeFactory::createToken);
        configuration.init();
        expect(configuration.options.maxMacros.value() == 21_u);
        expect(fixture.warnings.messages.empty());
        expect(&configuration.maps() == &fixture.maps);
        expect(&configuration.session() == &fixture.session);
        expect(&configuration.callbacks() == &fixture.callbacks);
    };

    "configuration tuple zero sorts packages but append uses the package handler priority"_test = [] {
        Fixture fixture;
        fixture.map(u"a-map", u"A"); fixture.map(u"b-map", u"B");
        auto a = package(u"a-map", 10, initA); a.priority = 10;
        auto b = package(u"b-map", 20, initB); b.priority = 7;
        Configuration::create(fixture.registry, u"a", std::move(a));
        Configuration::create(fixture.registry, u"b", std::move(b));
        auto configuration = fixture.compose({{u"a", 0}, {u"b", 5}});
        expect(configuration.options.maxBuffer.value() == 20_u);
        expect(selected(configuration) == u"B");
        configuration.init();
        expect(configuration.options.maxMacros.value() == 21_u);
    };

    "configuration init observes processors appended by an earlier init callback"_test = [] {
        Fixture fixture;
        ConfigurationDefinition definition;
        definition.init = ProcessorSpec<InitMethod>{appendInit};
        Configuration::create(fixture.registry, u"append", std::move(definition));
        auto configuration = fixture.compose({{u"append"}});
        configuration.init();
        expect(configuration.options.maxMacros.value() == 12_u);
        configuration.init();
        expect(configuration.options.maxMacros.value() == 122_u);
        expect(std::ranges::equal(fixture.registry.keys(), std::array{u"append"}));
    };

    "configuration init uses real map and session services with a local configuration"_test = [] {
        Fixture fixture;
        ConfigurationDefinition definition;
        definition.init = ProcessorSpec<InitMethod>{installMap};
        Configuration::create(fixture.registry, u"install", std::move(definition));
        auto configuration = fixture.compose({{u"install"}});
        configuration.init();
        expect(selected(configuration) == u"installed");
        expect(fixture.session.findMacro(u"owned")->replacement_source == u"value");
        expect(std::ranges::equal(fixture.registry.keys(), std::array{u"install"}));
    };

    "configuration selection warns for missing packages and rejects a different parser target"_test = [] {
        Fixture fixture;
        auto missing = fixture.compose({{u"missing"}});
        expect(std::ranges::equal(fixture.warnings.messages,
            std::array{u"MathJax Warning: Package 'missing' not found.  Omitted."}));
        expect(missing.handlers.keys().empty());
        ConfigurationDefinition definition;
        definition.parser = u"text";
        Configuration::create(fixture.registry, u"text-only", std::move(definition));
        expect(throws<std::runtime_error>([&] { (void)fixture.compose({{u"text-only"}}); }));
        auto text = fixture.compose({{u"text-only"}}, {u"text"});
        expect(text.handlers.keys().size() == 4_u);
    };

    "configuration keeps a selected record alive after its named registry entry is replaced"_test = [] {
        Fixture fixture;
        ConfigurationDefinition definition;
        definition.options.maxBuffer = 10;
        auto original = Configuration::create(fixture.registry, u"same", std::move(definition));
        std::weak_ptr<Configuration const> previous = original;
        {
            auto configuration = fixture.compose({{u"same"}});
            ConfigurationDefinition replacement;
            replacement.options.maxBuffer = 20;
            Configuration::create(fixture.registry, u"same", std::move(replacement));
            original.reset();
            expect(!previous.expired());
            expect(configuration.options.maxBuffer.value() == 10_u);
        }
        expect(previous.expired());
    };
};
