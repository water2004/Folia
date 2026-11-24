#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.parse_methods;
import folia.math.input.tex.parse_util;

namespace {
using folia::math::regexp_host::RegExp;
void installMethod(Fixture &fixture, TokenInputHandler method, FallbackHandler fallback = nullptr) {
    fixture.maps.registerMap(std::make_shared<RegExpMap>(
        RegExpMapInit{{u"method", TokenMapParser{method}}, std::make_shared<RegExp>(u"[\\s\\S]")}));
    const std::array registrations{
        HandlerRegistration{HandlerKind::character, {u"method"}, fallback}};
    fixture.options->handlers.add(registrations);
}
void installToken(Fixture &fixture, TokenInputHandler method, Token value) {
    fixture.maps.registerMap(std::make_shared<CharacterMap>(
        CharacterMapInit{{u"method", TokenMapParser{method}}, {std::move(value)}}));
    const std::array registrations{HandlerRegistration{HandlerKind::character, {u"method"}}};
    fixture.options->handlers.add(registrations);
}
MmlNode &first(TexParser &parser) {
    auto *root = parser.mml();
    if (!root || root->children().empty())
        throw std::runtime_error("Expected parsed token");
    if (root->isToken())
        return *root;
    return *root->children().front();
}
void decimalFallback(TexParser &parser, std::u16string_view text) {
    parser.Push(&parser.createToken(NodeKind::mo, text));
}
DispatchOutcome emitCommand(TexParser &parser, MacroInvocation const &invocation) {
    expect(parser.currentCS() == u"\\alias");
    expect(invocation.dispatch_token == u"\\original");
    auto args = std::get<std::span<TexArgument const>>(invocation.arguments);
    parser.Push(&parser.createToken(NodeKind::mi, std::get<std::u16string>(args.front())));
    return NoParseResult{};
}
DispatchOutcome callEnvironment(TexParser &parser, MacroInvocation const &invocation) {
    return parser.parse(HandlerKind::environment,
                        ParseInput{parser, std::u16string(invocation.input_token)});
}
StackEntry environmentBody(TexParser &parser, FrameHandle const &begin,
                           std::span<TexArgument const> args) {
    auto &item = dynamic_cast<BeginItem &>(*begin);
    expect(item.name == (args.empty() ? u"emptyActual" : u"valuesActual"));
    expect(begin->kind() == StackKind::begin);
    if (!args.empty()) {
        expect(args.size() == 3_u);
        expect(std::get<std::u16string>(args[0]) == u"payload");
        expect(!std::get<bool>(args[1]));
        expect(std::get<double>(args[2]) == 2.0);
    }
    return &parser.createToken(NodeKind::mi, u"E");
}
struct CreationObservation {
    std::optional<bool> auto_operator;
    bool fix_stretchy_before{}, fix_stretchy_after{};
};
thread_local CreationObservation *creationObservation{};
MmlNode &observeCreation(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                         std::optional<std::u16string_view> text) {
    auto &observed = *creationObservation;
    observed.auto_operator = definition.properties.tex.auto_operator;
    observed.fix_stretchy_before = definition.properties.tex.fix_stretchy.has_value();
    auto &node = NodeFactory::createToken(factory, kind, std::move(definition), text);
    observed.fix_stretchy_after = node.properties().tex.fix_stretchy.has_value();
    return node;
}
struct ObserveCreationScope {
    CreationObservation *previous = creationObservation;
    explicit ObserveCreationScope(CreationObservation &observation) {
        creationObservation = &observation;
    }
    ~ObserveCreationScope() {
        creationObservation = previous;
    }
};
} // namespace

suite parse_methods = [] {
    "parse methods variables use font and Unicode normalized math styles"_test = [] {
        struct Case {
            std::u16string source, style, expected;
        };
        const std::array cases{Case{u"x", u"TeX", u""},          Case{u"é", u"French", u"italic"},
                               Case{u"Α", u"French", u"normal"}, Case{u"α", u"ISO", u"italic"},
                               Case{u"𝛼", u"upright", u""},      Case{u"ϰ", u"upright", u"normal"},
                               Case{u"ϱ", u"upright", u""}};
        for (auto const &item : cases) {
            Fixture fixture;
            fixture.options->mathStyle = ParseOptions::getVariant(item.style);
            installMethod(fixture, ParseMethods::variable);
            TexParser parser(item.source, ParserEnvironmentSeed{}, *fixture.options);
            auto &node = first(parser);
            expect(node.kind() == NodeKind::mi);
            expect(NodeUtil::getText(node) == item.source);
            expect(attr(node.attributes(), u"mathvariant") == item.expected);
        }
        for (auto const &font : {u"bold", u"-tex-calligraphic", u""}) {
            Fixture fixture;
            installMethod(fixture, ParseMethods::variable);
            TexParser parser(u"x", ParserEnvironmentSeed{TexEnvironment{.font = font}},
                             *fixture.options);
            auto &node = first(parser);
            expect(attr(node.attributes(), u"mathvariant") == font);
            expect(node.properties().tex.ignore_variant.value_or(false) == (font[0] == u'-'));
        }
        expect(parse_util::isLatinOrGreekChar(u"!é"));
        expect(parse_util::isLatinOrGreekChar(u"ᾧ"));
        expect(!parse_util::isLatinOrGreekChar(u"𝛼"));
        expect(!parse_util::isLatinOrGreekChar(u"ϱ"));
    };
    "parse methods multi letter identifiers preserve empty font and creator properties"_test = [] {
        for (bool emptyFont : {false, true}) {
            Fixture fixture;
            installMethod(fixture, ParseMethods::variable);
            auto expression = std::make_shared<RegExp>(u"^[a-z]+", u"g");
            expression->last_index = 50;
            TexEnvironment env;
            env.multi_letter_identifiers = expression;
            env.no_auto_op = true;
            env.font = emptyFont ? u"" : u"normal";
            CreationObservation observed;
            ObserveCreationScope scope(observed);
            fixture.options->request().nodeFactory.setCreators(
                NodeCreators{.token = observeCreation});
            TexParser parser(u"abc", ParserEnvironmentSeed{env}, *fixture.options);
            auto *root = parser.mml();
            expect((root->isToken() ? 1u : root->children().size()) == (emptyFont ? 3u : 1u));
            expect(NodeUtil::getText(root->isToken() ? *root : *root->children().front()) ==
                   (emptyFont ? u"a" : u"abc"));
            expect(observed.auto_operator.has_value() == !emptyFont);
            if (observed.auto_operator)
                expect(!*observed.auto_operator);
            expect(expression->last_index == (emptyFont ? 50u : 0u));
        }
        Fixture fixture;
        installMethod(fixture, ParseMethods::variable);
        TexEnvironment env;
        env.multi_letter_identifiers = std::make_shared<RegExp>(u"^z+");
        TexParser parser(u"𝛼", ParserEnvironmentSeed{env}, *fixture.options);
        expect(parser.position() == 3_u); // Source advances by c.length - 1 even without a match.
        expect(NodeUtil::getText(first(parser)) == u"𝛼");
    };
    "parse methods digits strip braces and preserve String match regex state"_test = [] {
        for (auto const &flags : {u"", u"g", u"y", u"gy"}) {
            Fixture fixture;
            installMethod(fixture, ParseMethods::digit, decimalFallback);
            auto expression =
                std::make_shared<RegExp>(u"^[0-9]+(?:\\{,\\}[0-9]+)?(?:\\.[0-9]+)?", flags);
            expression->last_index = std::u16string_view(flags) == u"y" ? 0 : 99;
            fixture.effective.numberPattern = expression;
            TexParser parser(u"12{,}345.6", ParserEnvironmentSeed{TexEnvironment{.font = u"bold"}},
                             *fixture.options);
            auto &node = first(parser);
            expect(node.kind() == NodeKind::mn);
            expect(NodeUtil::getText(node) == u"12,345.6");
            expect(attr(node.attributes(), u"mathvariant") == u"bold");
            expect(parser.position() == 10_u);
            expect(expression->last_index == (std::u16string_view(flags) == u"y"   ? 10u
                                              : std::u16string_view(flags).empty() ? 99u
                                                                                   : 0u));
        }
        Fixture fixture;
        installMethod(fixture, ParseMethods::digit, decimalFallback);
        fixture.effective.numberPattern = std::make_shared<RegExp>(u"^[0-9]+|^\\.[0-9]+");
        TexParser parser(u".", ParserEnvironmentSeed{}, *fixture.options);
        auto &node = first(parser);
        expect(node.kind() == NodeKind::mo);
        expect(NodeUtil::getText(node) == u".");
    };
    "parse methods Greek callbacks apply source style and uppercase font precedence"_test = [] {
        struct Case {
            TokenInputHandler method;
            std::u16string text, style, font, expected;
        };
        const std::array cases{Case{ParseMethods::lcGreek, u"α", u"TeX", u"bold", u"italic"},
                               Case{ParseMethods::lcGreek, u"α", u"upright", u"bold", u"normal"},
                               Case{ParseMethods::ucGreek, u"Γ", u"TeX", u"", u"normal"},
                               Case{ParseMethods::ucGreek, u"Γ", u"ISO", u"", u"italic"},
                               Case{ParseMethods::ucGreek, u"Γ", u"ISO", u"bold", u"bold"}};
        for (auto const &item : cases) {
            Fixture fixture;
            fixture.options->mathStyle = ParseOptions::getVariant(item.style);
            installToken(fixture, item.method, Token(u"a", item.text));
            TexParser parser(u"a", ParserEnvironmentSeed{TexEnvironment{.font = item.font}},
                             *fixture.options);
            auto &node = first(parser);
            expect(NodeUtil::getText(node) == item.text);
            expect(attr(node.attributes(), u"mathvariant") == item.expected);
        }
    };
    "parse methods character defaults distinguish absent and empty attributes"_test = [] {
        for (bool supplied : {false, true}) {
            for (auto method : {ParseMethods::mathchar0mi, ParseMethods::mathchar7}) {
                Fixture fixture;
                installToken(
                    fixture, method,
                    Token(u"a", std::u16string(u"R"),
                          supplied ? std::optional<AttributeSet>{AttributeSet{}} : std::nullopt));
                TexParser parser(u"a", ParserEnvironmentSeed{}, *fixture.options);
                auto &node = first(parser);
                expect(attr(node.attributes(), u"mathvariant") == (supplied ? u""
                                                      : method == ParseMethods::mathchar0mi
                                                          ? u"italic"
                                                          : u"normal"));
            }
        }
        Fixture fixture;
        AttributeSet attrs;
        attrs.set(u"mathvariant", u"double-struck");
        installToken(fixture, ParseMethods::mathchar7, Token(u"a", std::u16string(u"R"), attrs));
        TexParser parser(u"a", ParserEnvironmentSeed{TexEnvironment{.font = u"bold"}},
                         *fixture.options);
        expect(attr(first(parser).attributes(), u"mathvariant") == u"bold");
    };
    "parse methods operators and delimiters retain creator ordering and explicit attributes"_test =
        [] {
            Fixture fixture;
            CreationObservation observed;
            ObserveCreationScope scope(observed);
            fixture.options->request().nodeFactory.setCreators(
                NodeCreators{.token = observeCreation});
            AttributeSet attrs;
            attrs.set(u"stretchy", true);
            attrs.set(u"largeop", true);
            installToken(fixture, ParseMethods::mathchar0mo,
                         Token(u"a", std::u16string(u"∑"), attrs));
            TexParser parser(u"a", ParserEnvironmentSeed{}, *fixture.options);
            auto &node = first(parser);
            fixture.options->request().root = &node;
            expect(node.properties().tex.fix_stretchy.value_or(false));
            expect(!observed.fix_stretchy_before);
            expect(!observed.fix_stretchy_after);
            expect(!std::get<bool>(*node.attributes().own(u"stretchy")));
            expect(std::get<bool>(*node.attributes().own(u"largeop")));
            expect(fixture.options->getList(u"fixStretchy").front() == &node);
            for (bool supplied : {false, true}) {
                Fixture delimiters;
                AttributeSet explicitAttrs;
                explicitAttrs.set(u"stretchy", true);
                explicitAttrs.set(u"fence", true);
                installToken(
                    delimiters, ParseMethods::delimiter,
                    Token(u"a", std::u16string(u"|"),
                          supplied ? std::optional<AttributeSet>{explicitAttrs} : std::nullopt));
                TexParser parsed(u"a", ParserEnvironmentSeed{}, *delimiters.options);
                auto &delimiter = first(parsed);
                expect(delimiter.kind() == NodeKind::mo);
                expect(std::get<bool>(*delimiter.attributes().own(u"stretchy")) == supplied);
                expect(std::get<bool>(*delimiter.attributes().own(u"fence")) == supplied);
                expect(delimiter.properties().tex.keep_attributes.value() == u"stretchy");
            }
        };
    "parse methods control sequence dispatch reaches actual CommandMap and source metadata"_test =
        [] {
            Fixture fixture;
            installMethod(fixture, ParseMethods::controlSequence);
            auto map = std::make_shared<CommandMap>(
                CommandMapInit{MacroMapInit{{u"commands", std::nullopt}, {}}});
            map->add(u"alias",
                     Macro(u"original", MacroMethod{emitCommand}, {std::u16string(u"R")}));
            fixture.maps.registerMap(map);
            const std::array registrations{HandlerRegistration{HandlerKind::macro, {u"commands"}}};
            fixture.options->handlers.add(registrations);
            TexParser parser(u"\\alias", ParserEnvironmentSeed{}, *fixture.options);
            auto *root = parser.mml();
            expect(parser.currentCS().empty());
            expect(root->children().size() == 1_u);
            expect(NodeUtil::getText(*root->children().front()) == u"R");
            expect(attr(*root, u"data-latex") == u"\\alias");
        };
    "parse methods environment creates the real begin frame drops its first argument and pushes body"_test =
        [] {
            Fixture fixture;
            installMethod(fixture, ParseMethods::controlSequence);
            auto environments = std::make_shared<EnvironmentMap>(EnvironmentMapInit{
                MacroMapInit{{u"environments", std::nullopt}, {}}, ParseMethods::environment});
            environments->add(u"empty", Macro(u"emptyActual", MacroMethod{environmentBody}));
            environments->add(u"values",
                              Macro(u"valuesActual", MacroMethod{environmentBody},
                                    {NullArgument{}, std::u16string(u"payload"), false, 2.0}));
            fixture.maps.registerMap(environments);
            fixture.maps.registerMap(std::make_shared<CommandMap>(
                CommandMapInit{MacroMapInit{{u"commands", std::nullopt},
                                            {Macro(u"empty", MacroMethod{callEnvironment}),
                                             Macro(u"values", MacroMethod{callEnvironment})}}}));
            const std::array registrations{
                HandlerRegistration{HandlerKind::macro, {u"commands"}},
                HandlerRegistration{HandlerKind::environment, {u"environments"}}};
            fixture.options->handlers.add(registrations);
            for (auto const &source : {u"\\empty", u"\\values"}) {
                TexParser parser(source, ParserEnvironmentSeed{}, *fixture.options);
                expect(NodeUtil::getText(first(parser)) == u"E");
            }
        };
};
