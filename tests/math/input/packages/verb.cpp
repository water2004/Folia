#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.verb.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.boldsymbol.configuration;

namespace {
constexpr std::array<MacroHandler, 1> verbCallbacks{&VerbMethods::Verb};
thread_local std::pair<std::string, std::u16string> *verbError{};
thread_local int *verbTokenCalls{};
MmlNode &formatVerbError(TeX &jax, TexError const &error) {
    if (verbError) {
        verbError->first = error.id();
        verbError->second = error.message();
    }
    return jax.formatError(error);
}
MmlNode &observeVerbToken(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                          std::optional<std::u16string_view> text) {
    if (verbTokenCalls)
        ++*verbTokenCalls;
    expect(kind == NodeKind::mtext);
    expect(text.has_value());
    expect(text.value() == u"a\u00a0b");
    expect(attr(definition.attributes, u"mathvariant") == u"monospace");
    return NodeFactory::createToken(factory, kind, std::move(definition), text);
}
struct VerbFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{verbCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::pair<std::string, std::u16string> error;
    explicit VerbFixture(bool aliases = false, bool bold = false) {
        VerbConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"verb", {}}};
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        if (bold) {
            BoldsymbolConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"boldsymbol", {}});
        }
        options.formatError = &formatVerbError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, std::move(options));
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error = {};
        struct Restore {
            std::pair<std::string, std::u16string> *old{verbError};
            ~Restore() {
                verbError = old;
            }
        } restore;
        verbError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        return jax->compile(math, document);
    }
};
std::vector<MmlNode *> verbText(MmlNode &root) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == NodeKind::mtext)
            result.push_back(&node);
    });
    return result;
}
} // namespace

suite verb_package = [] {
    "verb package owns the original command map and compiled Verb method"_test = [] {
        VerbFixture f;
        auto map = std::dynamic_pointer_cast<CommandMap>(f.maps.getMap(u"verb"));
        expect(map != nullptr);
        expect(map->contains(u"verb"));
        auto entry = map->lookup(u"verb");
        expect(std::get<MacroHandler>((*std::get<Macro const *>(entry)).function) ==
               &VerbMethods::Verb);
        auto package = f.registry.get(u"verb");
        expect(package->priority == 5.0);
        for (auto const &handler : package->handler)
            expect(std::ranges::equal(handler.maps, handler.kind == HandlerKind::macro
                                                        ? std::vector<std::u16string>{u"verb"}
                                                        : std::vector<std::u16string>{}));
    };
    "verb package scans literal content and converts only ordinary spaces to NBSP"_test = [] {
        const std::array<std::pair<std::u16string_view, std::u16string_view>, 9> cases{
            {{u"\\verb|a b  c|", u"a\u00a0b\u00a0\u00a0c"},
             {u"\\verb||", u""},
             {u"\\verb+a{b}\\x%$#+", u"a{b}\\x%$#"},
             {u"\\verb*abc*", u"abc"},
             {u"\\verb \t\n|a b|", u"a\u00a0b"},
             {u"\\verb|a\tb\nc\u00a0d|", u"a\tb\nc\u00a0d"},
             {u"\\verb|A😀B|", u"A😀B"},
             {u"\\verb§a b§", u"a\u00a0b"},
             {u"\\verb aXYa", u"XY"}}};
        for (auto const &[source, expected] : cases) {
            VerbFixture f;
            auto tokens = verbText(f.compile(std::u16string(source)));
            expect(f.error.first.empty());
            expect(tokens.size() == 1_u);
            if (tokens.size() != 1)
                continue;
            expect(NodeUtil::getText(*tokens[0]) == expected);
            expect(attr(*tokens[0], u"mathvariant") == u"monospace");
            expect(tokens[0]->children().size() == 1_u);
            expect(f.jax->parseOptions().request().parsers.empty());
        }
    };
    "verb package keeps monospace through surrounding font and boldsymbol scopes"_test = [] {
        VerbFixture f(false, true);
        auto tokens = verbText(f.compile(u"x+\\mathbf{\\verb|a b|}+\\boldsymbol{\\verb|c d|}+y"));
        expect(tokens.size() == 2_u);
        expect(NodeUtil::getText(*tokens[0]) == u"a\u00a0b");
        expect(NodeUtil::getText(*tokens[1]) == u"c\u00a0d");
        for (auto token : tokens)
            expect(attr(*token, u"mathvariant") == u"monospace");
        expect(f.error.first.empty());
    };
    "verb package preserves missing and unclosed delimiter errors then recovers"_test = [] {
        const std::array<std::tuple<std::u16string_view, std::string_view, std::u16string_view>, 4>
            cases{
                {{u"\\verb", "MissingArgFor", u"Missing argument for \\verb"},
                 {u"\\verb \t\n", "MissingArgFor", u"Missing argument for \\verb"},
                 {u"\\verb|abc", "NoClosingDelim", u"Can't find closing delimiter for \\verb"},
                 {u"\\verb😀abc😀", "NoClosingDelim", u"Can't find closing delimiter for \\verb"}}};
        VerbFixture f;
        for (auto const &[source, id, message] : cases) {
            f.compile(std::u16string(source));
            expect(f.error.first == id);
            expect(std::ranges::equal(f.error.second, message));
            expect(f.jax->parseOptions().request().parsers.empty());
        }
        auto token = verbText(f.compile(u"\\verb|ok|"))[0];
        expect(f.error.first.empty());
        expect(NodeUtil::getText(*token) == u"ok");
    };
    "verb package let aliases keep original dispatch name and failed current control sequence"_test =
        [] {
            VerbFixture f(true);
            expect(NodeUtil::getText(*verbText(f.compile(u"\\let\\v\\verb\\v|a b|"))[0]) ==
                   u"a\u00a0b");
            f.compile(u"\\v");
            expect(f.error.first == "MissingArgFor");
            expect(f.error.second == u"Missing argument for \\verb");
            f.compile(u"\\v|x");
            expect(f.error.first == "NoClosingDelim");
            expect(f.error.second == u"Can't find closing delimiter for \\v");
            f.jax->reset(7);
            expect(NodeUtil::getText(*verbText(f.compile(u"\\v|ok|"))[0]) == u"ok");
            expect(f.error.first.empty());
        };
    "verb package passes literal text and monospace through the configured token creator"_test =
        [] {
            VerbFixture f;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.token = observeVerbToken});
            int calls{};
            struct Restore {
                int *old{verbTokenCalls};
                ~Restore() {
                    verbTokenCalls = old;
                }
            } restore;
            verbTokenCalls = &calls;
            f.compile(u"\\verb|a b|");
            expect(calls == 1_i);
            expect(f.error.first.empty());
        };
};
