#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.braket.configuration;
import folia.math.input.tex.braket.methods;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 3> braketCallbacks{&BraketMethods::Braket, &BraketMethods::Bar,
                                                      BraketMethods::Macro};
thread_local std::string *braketError{};
thread_local std::u16string *braketMessage{};
MmlNode &formatBraketError(TeX &jax, TexError const &error) {
    if (braketError)
        *braketError = error.id();
    if (braketMessage)
        *braketMessage = error.message();
    return jax.formatError(error);
}
struct BraketFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{braketCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    std::u16string message;
    explicit BraketFixture(bool aliases = false, bool selected = true) {
        BraketConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (selected)
            options.packages->push_back({u"braket", {}});
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        options.formatError = &formatBraketError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        message.clear();
        struct Restore {
            std::string *error{braketError};
            std::u16string *message{braketMessage};
            ~Restore() {
                braketError = error;
                braketMessage = message;
            }
        } restore;
        braketError = &error;
        braketMessage = &message;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        auto &root = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        return root;
    }
    MmlNode &ok(std::u16string source) {
        auto &root = compile(std::move(source));
        expect(error.empty()) << error;
        return root;
    }
};
std::vector<MmlNode *> braketNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
std::u16string braketText(MmlNode &root) {
    std::u16string result;
    root.walkTree([&](auto &node) {
        if (node.isToken())
            result += NodeUtil::getText(node);
    });
    return result;
}
bool braketBool(AttributeSet const &attrs, std::u16string_view key, bool expected) {
    auto value = attrs.lookup(key);
    return value && std::holds_alternative<bool>(*value) && std::get<bool>(*value) == expected;
}
std::vector<MmlNode *> markedBars(MmlNode &root) {
    auto nodes = braketNodes(root, NodeKind::mo);
    std::erase_if(
        nodes, [](auto node) { return !braketBool(node->attributes(), u"data-braketbar", true); });
    return nodes;
}
struct BraketRequest {
    NodeKind kind;
    std::optional<std::u16string> text;
    NodeDefinition definition;
    std::vector<MmlNode *> children;
    MmlNode *node{};
};
thread_local std::vector<BraketRequest> *braketRequests{};
MmlNode &observeBraketToken(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                            std::optional<std::u16string_view> text) {
    auto saved = definition;
    auto &node = NodeFactory::createToken(factory, kind, std::move(definition), text);
    if (braketRequests && (kind == NodeKind::mo || kind == NodeKind::mspace))
        braketRequests->push_back({kind,
                                   text ? std::optional<std::u16string>(*text) : std::nullopt,
                                   std::move(saved),
                                   {},
                                   &node});
    return node;
}
MmlNode &observeBraketNode(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                           NodeDefinition definition, MmlNode *text) {
    if (braketRequests && kind == NodeKind::tex_atom &&
        definition.properties.tex.explicit_tex_class)
        braketRequests->push_back(
            {kind, std::nullopt, definition, {children.begin(), children.end()}});
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
struct BraketCommand {
    std::u16string_view name, arguments, text;
};
constexpr BraketCommand braketCommands[]{
    {u"bra", u"{x}", u"⟨x|"},         {u"ket", u"{x}", u"|x⟩"}, {u"braket", u"{x}", u"⟨x⟩"},
    {u"set", u"{x}", u"{x}"},         {u"Bra", u"{x}", u"⟨x|"}, {u"Ket", u"{x}", u"|x⟩"},
    {u"Braket", u"{x}", u"⟨x⟩"},      {u"Set", u"{x}", u"{x}"}, {u"ketbra", u"{x}{y}", u"|x⟩⟨y|"},
    {u"Ketbra", u"{x}{y}", u"|x⟩⟨y|"}};
} // namespace

suite braket = [] {
    "braket original eleven command entries character map and package selection"_test = [] {
        BraketFixture f;
        expect(BraketMethods::Macro == &BaseMethods::Macro);
        auto commands = f.maps.getMap(u"Braket-macros");
        for (auto const &entry : braketCommands) {
            expect(commands->contains(entry.name));
            auto &root = f.ok(u"\\" + std::u16string(entry.name) + std::u16string(entry.arguments));
            expect(std::ranges::equal(braketText(root), entry.text));
        }
        auto bar = std::get<Macro const *>(commands->lookup(u"|"));
        expect(std::get<MacroHandler>(bar->function) == &BraketMethods::Bar);
        bar = std::get<Macro const *>(f.maps.getMap(u"Braket-characters")->lookup(u"|"));
        expect(std::get<MacroHandler>(bar->function) == &BraketMethods::Bar);
        BraketFixture absent(false, false);
        absent.compile(u"\\Braket{x}");
        expect(absent.error == "UndefinedControlSequence");
        for (auto command : {u"braket", u"set", u"Braket", u"Set"})
            for (auto argument : {u" x", u"{}"}) {
                auto &root = f.ok(u"\\" + std::u16string(command) + argument);
                expect(markedBars(root).empty());
                expect(braketNodes(root, NodeKind::mo).size() == 2_u);
            }
    };
    "braket bars preserve fixed count behavior stretchy limits double bars and nested frames"_test =
        [] {
            BraketFixture f;
            struct Case {
                std::u16string_view source, glyphs;
                std::optional<bool> stretchy;
            };
            const Case cases[]{{u"\\braket{a|b|c}", u"||", false},
                               {u"\\set{a|b|c}", u"||", false},
                               {u"\\Braket{a|b|c}", u"||", true},
                               {u"\\Set{a|b|c}", u"|", true},
                               {u"\\Braket{a||b}", u"‖", true},
                               {u"\\braket{a||b}", u"‖", false},
                               {u"\\Braket{a\\|b}", u"‖", true},
                               {u"\\braket{a\\|b}", u"‖", false},
                               {u"\\Braket{a|{bc}|d}", u"||", true},
                               {u"\\Braket{a|\\braket{b|c}|d}", u"|||", std::nullopt},
                               {u"\\Set{a|b||c}", u"|", true}};
            for (auto const &item : cases) {
                auto nodes = markedBars(f.ok(std::u16string(item.source)));
                std::u16string glyphs;
                for (auto node : nodes) {
                    glyphs += NodeUtil::getText(*node);
                    if (item.stretchy)
                        expect(braketBool(node->attributes(), u"stretchy", *item.stretchy));
                }
                expect(std::ranges::equal(glyphs, item.glyphs));
            }
            for (auto source : {u"|", u"||", u"\\|", u"{a|b}", u"\\left|x\\right|"})
                expect(markedBars(f.ok(source)).empty());
        };
    "braket Set thin spaces and current font inheritance remain scoped"_test = [] {
        BraketFixture f;
        auto spaces = braketNodes(f.ok(u"\\Set{x|y}"), NodeKind::mspace);
        expect(spaces.size() == 2_u);
        for (auto node : spaces)
            expect(attr(*node, u"width") == u"0.167em");
        auto tokens = braketNodes(f.ok(u"\\mathbf{\\Braket{x|{\\mathit{y}}|z}}w"), NodeKind::mi);
        expect(tokens.size() == 4_u);
        constexpr std::array<std::u16string_view, 4> fonts{u"bold", u"-tex-mathit", u"bold",
                                                           u"italic"};
        for (std::size_t i = 0; i < std::min(tokens.size(), fonts.size()); ++i)
            expect(std::ranges::equal(attr(*tokens[i], u"mathvariant"), fonts[i]));
    };
    "braket over fractions before between and after bars retain only their own operands"_test = [] {
        BraketFixture f;
        struct Case {
            std::u16string_view source;
            std::vector<std::pair<std::u16string_view, std::u16string_view>> fractions;
        };
        const Case cases[]{{u"\\Braket{a\\over b|c}", {{u"a", u"b"}}},
                           {u"\\Braket{a|b\\over c|d}", {{u"b", u"c"}}},
                           {u"\\Braket{a|b\\over c}", {{u"b", u"c"}}},
                           {u"\\Braket{a\\over b|c\\over d|e\\over f}",
                            {{u"a", u"b"}, {u"c", u"d"}, {u"e", u"f"}}},
                           {u"\\Set{a\\over b|c\\over d}", {{u"a", u"b"}, {u"c", u"d"}}},
                           {u"\\braket{a\\over b|c}", {{u"a", u"b|c"}}}};
        for (auto const &item : cases) {
            auto fractions = braketNodes(f.ok(std::u16string(item.source)), NodeKind::mfrac);
            expect(fractions.size() == item.fractions.size());
            for (std::size_t i = 0; i < std::min(fractions.size(), item.fractions.size()); ++i) {
                auto children = fractions[i]->children();
                expect(children.size() == 2_u);
                if (children.size() == 2) {
                    expect(std::ranges::equal(braketText(*children[0]), item.fractions[i].first));
                    expect(std::ranges::equal(braketText(*children[1]), item.fractions[i].second));
                }
            }
        }
    };
    "braket item marked close saves children while ordinary close and single replay retain original forms"_test =
        [] {
            BraketFixture f;
            auto &factory = *f.jax->parseOptions().itemFactory;
            auto frame = factory.create(StackKind::braket);
            auto &item = dynamic_cast<BraketItem &>(*frame);
            item.open = u"⟨";
            item.close = u"⟩";
            auto &x = f.jax->parseOptions().request().nodeFactory.makeToken(NodeKind::mi, u"x");
            auto mml = factory.create(StackKind::mml, MmlItemInit{x});
            expect(item.checkItem(mml).action == CheckAction::drop);
            auto close = factory.create(StackKind::close);
            dynamic_cast<CloseItem &>(*close).braket_bar = true;
            expect(item.checkItem(close).action == CheckAction::drop);
            expect(item.Size() == 0_u && item.barNodes.size() == 1_u);
            if (!item.barNodes.empty())
                expect(item.barNodes[0] == &x);
            auto &y = f.jax->parseOptions().request().nodeFactory.makeToken(NodeKind::mi, u"y");
            expect(item.checkItem(factory.create(StackKind::mml, MmlItemInit{y})).action ==
                   CheckAction::drop);
            expect(&item.toMml(false) == &y);
            auto result = item.checkItem(factory.create(StackKind::close));
            expect(result.action == CheckAction::replay && result.replacements.size() == 1_u);
            if (!result.replacements.empty())
                expect(std::holds_alternative<FrameHandle>(result.replacements[0]));
            auto one = factory.create(StackKind::braket);
            auto &single = dynamic_cast<BraketItem &>(*one);
            single.single = true;
            single.open = u"{";
            single.close = u"}";
            auto &z = f.jax->parseOptions().request().nodeFactory.makeToken(NodeKind::mi, u"z");
            result = single.checkItem(factory.create(StackKind::mml, MmlItemInit{z}));
            expect(result.action == CheckAction::replay && result.replacements.size() == 1_u);
            if (!result.replacements.empty())
                expect(std::holds_alternative<MmlNode *>(result.replacements[0]));
        };
    "braket configured creators receive fence and bar classes before construction and empty space text"_test =
        [] {
            BraketFixture f;
            std::vector<BraketRequest> requests;
            struct Restore {
                ~Restore() {
                    braketRequests = nullptr;
                }
            } restore;
            braketRequests = &requests;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.node = &observeBraketNode, .token = &observeBraketToken});
            auto &root = f.ok(u"\\braket{a|b}");
            expect(requests.size() == 3_u);
            if (requests.size() == 3) {
                constexpr std::array classes{TexClass::ord, TexClass::open, TexClass::close};
                for (std::size_t i = 0; i < 3; ++i) {
                    expect(requests[i].definition.properties.tex.explicit_tex_class == classes[i]);
                    expect(braketBool(requests[i].definition.attributes, u"stretchy", false));
                    if (i > 0) {
                        expect(braketBool(requests[i].definition.attributes, u"fence", true));
                        expect(braketBool(requests[i].definition.attributes, u"symmetric", true));
                    }
                }
                auto bars = markedBars(root);
                if (!bars.empty())
                    expect(bars[0] == requests[0].node);
            }
            requests.clear();
            f.ok(u"\\Braket{a|b}");
            expect(requests.size() == 3_u);
            if (requests.size() == 3) {
                expect(requests[0].definition.properties.tex.explicit_tex_class == TexClass::close);
                expect(requests[0].children.empty());
                expect(requests[1].definition.properties.tex.explicit_tex_class == TexClass::bin);
                expect(requests[2].definition.properties.tex.explicit_tex_class == TexClass::open);
                expect(requests[2].children.empty());
            }
            requests.clear();
            f.ok(u"\\Set{a|b}");
            std::size_t spaces = 0;
            for (auto const &request : requests)
                if (request.kind == NodeKind::mspace) {
                    ++spaces;
                    expect(request.text.has_value());
                    if (request.text)
                        expect(request.text->empty());
                    expect(attr(request.definition.attributes, u"width") == u"0.167em");
                    expect(request.node && request.node->children().empty());
                    if (request.node)
                        expect(NodeUtil::getText(*request.node).empty());
                }
            expect(spaces == 2_u);
        };
    "braket argument and child errors preserve original order and recover with no parser borrows"_test =
        [] {
            BraketFixture f;
            for (auto const &command : braketCommands) {
                f.compile(u"\\" + std::u16string(command.name));
                expect(f.error == "MissingArgFor");
                f.ok(u"\\Braket{x|y}");
            }
            struct Case {
                std::u16string_view source;
                std::string_view error;
            };
            for (auto const &item : {Case{u"\\Braket{a|b", "MissingCloseBrace"},
                                     {u"\\Braket{\\unknown|b}", "UndefinedControlSequence"},
                                     {u"\\Braket{a|{b|c}|d}", "ExtraCloseMissingOpen"},
                                     {u"\\Braket{x\\right)}", "MissingLeftExtraRight"},
                                     {u"\\Braket{x&y}", "Misplaced"},
                                     {u"\\Braket{a\\over b\\over c}", "AmbiguousUseOf"},
                                     {u"\\Ketbra{x}", "MissingArgFor"}}) {
                f.compile(std::u16string(item.source));
                expect(f.error == item.error);
                f.ok(u"\\Braket{x|y}");
            }
        };
    "braket fixed callback let aliases keep bracket parameters control bars and scanner input names"_test =
        [] {
            BraketFixture f(true);
            f.ok(u"\\let\\saved\\Braket\\let\\savedBar\\|\\let\\savedBra\\bra");
            auto bars = markedBars(f.ok(u"\\saved{x\\savedBar y}"));
            expect(bars.size() == 1_u);
            if (!bars.empty())
                expect(NodeUtil::getText(*bars[0]) == u"‖");
            expect(braketText(f.ok(u"\\savedBra{x}")) == u"⟨x|");
            f.compile(u"\\saved");
            expect(f.error == "MissingArgFor");
            expect(f.message == u"Missing argument for \\saved");
            f.ok(u"\\saved{x|y}");
        };
};
