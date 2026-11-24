#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.bboldx.configuration;
import folia.math.input.tex.bboldx.methods;
import folia.math.input.tex.textmacros.configuration;
import folia.math.input.tex.newcommand.configuration;

namespace {
struct BboldxCharacterCase {
    std::u16string_view map, name, glyph;
    TokenInputHandler parser;
    NodeKind kind;
    std::array<std::u16string_view, 4> fonts;
    bool text;
};
struct BboldxCommandCase {
    std::u16string_view map, name;
    MacroHandler parser;
    std::array<std::u16string_view, 3> arguments;
    std::size_t count;
    std::u16string_view glyph;
    NodeKind kind;
    std::array<std::u16string_view, 4> fonts;
    bool text;
};
#include "math/fixtures/bboldx.inc"
constexpr std::array<MacroHandler, 3> bbCallbacks{BboldxMethods::Macro, &BboldxMethods::ChooseFont,
                                                  &BboldxMethods::ChooseTextFont};
constexpr std::array<TokenInputHandler, 4> bbTokenCallbacks{
    &BboldxMethods::mathchar0miNormal, &BboldxMethods::delimiterNormal,
    &BboldxMethods::mathchar0miBold, &BboldxMethods::delimiterBold};
thread_local std::string *bbError{};
MmlNode &formatBboldxError(TeX &jax, TexError const &error) {
    if (bbError)
        *bbError = error.id();
    return jax.formatError(error);
}
struct BboldxFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{bbCallbacks, bbTokenCallbacks};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit BboldxFixture(bool text = true, bool bfbb = false, bool light = false,
                           bool aliases = false, bool reversed = false, bool selected = true) {
        BboldxConfiguration::initializeModule(registry, maps);
        TextMacrosConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages.emplace();
        if (selected && reversed)
            options.packages->push_back({u"bboldx", {}});
        options.packages->push_back({u"base", {}});
        if (text)
            options.packages->push_back({u"textmacros", {}});
        if (selected && !reversed)
            options.packages->push_back({u"bboldx", {}});
        if (selected)
            options.bboldx = BboldxOptions{.bfbb = bfbb, .light = light};
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        options.formatError = &formatBboldxError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *previous{bbError};
            ~Restore() {
                bbError = previous;
            }
        } restore;
        bbError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        auto &root = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        if (auto data = jax->parseOptions().textmacros) {
            expect(data->parseOptions->request().texParser == nullptr);
            expect(data->parseOptions->request().parsers.empty());
        }
        return root;
    }
    MmlNode &ok(std::u16string source) {
        auto &root = compile(std::move(source));
        expect(error.empty()) << error;
        return root;
    }
};
std::vector<MmlNode *> bbNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
MmlNode *bbFontNode(MmlNode &root) {
    std::vector<MmlNode *> nodes;
    root.walkTree([&](auto &node) {
        if (node.isToken() && attr(node, u"mathvariant").starts_with(u"-bboldx"))
            nodes.push_back(&node);
    });
    expect(nodes.size() == 1_u);
    return nodes.size() == 1 ? nodes[0] : nullptr;
}
std::u16string bbSource(std::u16string_view name, bool text, bool argument = false) {
    auto source = u"\\" + std::u16string(name) + (argument ? u"{x}" : u"");
    return text ? u"\\text{" + source + u"}" : source;
}
bool bbFalse(AttributeSet const &attrs, std::u16string_view key) {
    auto value = attrs.lookup(key);
    return value && std::holds_alternative<bool>(*value) && !std::get<bool>(*value);
}
struct BbRequest {
    NodeKind kind;
    NodeDefinition definition;
    std::u16string text;
    MmlNode *node;
};
thread_local std::vector<BbRequest> *bbRequests{};
MmlNode &observeBboldx(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                       std::optional<std::u16string_view> text) {
    auto saved = definition;
    auto &node = NodeFactory::createToken(factory, kind, std::move(definition), text);
    if (bbRequests && text &&
        (std::ranges::equal(*text, std::u16string_view(u"Γ")) ||
         std::ranges::equal(*text, std::u16string_view(u"("))))
        bbRequests->push_back({kind, std::move(saved), std::u16string(*text), &node});
    return node;
}
} // namespace

suite bboldx = [] {
    "bboldx all ten maps retain independent Source character rows and original command arguments"_test =
        [] {
            BboldxFixture f;
            expect(std::size(bboldxCharacters) == 168_u);
            expect(std::size(bboldxCommands) == 12_u);
            expect(BboldxMethods::Macro == &BaseMethods::Macro);
            for (auto const &entry : bboldxCharacters) {
                auto map = f.maps.getMap(entry.map);
                expect(map != nullptr);
                if (!map)
                    continue;
                expect(std::get<TokenInputHandler>(*map->parser()) == entry.parser);
                auto token = std::get<Token const *>(map->lookup(entry.name));
                expect(token && token->character.has_value());
                if (token && token->character)
                    expect(std::ranges::equal(*token->character, entry.glyph));
            }
            for (auto const &entry : bboldxCommands) {
                auto macro = std::get<Macro const *>(f.maps.getMap(entry.map)->lookup(entry.name));
                expect(std::get<MacroHandler>(macro->function) == entry.parser);
                expect(macro->arguments.size() == entry.count);
                for (std::size_t i = 0; i < std::min(macro->arguments.size(), entry.count); ++i)
                    expect(std::ranges::equal(std::get<std::u16string>(macro->arguments[i]),
                                              entry.arguments[i]));
            }
            auto lambda = std::get<Token const *>(
                f.maps.getMap(u"bboldx-mathchar0miBold")->lookup(u"bfbblambda"));
            expect(lambda->character &&
                   std::ranges::equal(*lambda->character, std::u16string_view(u"οBB")));
        };
    "bboldx every character and command compiles in all four option combinations with Source fonts"_test =
        [] {
            for (std::size_t i = 0; i < 4; ++i) {
                BboldxFixture math(false, i >= 2, i % 2 != 0), text(true, i >= 2, i % 2 != 0);
                for (auto const &entry : bboldxCharacters) {
                    auto &fixture = entry.text ? text : math;
                    auto node = bbFontNode(fixture.ok(bbSource(entry.name, entry.text)));
                    if (!node)
                        continue;
                    expect(node->kind() == entry.kind);
                    expect(std::ranges::equal(NodeUtil::getText(*node), entry.glyph));
                    expect(std::ranges::equal(attr(*node, u"mathvariant"), entry.fonts[i]));
                    if (entry.kind == NodeKind::mo)
                        expect(bbFalse(node->attributes(), u"stretchy"));
                }
                for (auto const &entry : bboldxCommands) {
                    auto &fixture = entry.text ? text : math;
                    auto node =
                        bbFontNode(fixture.ok(bbSource(entry.name, entry.text, entry.count == 3)));
                    if (!node)
                        continue;
                    expect(node->kind() == entry.kind);
                    expect(std::ranges::equal(NodeUtil::getText(*node), entry.glyph));
                    expect(std::ranges::equal(attr(*node, u"mathvariant"), entry.fonts[i]));
                }
            }
        };
    "bboldx priority selection text registration and shared options preserve font scope"_test = [] {
        for (bool reversed : {false, true}) {
            BboldxFixture f(true, false, false, false, reversed);
            auto node = bbFontNode(f.ok(u"\\mathbb{x}"));
            if (node)
                expect(attr(*node, u"mathvariant") == u"-bboldx");
            auto data = f.jax->parseOptions().textmacros;
            expect(&data->parseOptions->options == &f.jax->options);
            auto const &packages = *data->parseOptions->options.textmacros->packages;
            expect(packages.size() == 2_u);
            if (packages.size() == 2)
                expect(std::ranges::equal(packages[1].name, std::u16string_view(u"text-bboldx")));
            auto tokens = bbNodes(f.ok(u"\\mathbf{\\mathbb{x}y}z"), NodeKind::mi);
            expect(tokens.size() == 3_u);
            if (tokens.size() == 3) {
                expect(attr(*tokens[0], u"mathvariant") == u"-bboldx");
                expect(attr(*tokens[1], u"mathvariant") == u"bold");
                expect(attr(*tokens[2], u"mathvariant") == u"italic");
            }
            f.jax->options.bboldx->bfbb = true;
            f.jax->options.bboldx->light = true;
            node = bbFontNode(f.ok(u"\\text{\\textbb{x}}"));
            if (node)
                expect(attr(*node, u"mathvariant") == u"-bboldx-bold");
            expect(packages.size() == 2_u);
        }
        BboldxFixture absent(true, false, false, false, false, false), math(false);
        auto tokens = bbNodes(absent.ok(u"\\mathbb{x}"), NodeKind::mi);
        if (!tokens.empty())
            expect(attr(*tokens[0], u"mathvariant") == u"double-struck");
        absent.compile(u"\\bbGamma");
        expect(absent.error == "UndefinedControlSequence");
        expect(math.jax->parseOptions().textmacros == nullptr);
        auto texts = bbNodes(math.ok(u"\\text{\\textbb{x}}"), NodeKind::mtext);
        if (!texts.empty())
            expect(NodeUtil::getText(*texts[0]) == u"\\textbb{x}");
    };
    "bboldx real token creators receive explicit variants and delimiter flag with stable node borrows"_test =
        [] {
            BboldxFixture f;
            std::vector<BbRequest> requests;
            struct Restore {
                ~Restore() {
                    bbRequests = nullptr;
                }
            } restore;
            bbRequests = &requests;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.token = &observeBboldx});
            auto &tree = f.ok(u"\\bbGamma+\\bfbbLparen");
            expect(requests.size() == 2_u);
            if (requests.size() == 2) {
                expect(requests[0].kind == NodeKind::mi);
                expect(attr(requests[0].definition.attributes, u"mathvariant") == u"-bboldx");
                expect(requests[1].kind == NodeKind::mo);
                expect(attr(requests[1].definition.attributes, u"mathvariant") == u"-bboldx-bold");
                expect(bbFalse(requests[1].definition.attributes, u"stretchy"));
                auto identifiers = bbNodes(tree, NodeKind::mi),
                     operators = bbNodes(tree, NodeKind::mo);
                expect(std::ranges::find(identifiers, requests[0].node) != identifiers.end());
                expect(std::ranges::find(operators, requests[1].node) != operators.end());
            }
        };
    "bboldx missing arguments and child errors release parser borrowing and recover next formula"_test =
        [] {
            for (auto const &[source, error] :
                 {std::pair<std::u16string_view, std::string_view>{u"\\mathbb", "MissingArgFor"},
                  {u"\\mathbfbb", "MissingArgFor"},
                  {u"\\text{\\textbb}", "MissingArgFor"},
                  {u"\\text{\\textbfbb}", "MissingArgFor"},
                  {u"\\mathbb{\\unknown}", "UndefinedControlSequence"}}) {
                BboldxFixture f;
                f.compile(std::u16string(source));
                expect(f.error == error);
                f.ok(u"\\mathbb{x}");
            }
        };
    "bboldx fixed macro and token callback tables support let captures across later option changes"_test =
        [] {
            BboldxFixture f(true, false, false, true);
            for (auto callback : bbCallbacks)
                expect(f.callbacks.macroHandler(f.callbacks.macroBinding(callback)) == callback);
            for (auto callback : bbTokenCallbacks)
                expect(f.callbacks.tokenParser(f.callbacks.tokenBinding(callback)) == callback);
            f.ok(u"\\let\\savedFont\\mathbb\\let\\savedChar\\bbGamma\\let\\savedDelim\\bfbbLparen\\"
                 u"let\\savedMacro\\imathbb"
                 u"\\let\\savedBold\\bfbbGamma\\let\\savedNormalDelim\\bbLparen");
            for (auto const &[command, glyph] :
                 {std::pair<std::u16string_view, std::u16string_view>{u"savedFont{x}", u"x"},
                  {u"savedChar", u"Γ"},
                  {u"savedDelim", u"("},
                  {u"savedMacro", u"ı"},
                  {u"savedBold", u"Γ"},
                  {u"savedNormalDelim", u"("}}) {
                auto node = bbFontNode(f.ok(u"\\" + std::u16string(command)));
                if (node)
                    expect(std::ranges::equal(NodeUtil::getText(*node), glyph));
            }
            f.jax->options.bboldx->light = true;
            auto node = bbFontNode(f.ok(u"\\savedChar"));
            if (node)
                expect(attr(*node, u"mathvariant") == u"-bboldx-light");
            node = bbFontNode(f.ok(u"\\savedDelim"));
            if (node)
                expect(attr(*node, u"mathvariant") == u"-bboldx");
            node = bbFontNode(f.ok(u"\\savedBold"));
            if (node)
                expect(attr(*node, u"mathvariant") == u"-bboldx");
            node = bbFontNode(f.ok(u"\\savedNormalDelim"));
            if (node)
                expect(attr(*node, u"mathvariant") == u"-bboldx-light");
            f.compile(u"\\text{\\savedChar}");
            expect(f.error == "MathMacro");
            f.ok(u"\\text{\\txtbbGamma}");
        };
};
