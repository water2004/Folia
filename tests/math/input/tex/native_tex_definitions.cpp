#include "support/folia_test.hpp"
import folia.math.input.tex.session;

using namespace boost::ut;
using namespace folia::math::tex;

suite native_tex_definitions = [] {
    "native let definitions distinguish builtin operation and captured character token"_test = [] {
        TexSessionState session;
        session.defineCommand(u"savedRoot", {u"sqrt", NativeMacroAlias{
            MacroHandlerId{7}, {std::u16string(u"root"), false, 2.0, NullArgument{}}}});
        auto const* root = session.findCommand(u"savedRoot");
        expect(root->source_token == u"sqrt");
        auto const& alias = std::get<NativeMacroAlias>(root->body);
        expect(alias.handler == MacroHandlerId{7});
        expect(alias.arguments.size() == 4_u);
        expect(std::get<std::u16string>(alias.arguments[0]) == u"root");
        expect(std::get<double>(alias.arguments[2]) == 2.0);
        folia::math::mml::AttributeSet attributes;
        attributes.set(u"mathvariant", u"normal");
        Token original(u"alpha", u"α", attributes);
        session.defineCommand(u"savedAlpha", {u"savedAlpha",
            CharacterMacroAlias{TokenParserId{3}, original}});
        original.character = u"β";
        original.attributes->set(u"mathvariant", u"bold");
        auto const& captured = std::get<CharacterMacroAlias>(session.findCommand(u"savedAlpha")->body);
        expect(captured.parser == TokenParserId{3});
        expect(captured.captured.character == u"α");
        expect(std::get<std::u16string>(*captured.captured.attributes->own(u"mathvariant")) == u"normal");
        expect(session.findMacro(u"savedRoot") == nullptr);
        expect(session.findMacro(u"savedAlpha") == nullptr);
    };

    "native definition suppression shadows outer replacement without falling through"_test = [] {
        TexSessionState session;
        session.defineMacro(u"foo", {u"outer", {}, {}, 0, {}});
        session.definitions().groups.emplace_back(-101);
        session.defineCommand(u"foo", {u"foo", MacroSuppression{}});
        expect(session.findMacro(u"foo") == nullptr);
        expect(std::holds_alternative<MacroSuppression>(session.findCommand(u"foo")->body));
        expect(session.eraseCommand(u"foo"));
        expect(session.findMacro(u"foo")->replacement_source == u"outer");
        session.defineCommand(u"foo", {u"sqrt", NativeMacroAlias{MacroHandlerId{7}, {}}});
        expect(session.findMacro(u"foo") == nullptr);
        session.definitions().groups.pop_back();
        expect(session.findMacro(u"foo")->replacement_source == u"outer");
    };

    "native persistent definitions own all three Source domains and isolate copied values"_test = [] {
        TexSessionState session;
        session.defineMacro(u"foo", {u"x", {}, {}, 0, {}});
        session.defineDelimiter(u"\\foo", Token(u"\\foo", u"(", {}));
        session.defineEnvironment(u"foo", {u"(#1)", u"!", 1, std::u16string(u"d")});
        auto copy = session;
        session.defineDelimiter(u"\\foo", Token(u"\\foo", std::nullopt, {}));
        session.defineEnvironment(u"foo", {u"changed", {}, 0, {}});
        session.defineMacro(u"foo", {u"changed", {}, {}, 0, {}});
        expect(copy.findMacro(u"foo")->replacement_source == u"x");
        expect(copy.findDelimiter(u"\\foo")->character == u"(");
        expect(!session.findDelimiter(u"\\foo")->character);
        expect(copy.findEnvironment(u"foo")->default_first_argument == u"d");
        expect(copy.findEnvironment(u"foo")->begin_source == u"(#1)");
        auto equation = copy.beginEquation();
        copy.finishEquation(equation);
        copy.resetTags();
        expect(copy.findMacro(u"foo") != nullptr);
        expect(copy.findEnvironment(u"foo") != nullptr);
        expect(copy.findDelimiter(u"\\foo") != nullptr);
    };

    "native alias validation leaves an existing definition unchanged"_test = [] {
        TexSessionState session;
        session.defineMacro(u"foo", {u"kept", {}, {}, 0, {}});
        expect(throws<std::invalid_argument>([&] {
            session.defineCommand(u"foo", {u"foo", NativeMacroAlias{MacroHandlerId{}, {}}});
        }));
        expect(throws<std::invalid_argument>([&] {
            session.defineCommand(u"foo", {u"foo", CharacterMacroAlias{
                TokenParserId{}, Token(u"alpha", u"α")}});
        }));
        expect(session.findMacro(u"foo")->replacement_source == u"kept");
    };
};
