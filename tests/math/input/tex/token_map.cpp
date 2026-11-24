#include "math/support/tex_fixture.hpp"

using namespace boost::ut;
using namespace folia::math::tex;
namespace {
DispatchOutcome macroMethod(TexParser&, MacroInvocation const&) { return Handled{}; }
DispatchOutcome expansion(TexParser&, MacroInvocation const&) { return NoParseResult{}; }
DispatchOutcome tokenMethod(TexParser&, TokenInput const&) { return Handled{}; }
struct CommandTrace {
    MacroMap* map{};
    std::vector<std::u16string> observations;
    bool stop{}, fail{}, remove{};
};
thread_local CommandTrace* commandTrace{};
struct CommandTraceScope {
    CommandTrace* previous = commandTrace;
    explicit CommandTraceScope(CommandTrace& trace) { commandTrace = &trace; }
    ~CommandTraceScope() { commandTrace = previous; }
};
DispatchOutcome recordCommand(TexParser& parser, MacroInvocation const& invocation) {
    auto& trace = *commandTrace;
    trace.observations.push_back(parser.currentCS());
    trace.observations.emplace_back(invocation.input_token);
    trace.observations.emplace_back(invocation.dispatch_token);
    if (trace.remove) trace.map->remove(invocation.input_token);
    auto args = std::get<std::span<TexArgument const>>(invocation.arguments);
    if (!args.empty()) trace.observations.push_back(std::get<std::u16string>(args.front()));
    if (trace.fail) throw TexError("CommandTrace", u"Command callback failed");
    return trace.stop ? DispatchOutcome{ContinueDispatch{}} : DispatchOutcome{NoParseResult{}};
}
DispatchOutcome nestedCommand(TexParser& parser, MacroInvocation const&) {
    commandTrace->observations.push_back(parser.currentCS());
    commandTrace->map->parse(parser, u"inner");
    commandTrace->observations.push_back(parser.currentCS());
    return NoParseResult{};
}
DispatchOutcome recordCharacter(TexParser& parser, TokenInput const& input) {
    auto& trace = *commandTrace;
    trace.observations.push_back(parser.currentCS());
    trace.observations.push_back(std::get<Token const*>(input.value)->character.value());
    return NoParseResult{};
}
DispatchOutcome recordExpansion(TexParser& parser, MacroInvocation const& invocation) {
    auto& trace = *commandTrace;
    trace.observations.push_back(parser.currentCS());
    trace.observations.emplace_back(invocation.dispatch_token);
    auto& definition = std::get<std::reference_wrapper<CommandDefinition const>>(invocation.arguments).get();
    trace.observations.push_back(std::get<MacroDefinition>(definition.body).replacement_source);
    return NoParseResult{};
}
}

suite token_map = [] {
    "command map invokes captured definitions and restores currentCS after normal results"_test = [] {
        Fixture fixture;
        TexParser parser(u"", ParserEnvironmentSeed{}, *fixture.options);
        parser.setCurrentCS(u"previous");
        CommandMap map(CommandMapInit{MacroMapInit{{u"commands", std::nullopt}, {}}});
        map.add(u"alias", Macro(u"definition", MacroMethod{recordCommand}, {std::u16string(u"bound")}));
        CommandTrace trace{.map=&map, .remove=true};
        CommandTraceScope scope(trace);
        expect(std::holds_alternative<Handled>(map.parse(parser, u"alias")));
        expect(std::ranges::equal(trace.observations,
            std::array<std::u16string,4>{u"\\alias",u"alias",u"\\definition",u"bound"}));
        expect(!map.contains(u"alias"));
        expect(parser.currentCS() == u"previous");
        expect(std::holds_alternative<ContinueDispatch>(map.parse(parser,u"absent")));
        map.add(Macro(u"missing", MacroMethod{static_cast<MacroHandler>(nullptr)}));
        expect(std::holds_alternative<ContinueDispatch>(map.parse(parser,u"missing")));
        expect(parser.currentCS() == u"previous");
        trace.remove=false; trace.stop=true;
        map.add(Macro(u"stop", MacroMethod{recordCommand}));
        expect(std::holds_alternative<ContinueDispatch>(map.parse(parser,u"stop")));
        expect(parser.currentCS() == u"previous");
        MacroMap macros(MacroMapInit{{u"macros", std::nullopt}, {Macro(u"raw",MacroMethod{recordCommand})}});
        trace.observations.clear();
        macros.parse(parser,u"raw");
        expect(std::ranges::equal(trace.observations,
            std::array<std::u16string,3>{u"previous",u"raw",u"raw"}));
    };
    "command map nesting restores enclosing commands and retains the failed command on exception"_test = [] {
        Fixture fixture;
        TexParser parser(u"", ParserEnvironmentSeed{}, *fixture.options);
        parser.setCurrentCS(u"previous");
        CommandMap map(CommandMapInit{MacroMapInit{{u"nested", std::nullopt}, {
            Macro(u"outer", MacroMethod{nestedCommand}), Macro(u"inner", MacroMethod{recordCommand})}}});
        CommandTrace trace{.map=&map};
        CommandTraceScope scope(trace);
        expect(std::holds_alternative<Handled>(map.parse(parser,u"outer")));
        expect(std::ranges::equal(trace.observations,
            std::array<std::u16string,5>{u"\\outer",u"\\inner",u"inner",u"\\inner",u"\\outer"}));
        expect(parser.currentCS() == u"previous");
        trace.fail=true;
        texError([&] { map.parse(parser,u"outer"); }, "CommandTrace");
        expect(parser.currentCS() == u"\\inner");
    };
    "command map uses canonical session definitions for aliases characters suppression and expansion"_test = [] {
        Fixture fixture;
        TexParser parser(u"", ParserEnvironmentSeed{}, *fixture.options);
        const std::array<MacroHandler,1> macros{recordCommand};
        const std::array<TokenInputHandler,1> tokens{recordCharacter};
        MacroCallbacks callbacks(macros,tokens);
        CommandMap map(SessionMacroMapInit{{u"session-commands",std::nullopt},fixture.session,callbacks,recordExpansion});
        CommandTrace trace{.map=&map};
        CommandTraceScope scope(trace);
        fixture.session.defineCommand(u"alias",CommandDefinition{u"original",NativeMacroAlias{MacroHandlerId{1},{std::u16string(u"captured")}}});
        map.parse(parser,u"alias");
        expect(std::ranges::equal(trace.observations,
            std::array<std::u16string,4>{u"\\alias",u"alias",u"\\original",u"captured"}));
        trace.observations.clear();
        fixture.session.defineCommand(u"char",CommandDefinition{u"alpha",CharacterMacroAlias{TokenParserId{1},Token(u"alpha",std::u16string(u"α"))}});
        map.parse(parser,u"char");
        expect(std::ranges::equal(trace.observations,std::array<std::u16string,2>{u"\\char",u"α"}));
        trace.observations.clear();
        fixture.session.defineMacro(u"expand",MacroDefinition{.replacement_source=u"replacement"});
        map.parse(parser,u"expand");
        expect(std::ranges::equal(trace.observations,std::array<std::u16string,3>{u"\\expand",u"\\expand",u"replacement"}));
        fixture.session.defineCommand(u"expand",CommandDefinition{u"expand",MacroSuppression{}});
        expect(std::holds_alternative<InvokeFallback>(map.parse(parser,u"expand")));
        expect(parser.currentCS().empty());
    };
    "token map lookup keeps the lookup key separate from the captured token"_test = [] {
        CharacterMap map(CharacterMapInit{{u"characters", std::nullopt},
            {Token(u"x", std::u16string(u"a")), Token(u"nil", std::nullopt)}});
        map.add(u"alias", Token(u"original", std::u16string(u"b")));
        expect(map.contains(u"alias"));
        expect(!map.contains(u"original"));
        auto value = std::get<Token const*>(map.lookup(u"alias"));
        expect(value->token == u"original");
        expect(value->character.value() == u"b");
        map.add(u"alias", Token(u"changed", std::u16string(u"c")));
        expect(std::get<Token const*>(map.lookup(u"alias"))->token == u"changed");
        map.remove(u"alias");
        expect(std::holds_alternative<std::monostate>(map.lookup(u"alias")));
        expect(map.contains(u"nil"));
        expect(!std::get<Token const*>(map.lookup(u"nil"))->character.has_value());
        map.setParser(TokenMapParser{tokenMethod});
        expect(std::get<TokenInputHandler>(map.parserFor(u"x").value()) == tokenMethod);
        map.setParser(std::nullopt);
        expect(!map.parserFor(u"x").has_value());
    };

    "token map regexp lookup preserves the original stateful test operations"_test = [] {
        auto expression = std::make_shared<folia::math::regexp_host::RegExp>(u"^a$", u"g");
        RegExpMap map(RegExpMapInit{{u"letters", TokenMapParser{tokenMethod}}, expression});
        expect(map.contains(u"a"));
        expect(expression->last_index == 1_u);
        expect(std::holds_alternative<std::monostate>(map.lookup(u"a")));
        expect(expression->last_index == 0_u);
        expect(std::get<std::u16string_view>(map.lookup(u"a")) == u"a");
        expect(expression->last_index == 1_u);
    };

    "token map macro values preserve all argument types and absent callbacks"_test = [] {
        MacroMap map(MacroMapInit{{u"macros", std::nullopt}, {
            Macro(u"source", MacroMethod{macroMethod}, {false, 2.0, std::u16string(u"x"), NullArgument{}}),
            Macro(u"missing", MacroMethod{static_cast<MacroHandler>(nullptr)})}});
        map.add(u"alias", Macro(u"source", MacroMethod{macroMethod}, {std::u16string(u"bound")}));
        expect(std::get<Macro const*>(map.lookup(u"alias"))->token == u"source");
        auto value = std::get<Macro const*>(map.lookup(u"source"));
        expect(value->arguments.size() == 4_u);
        expect(!std::get<bool>(value->arguments[0]));
        expect(std::get<double>(value->arguments[1]) == 2.0);
        expect(std::get<std::u16string>(value->arguments[2]) == u"x");
        expect(std::holds_alternative<NullArgument>(value->arguments[3]));
        expect(std::get<MacroHandler>(map.parserFor(u"source").value()) == macroMethod);
        expect(map.contains(u"missing"));
        expect(!map.parserFor(u"missing").has_value());
    };

    "token map session lookup selects current definitions and typed alias bindings"_test = [] {
        std::array<MacroHandler, 1> macros{macroMethod};
        std::array<TokenInputHandler, 1> tokens{tokenMethod};
        MacroCallbacks callbacks(macros, tokens);
        TexSessionState session;
        session.defineMacro(u"x", MacroDefinition{.replacement_source = u"outer"});
        MacroMap map(SessionMacroMapInit{{u"session", std::nullopt}, session, callbacks, expansion});
        expect(std::get<MacroHandler>(map.parserFor(u"x").value()) == expansion);
        session.definitions().groups.push_back(DefinitionLayer{-101});
        session.defineCommand(u"x", CommandDefinition{u"original", NativeMacroAlias{MacroHandlerId{1}, {true}}});
        expect(std::get<MacroHandler>(map.parserFor(u"x").value()) == macroMethod);
        auto selected = std::get<CommandDefinition const*>(map.lookup(u"x"));
        expect(selected == session.findCommand(u"x"));
        expect(selected->source_token == u"original");
        session.defineCommand(u"x", CommandDefinition{u"captured",
            CharacterMacroAlias{TokenParserId{1}, Token(u"captured", std::u16string(u"c"))}});
        expect(std::get<TokenInputHandler>(map.parserFor(u"x").value()) == tokenMethod);
        session.defineCommand(u"x", CommandDefinition{u"x", MacroSuppression{}});
        expect(map.contains(u"x"));
        expect(!map.parserFor(u"x").has_value());
        expect(!session.findMacro(u"x"));
        session.definitions().groups.pop_back();
        expect(std::get<MacroHandler>(map.parserFor(u"x").value()) == expansion);
    };

    "token map parseResult normalizes only the undefined result category"_test = [] {
        expect(std::holds_alternative<Handled>(parseResult(NoParseResult{})));
        expect(std::holds_alternative<Handled>(parseResult(Handled{})));
        expect(std::holds_alternative<ContinueDispatch>(parseResult(ContinueDispatch{})));
        expect(std::holds_alternative<InvokeFallback>(parseResult(InvokeFallback{})));
    };
};
