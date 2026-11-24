#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.noundefined.configuration;

namespace {
thread_local std::string *undefinedError{};
thread_local std::vector<std::string> *creatorEvents{};
MmlNode &formatUndefinedError(TeX &jax, TexError const &error) {
    if (undefinedError)
        *undefinedError = error.id();
    return jax.formatError(error);
}
MmlNode *observeText(NodeFactory &factory, std::optional<std::u16string_view> value) {
    if (creatorEvents && value == u"\\missing")
        creatorEvents->push_back("text");
    return NodeFactory::createText(factory, value);
}
MmlNode &observeToken(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                      std::optional<std::u16string_view> value) {
    if (creatorEvents && kind == NodeKind::mtext)
        creatorEvents->push_back("token");
    return NodeFactory::createToken(factory, kind, std::move(definition), value);
}
MmlNode &observeNode(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                    NodeDefinition definition, MmlNode *text) {
    if (creatorEvents && kind == NodeKind::mtext) {
        creatorEvents->push_back("node");
        expect(text != nullptr);
        expect(NodeUtil::getText(*text) == u"\\missing");
        expect(attr(definition.attributes, u"mathcolor") == u"red");
    }
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
struct UndefinedFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit UndefinedFixture(TexOptionValues options = {}, bool enabled = true) {
        NoUndefinedConfiguration::initializeModule(registry);
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (enabled)
            options.packages->push_back({u"noundefined", {}});
        options.formatError = &formatUndefinedError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, std::move(options));
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{undefinedError};
            ~Restore() { undefinedError = old; }
        } restore;
        undefinedError = &error;
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        return jax->compile(math, document);
    }
};
std::vector<MmlNode *> undefinedText(MmlNode &root) {
    std::vector<MmlNode *> tokens;
    root.walkTree([&](auto &node) {
        if (node.kind() == NodeKind::mtext)
            tokens.push_back(&node);
    });
    return tokens;
}
}

suite noundefined_package = [] {
    "noundefined package registers only original macro fallback and priority three"_test = [] {
        UndefinedFixture f;
        auto package = f.registry.get(u"noundefined");
        expect(package->priority == 3.0);
        expect(package->fallback.size() == 1_u);
        expect(package->fallback[0].first == HandlerKind::macro);
        for (auto const &handler : package->handler)
            expect(handler.maps.empty());
        expect(f.jax->options.noundefined->color.value() == u"red");
        expect(f.jax->options.noundefined->background->empty());
        expect(f.jax->options.noundefined->size->empty());
    };
    "noundefined package displays literal unknown control sequences and preserves following text"_test = [] {
        UndefinedFixture f;
        auto &root = f.compile(u"\\missing+x+\\?+\\未知");
        auto tokens = undefinedText(root);
        expect(tokens.size() == 3_u);
        const std::array<std::u16string_view, 3> expected{u"\\missing", u"\\?", u"\\未"};
        for (std::size_t i = 0; i < std::min(tokens.size(), expected.size()); ++i) {
            expect(NodeUtil::getText(*tokens[i]) == expected[i]);
            expect(attr(*tokens[i], u"mathcolor") == u"red");
            expect(tokens[i]->attributes().own(u"mathbackground") == nullptr);
            expect(tokens[i]->attributes().own(u"mathsize") == nullptr);
        }
        bool following{};
        root.walkTree([&](auto &node) {
            if (node.isToken() && NodeUtil::getText(node) == u"知")
                following = true;
        });
        expect(following);
        expect(f.error.empty());
        expect(!f.jax->parseOptions().request().error);
        expect(f.jax->parseOptions().request().parsers.empty());
        f.jax->reset(12);
        expect(NodeUtil::getText(*undefinedText(f.compile(u"\\again"))[0]) == u"\\again");
    };
    "noundefined package uses nonempty supplied styles and merges omitted defaults"_test = [] {
        const std::array<NoUndefinedOptions, 3> cases{{
            {u"", u"", u""}, {u"blue", u"yellow", u"125%"}, {{}, u"black", u"2em"}}};
        for (auto const &options : cases) {
            TexOptionValues values;
            values.noundefined = options;
            UndefinedFixture f(values);
            auto token = undefinedText(f.compile(u"\\missing"))[0];
            for (auto const &[attribute, expected] : {
                     std::pair{u"mathcolor", options.color.value_or(u"red")},
                     std::pair{u"mathbackground", options.background.value()},
                     std::pair{u"mathsize", options.size.value()}}) {
                const auto actual = attr(*token, attribute);
                const auto effective = expected.empty()
                    ? std::u16string(attribute == std::u16string_view(u"mathbackground")
                                         ? u"transparent"
                                         : attribute == std::u16string_view(u"mathsize") ? u"normal" : u"")
                    : expected;
                expect(std::ranges::equal(actual, effective))
                    << std::string(std::u16string_view(attribute).begin(),
                                   std::u16string_view(attribute).end())
                    << ": actual=" << std::string(actual.begin(), actual.end())
                    << " expected=" << std::string(effective.begin(), effective.end());
                if (expected.empty())
                    expect(token->attributes().own(attribute) == nullptr);
            }
            expect(f.error.empty());
        }
    };
    "noundefined package retains known math commands and other parser errors"_test = [] {
        UndefinedFixture f;
        auto &root = f.compile(u"\\frac{\\missing}{x}");
        bool fraction{};
        root.walkTree([&](auto &node) { fraction |= node.kind() == NodeKind::mfrac; });
        expect(fraction);
        expect(f.error.empty());
        f.compile(u"\\begin{unknown}x\\end{unknown}");
        expect(f.error == "UnknownEnv");
        f.compile(u"{\\missing");
        expect(f.error == "ExtraOpenMissingClose");
        expect(f.jax->parseOptions().request().parsers.empty());
        f.compile(u"\\after");
        expect(f.error.empty());
        UndefinedFixture base({}, false);
        base.compile(u"\\missing");
        expect(base.error == "UndefinedControlSequence");
    };
    "noundefined package calls configured text then node creators without token creator"_test = [] {
        UndefinedFixture f;
        f.jax->parseOptions().request().nodeFactory.setCreators(
            NodeCreators{.node = observeNode, .token = observeToken, .text = observeText});
        std::vector<std::string> events;
        struct Restore {
            std::vector<std::string> *old{creatorEvents};
            ~Restore() { creatorEvents = old; }
        } restore;
        creatorEvents = &events;
        f.compile(u"\\missing");
        expect(events == std::vector<std::string>{"text", "node"});
        expect(f.error.empty());
    };
};
