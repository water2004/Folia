#include "support/folia_test.hpp"
import folia.math.input.tex.parser;

using namespace boost::ut;
using namespace folia::math::tex;
namespace {
struct Warnings final : WarningSink {
    std::vector<std::u16string> messages;
    void warn(std::u16string_view text) override { messages.emplace_back(text); }
};
std::shared_ptr<CharacterMap> characters(std::u16string name, std::optional<std::u16string> value) {
    return std::make_shared<CharacterMap>(CharacterMapInit{{std::move(name), std::nullopt}, {Token(u"x", std::move(value))}});
}
std::u16string selected(SubHandler const& handler) {
    return std::get<Token const*>(handler.lookup(u"x"))->character.value();
}
}

suite map_handler = [] {
    "map handler reverses input maps and preserves stable priority and duplicate removal"_test = [] {
        MapHandler registry;
        Warnings warnings;
        for (auto name : {u"a", u"b", u"c", u"early"}) registry.registerMap(characters(name, std::u16string(name)));
        SubHandler handler(registry, warnings);
        const std::array<std::u16string, 3> maps{u"a", u"b", u"c"};
        handler.add(maps);
        expect(handler.toString() == u"c, b, a");
        expect(selected(handler) == u"c");
        const std::array<std::u16string, 1> early{u"early"};
        handler.add(early, nullptr, 1);
        expect(handler.toString() == u"early, c, b, a");
        const std::array<std::u16string, 1> a{u"a"};
        handler.add(a);
        handler.remove(a);
        expect(handler.toString() == u"early, c, b, a");
        expect(warnings.messages.empty());
    };

    "map handler keeps earlier additions when a missing reversed map stops registration"_test = [] {
        MapHandler registry;
        Warnings warnings;
        registry.registerMap(characters(u"first", std::u16string(u"f")));
        registry.registerMap(characters(u"last", std::u16string(u"l")));
        SubHandler handler(registry, warnings);
        const std::array<std::u16string, 3> maps{u"first", u"missing", u"last"};
        handler.add(maps);
        expect(handler.toString() == u"last");
        expect(warnings.messages == std::vector<std::u16string>{u"TexParser Warning: Configuration 'missing' not found! Omitted."});
    };

    "map handler null character suppresses contains without looking through the first applicable map"_test = [] {
        MapHandler registry;
        Warnings warnings;
        registry.registerMap(characters(u"visible", std::u16string(u"v")));
        registry.registerMap(characters(u"suppressed", std::nullopt));
        SubHandler handler(registry, warnings);
        const std::array<std::u16string, 2> maps{u"visible", u"suppressed"};
        handler.add(maps);
        expect(!handler.contains(u"x"));
        expect(handler.applicable(u"x") == registry.getMap(u"suppressed"));
        expect(!std::get<Token const*>(handler.lookup(u"x"))->character.has_value());
        expect(!handler.contains(u"absent"));
    };

    "map handler replacement retains an old selected map only until its configuration releases it"_test = [] {
        MapHandler registry;
        Warnings warnings;
        auto original = characters(u"same", std::u16string(u"old"));
        std::weak_ptr<TokenMap> previous = original;
        registry.registerMap(original);
        SubHandler handler(registry, warnings);
        const std::array<std::u16string, 1> maps{u"same"};
        handler.add(maps);
        registry.registerMap(characters(u"same", std::u16string(u"new")));
        original.reset();
        expect(!previous.expired());
        expect(selected(handler) == u"old");
        handler.add(maps);
        handler.remove(maps);
        expect(previous.expired());
        expect(selected(handler) == u"new");
        handler.remove(maps);
        expect(handler.toString().empty());
    };

    "map handler repeated same name replacement releases unselected mappings by RAII"_test = [] {
        std::weak_ptr<TokenMap> previous;
        {
            MapHandler registry;
            for (int i = 0; i < 256; ++i) {
                auto current = characters(u"same", std::u16string(u"x"));
                registry.registerMap(current);
                expect(previous.expired());
                previous = current;
            }
            expect(!previous.expired());
        }
        expect(previous.expired());
    };

    "map handler subhandler keys follow first insertion and replacement preserves that order"_test = [] {
        MapHandler registry;
        Warnings warnings;
        auto old = characters(u"same", std::u16string(u"old"));
        registry.registerMap(old);
        SubHandlers handlers(registry, warnings);
        const std::array<HandlerRegistration, 1> macro{{{HandlerKind::macro, {u"same"}}}};
        const std::array<HandlerRegistration, 1> character{{{HandlerKind::character, {u"same"}}}};
        handlers.add(macro);
        registry.registerMap(characters(u"same", std::u16string(u"new")));
        handlers.add(character);
        const std::array order{HandlerKind::macro, HandlerKind::character};
        expect(std::ranges::equal(handlers.keys(), order));
        expect(handlers.retrieve(u"same") == old);
        handlers.set(HandlerKind::macro, std::make_unique<SubHandler>(registry, warnings));
        expect(handlers.keys().front() == HandlerKind::macro);
        expect(handlers.retrieve(u"same") == registry.getMap(u"same"));
        handlers.remove(character);
        expect(!handlers.retrieve(u"same"));
    };
};
