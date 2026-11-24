#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.textmacros.configuration;
import folia.math.input.tex.color.configuration;
import folia.math.input.tex.html.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.configmacros.configuration;

namespace {
constexpr std::array<MacroHandler, 11> textCallbacks{
    TextMacrosMethods::Macro,     TextMacrosMethods::Spacer,  TextMacrosMethods::Hskip,
    TextMacrosMethods::rule,      TextMacrosMethods::Rule,    TextMacrosMethods::HandleRef,
    TextMacrosMethods::UnderOver, TextMacrosMethods::Lap,     TextMacrosMethods::Phantom,
    TextMacrosMethods::Smash,     TextMacrosMethods::MmlToken};
thread_local std::string *textError{};
MmlNode &formatTextError(TeX &jax, TexError const &error) {
    if (textError)
        *textError = error.id();
    return jax.formatError(error);
}
struct TextFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{textCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit TextFixture(bool extras = false, bool aliases = false, bool childStyle = false) {
        TextMacrosConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"textmacros", {}}};
        options.maxMacros = 100;
        if (childStyle) {
            ConfigurationDefinition textStyle;
            textStyle.parser = u"text";
            textStyle.options.mathStyle = u"ISO";
            Configuration::create(registry, u"text-style-test", std::move(textStyle));
            options.mathStyle = u"French";
            options.textmacros =
                TextMacrosOptions{.packages = std::vector<PackageSelection>{
                                      {u"text-base", {}}, {u"text-style-test", {}}}};
        }
        if (extras) {
            ColorConfiguration::initializeModule(registry, maps);
            HtmlConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"color", {}});
            options.packages->push_back({u"html", {}});
        }
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            ConfigMacrosConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
            options.packages->push_back({u"configmacros", {}});
            MacroDefinition fixed;
            fixed.replacement_source = u"Fixed #1";
            fixed.arity = 1;
            options.macros = std::unordered_map<std::u16string, MacroDefinition>{{u"fixed", fixed}};
        }
        options.formatError = &formatTextError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    TextMacrosData &data() {
        return *jax->parseOptions().textmacros;
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *previous{textError};
            ~Restore() {
                textError = previous;
            }
        } restore;
        textError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        auto &root = jax->compile(math, document);
        expect(jax->parseOptions().request().parsers.empty());
        expect(data().parseOptions->request().parsers.empty());
        expect(data().parseOptions->request().texParser == nullptr);
        return root;
    }
    MmlNode &ok(std::u16string source) {
        auto &root = compile(std::move(source));
        expect(error.empty()) << error;
        return root;
    }
    MmlNode &text(std::u16string_view source) {
        return ok(u"\\text{" + std::u16string(source) + u"}");
    }
};
std::vector<MmlNode *> textNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
std::u16string allText(MmlNode &root) {
    std::u16string result;
    root.walkTree([&](auto &node) {
        if (node.isToken())
            result += NodeUtil::getText(node);
    });
    return result;
}
bool textNumber(AttributeSet const &attributes, std::u16string_view key, double expected) {
    auto value = attributes.lookup(key);
    return value && std::holds_alternative<double>(*value) && std::get<double>(*value) == expected;
}
struct TextRequest {
    NodeKind kind;
    NodeDefinition definition;
    MmlNode *node;
    bool levelPresent;
};
struct TextMappingCase {
    std::u16string_view map, name;
    MacroHandler method;
    std::vector<TexArgument> arguments;
};
#include "math/fixtures/textmacros-mappings.inc"
thread_local std::vector<TextRequest> *textRequests{};
MmlNode &observeTextNode(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                         NodeDefinition definition, MmlNode *text) {
    auto saved = definition;
    auto active =
        dynamic_cast<TextParser *>(dynamic_cast<ParseOptions &>(factory.configuration()).parser());
    const bool levelPresent = active && active->level().has_value();
    auto &node = NodeFactory::createNode(factory, kind, children, std::move(definition), text);
    if (textRequests && (kind == NodeKind::mtext || kind == NodeKind::mo))
        textRequests->push_back({kind, std::move(saved), &node, levelPresent});
    return node;
}
} // namespace

suite textmacros = [] {
    "textmacros every original character and command mapping keeps its compiled callback and typed arguments"_test =
        [] {
            TextFixture f;
            for (auto const &entry : textMappingCases) {
                auto lookup = f.maps.getMap(entry.map)->lookup(entry.name);
                auto macro = std::get_if<Macro const *>(&lookup);
                expect(macro && *macro);
                if (macro && *macro) {
                    expect(std::get<MacroHandler>((*macro)->function) == entry.method);
                    expect(std::ranges::equal((*macro)->arguments, entry.arguments));
                }
            }
        };
    "textmacros child configuration captures its own style while sharing the configured main options"_test =
        [] {
            TextFixture f(false, false, true);
            expect(f.jax->options.mathStyle == u"French");
            expect(&f.data().parseOptions->options == &f.jax->options);
            expect(f.data().parseOptions->mathStyle == ParseOptions::getVariant(u"ISO"));
            f.text(u"abc");
        };
    "textmacros text parsing preserves quotes whitespace comments escapes Unicode and inserts"_test =
        [] {
            TextFixture f;
            struct Case {
                std::u16string_view input, output;
            };
            for (auto const &item : {Case{u"", u""},
                                     {u"abc 日本😀", u"abc 日本😀"},
                                     {u" a   b\t\nc ", u"\u00a0a b c\u00a0"},
                                     {u"``double'' `single'", u"“double” ‘single’"},
                                     {u"a%comment\nb", u"ab"},
                                     {u"a%tail", u"a"},
                                     {u"a~b", u"a\u00a0b"},
                                     {u"\\$\\_\\%\\{\\}\\ \\&\\#", u"$_%{} &#"},
                                     {u"\\dagger\\ddagger\\S\\AA\\ldots\\vdots", u"†‡§Å…⋮"}})
                expect(std::ranges::equal(allText(f.text(item.input)), item.output));
            expect(textNodes(f.text(u""), NodeKind::mtext).empty());
        };
    "textmacros grouped fonts emphasis and argument fonts restore the previous environment"_test =
        [] {
            TextFixture f;
            auto nodes =
                textNodes(f.text(u"a{\\bf b}c\\emph{d\\emph{e}}f\\frak{g}h"), NodeKind::mtext);
            constexpr std::array<std::u16string_view, 8> fonts{u"normal",      u"bold",   u"normal",
                                                               u"-tex-mathit", u"normal", u"normal",
                                                               u"fraktur",     u"normal"};
            expect(nodes.size() == fonts.size());
            for (std::size_t i = 0; i < std::min(nodes.size(), fonts.size()); ++i)
                expect(std::ranges::equal(attr(*nodes[i], u"mathvariant"), fonts[i]));
            for (auto const &[command, font] :
                 {std::pair<std::u16string_view, std::u16string_view>{u"rm", u"normal"},
                  {u"mit", u"italic"},
                  {u"oldstyle", u"-tex-oldstyle"},
                  {u"cal", u"-tex-calligraphic"},
                  {u"it", u"-tex-mathit"},
                  {u"bf", u"bold"},
                  {u"sf", u"sans-serif"},
                  {u"tt", u"monospace"},
                  {u"Bbb", u"double-struck"}}) {
                const auto argument =
                    std::ranges::equal(command, std::u16string_view(u"Bbb")) ? u"{x}" : u" x";
                nodes =
                    textNodes(f.text(u"\\" + std::u16string(command) + argument), NodeKind::mtext);
                expect(nodes.size() == 1_u);
                if (!nodes.empty())
                    expect(std::ranges::equal(attr(*nodes[0], u"mathvariant"), font));
            }
        };
    "textmacros all fourteen sizes remain numeric values on the actual text nodes"_test = [] {
        TextFixture f;
        const std::pair<std::u16string_view, double> sizes[]{
            {u"Tiny", .5},     {u"tiny", .6},         {u"scriptsize", .7}, {u"SMALL", .7},
            {u"Small", .8},    {u"footnotesize", .8}, {u"small", .9},      {u"normalsize", 1},
            {u"large", 1.095}, {u"Large", 1.2},       {u"LARGE", 1.44},    {u"huge", 1.73},
            {u"Huge", 2.07},   {u"HUGE", 2.49}};
        for (auto const &[command, size] : sizes) {
            auto nodes =
                textNodes(f.text(u"\\" + std::u16string(command) + u" x"), NodeKind::mtext);
            expect(nodes.size() == 1_u);
            if (!nodes.empty())
                expect(textNumber(nodes[0]->attributes(), u"mathsize", size));
        }
    };
    "textmacros accents create real mover slots and both embedded math delimiters reach TeX"_test =
        [] {
            TextFixture f;
            for (auto const &[command, glyph] :
                 {std::pair<std::u16string_view, std::u16string_view>{u"'", u"´"},
                  {u"’", u"´"},
                  {u"`", u"`"},
                  {u"‘", u"`"},
                  {u"^", u"^"},
                  {u"\"", u"¨"},
                  {u"~", u"~"},
                  {u"=", u"¯"},
                  {u".", u"˙"},
                  {u"u", u"˘"},
                  {u"v", u"ˇ"}}) {
                auto nodes =
                    textNodes(f.text(u"\\" + std::u16string(command) + u"{a}"), NodeKind::mover);
                expect(nodes.size() == 1_u);
                if (!nodes.empty() && nodes[0]->children().size() == 2) {
                    expect(allText(*nodes[0]->children()[0]) == u"a");
                    expect(std::ranges::equal(allText(*nodes[0]->children()[1]), glyph));
                }
            }
            for (auto source :
                 {u"a $x+1$ b", u"a \\(x+1\\) b", u"$x_{y}$", u"$\\{x\\}$", u"$\\$+x$"}) {
                auto &tree = f.text(source);
                expect(!textNodes(tree, NodeKind::tex_atom).empty());
                auto tokens = textNodes(tree, NodeKind::mi);
                expect(std::ranges::any_of(
                    tokens, [](auto node) { return NodeUtil::getText(*node) == u"x"; }));
            }
        };
    "textmacros eleven Base aliases operate on the real text parser graph"_test = [] {
        TextFixture f;
        constexpr std::array<MacroHandler, 11> originals{
            &BaseMethods::Macro,     &BaseMethods::Spacer,  &BaseMethods::Hskip,
            &BaseMethods::rule,      &BaseMethods::Rule,    &BaseMethods::HandleRef,
            &BaseMethods::UnderOver, &BaseMethods::Lap,     &BaseMethods::Phantom,
            &BaseMethods::Smash,     &BaseMethods::MmlToken};
        expect(std::ranges::equal(textCallbacks, originals));
        const std::pair<std::u16string_view, NodeKind> cases[]{
            {u"\\,", NodeKind::mspace},
            {u"\\hskip 1em", NodeKind::mspace},
            {u"\\rule{1em}{2em}", NodeKind::mspace},
            {u"\\Rule{1em}{2em}{0em}", NodeKind::mspace},
            {u"\\phantom{x}", NodeKind::mphantom},
            {u"\\smash{x}", NodeKind::mpadded},
            {u"\\llap{x}", NodeKind::mpadded},
            {u"\\underline{x}", NodeKind::munder},
            {u"\\mmlToken{mi}[mathcolor=blue]{x}", NodeKind::mi},
            {u"\\ref{missing}", NodeKind::mtext},
            {u"\\eqref{missing}", NodeKind::mtext}};
        for (auto const &[source, kind] : cases)
            expect(!textNodes(f.text(source), kind).empty());
    };
    "textmacros loaded Color and HTML macros forward to the text receiver and restore group color"_test =
        [] {
            TextFixture f(true);
            auto nodes = textNodes(
                f.text(u"a{\\color{red}b}c\\textcolor{blue}{d}\\href{target}{e}"), NodeKind::mtext);
            constexpr std::array<std::u16string_view, 5> colors{u"", u"red", u"", u"blue", u""};
            expect(nodes.size() == colors.size());
            for (std::size_t i = 0; i < std::min(nodes.size(), colors.size()); ++i)
                expect(std::ranges::equal(attr(*nodes[i], u"mathcolor"), colors[i]));
            if (nodes.size() == 5)
                expect(attr(*nodes[4], u"href") == u"target");
        };
    "textmacros errors restore both parser lists and primary borrow before following formulas"_test =
        [] {
            const std::pair<std::u16string_view, std::string_view> cases[]{
                {u"^", "MathModeOnly"},
                {u"_", "MathModeOnly"},
                {u"&", "Misplaced"},
                {u"#", "Misplaced"},
                {u"$x", "MathNotTerminated"},
                {u"\\(x", "MathNotTerminated"},
                {u"$x}", "MathNotTerminated"},
                {u"\\unknown", "UndefinedControlSequence"},
                {u"\\frac{a}{b}", "MathMacro"},
                {u"\\href{x}{y}", "UndefinedControlSequence"},
                {u"\\bf\\emph", "MissingArgFor"},
                {u"\\u", "MissingArgFor"}};
            for (auto const &[source, error] : cases) {
                TextFixture f;
                f.compile(u"\\text{" + std::u16string(source) + u"}");
                expect(f.error == error);
                f.text(u"ok");
            }
        };
    "textmacros replacement definitions and fixed callback let captures remain owned across formulas"_test =
        [] {
            TextFixture f(false, true);
            f.ok(u"\\newcommand{\\word}[1]{Hello #1}\\let\\saved\\word");
            expect(allText(f.text(u"\\word{A} \\saved{B}")) == u"Hello A Hello B");
            f.ok(u"\\let\\captured\\fixed\\renewcommand{\\fixed}[1]{Changed #1}");
            expect(allText(f.text(u"\\captured{A} \\fixed{B}")) == u"Fixed A Changed B");
        };
    "textmacros configured creators and copied node lists retain actual accent identity"_test = [] {
        TextFixture f;
        std::vector<TextRequest> requests;
        struct Restore {
            ~Restore() {
                textRequests = nullptr;
            }
        } restore;
        textRequests = &requests;
        f.data().parseOptions->request().nodeFactory.setCreators(
            NodeCreators{.node = &observeTextNode});
        auto &tree = f.text(u"\\bf\\large a\\u{b}$x$");
        expect(std::ranges::any_of(requests, [](auto const &r) {
            return r.kind == NodeKind::mtext &&
                   attr(r.definition.attributes, u"mathvariant") == u"bold";
        }));
        auto accent =
            std::ranges::find_if(requests, [](auto const &r) { return r.kind == NodeKind::mo; });
        expect(accent != requests.end());
        if (accent != requests.end()) {
            auto nodes = textNodes(tree, NodeKind::mo);
            expect(std::ranges::find(nodes, accent->node) != nodes.end());
            auto list = f.jax->parseOptions().request().nodeLists.find(u"mo");
            expect(list != f.jax->parseOptions().request().nodeLists.end());
            if (list != f.jax->parseOptions().request().nodeLists.end())
                expect(std::ranges::find(list->second, accent->node) != list->second.end());
            expect(textNumber(accent->node->attributes(), u"mathsize", 1.095));
            expect(attr(*accent->node, u"mathvariant") == u"bold");
        }
    };
    "textmacros real TextParser construction assigns level after parsing protects explicit attributes and reparents rows"_test =
        [] {
            TextFixture f;
            auto &inner = *f.data().parseOptions;
            TexParser primary(u"", {}, f.jax->parseOptions());
            primary.mml();
            struct Restore {
                TextMacrosData &data;
                ~Restore() {
                    data.parseOptions->request().texParser = nullptr;
                }
            } restore{f.data()};
            inner.request().texParser = &primary;
            std::vector<TextRequest> requests;
            struct RestoreRequests {
                ~RestoreRequests() {
                    textRequests = nullptr;
                }
            } restoreRequests;
            textRequests = &requests;
            inner.request().nodeFactory.setCreators(NodeCreators{.node = &observeTextNode});
            TextParser p(u"a", {}, inner, ScriptLevel{2.0});
            auto *node = p.mml();
            expect(node->kind() == NodeKind::mstyle);
            expect(textNumber(node->attributes(), u"scriptlevel", 2));
            expect(!requests.empty() && !requests[0].levelPresent);
            TexEnvironment environment;
            environment.math_size = 2.0;
            environment.math_color = u"red";
            environment.math_variant = u"bold";
            TextParser q(u"", ParserEnvironmentSeed{environment}, inner);
            NodeDefinition explicitDefinition;
            explicitDefinition.attributes.set(u"mathsize", 0.0);
            explicitDefinition.attributes.set(u"mathcolor", u"blue");
            explicitDefinition.attributes.set(u"mathvariant", u"italic");
            auto &token = inner.request().nodeFactory.makeToken(NodeKind::mi, u"x",
                                                                std::move(explicitDefinition));
            q.addAttributes(token);
            expect(textNumber(token.attributes(), u"mathsize", 0));
            expect(attr(token, u"mathcolor") == u"blue");
            expect(attr(token, u"mathvariant") == u"italic");
            q.mml();
            TextParser r(u"", {}, inner);
            auto &a = inner.request().nodeFactory.makeToken(NodeKind::mi, u"a");
            auto &b = inner.request().nodeFactory.makeToken(NodeKind::mi, u"b");
            const std::array children{&a, &b};
            auto &inferred =
                inner.request().nodeFactory.makeNode(NodeKind::inferred_mrow, children);
            r.PushMath(inferred);
            auto *math = r.mml();
            expect(math->kind() == NodeKind::tex_atom);
            auto rows = textNodes(*math, NodeKind::mrow);
            expect(rows.size() == 1_u);
            if (rows.size() == 1 && rows[0]->children().size() == 2) {
                expect(rows[0]->children()[0] == &a);
                expect(rows[0]->children()[1] == &b);
            }
            for (auto source : {u"}", u"$x}"})
                texError([&] { TextParser parser(source, {}, inner); }, "ExtraCloseMissingOpen");
            expect(inner.request().parsers.empty());
        };
};
