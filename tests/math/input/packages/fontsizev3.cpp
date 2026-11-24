#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.fontsizev3.configuration;
import folia.math.input.tex.base_methods;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 1> sizeCallbacks{&BaseMethods::SetSize};
struct SizeFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{sizeCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    explicit SizeFixture(bool selected = true, bool aliases = false, bool reversed = false) {
        FontSizeV3Configuration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (selected) {
            if (reversed)
                options.packages->insert(options.packages->begin(), {u"fontsizev3", {}});
            else
                options.packages->push_back({u"fontsizev3", {}});
        }
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        auto &root = jax->compile(math, document);
        expect(!jax->parseOptions().request().error);
        expect(jax->parseOptions().request().parsers.empty());
        return root;
    }
};
std::vector<MmlNode *> sizeNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
std::u16string inheritedSize(MmlNode const &node) {
    auto current = &node;
    while (current) {
        auto value = attr(current->attributes(), u"mathsize");
        if (!value.empty())
            return value;
        current = current->parent();
    }
    return u"1em";
}
thread_local std::vector<std::u16string> *sizeRequests{};
MmlNode &observeSize(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                     NodeDefinition definition, MmlNode *text) {
    if (sizeRequests && kind == NodeKind::mstyle)
        sizeRequests->push_back(attr(definition.attributes, u"mathsize"));
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
} // namespace
suite fontsizev3 = [] {
    "fontsizev3 ten original map entries reuse Base SetSize including original tiny and Tiny ratios"_test =
        [] {
            SizeFixture f;
            struct Case {
                std::u16string_view name, text;
                double scale;
            };
            const Case cases[]{{u"tiny", u"0.5em", .5},       {u"Tiny", u"0.6em", .6},
                               {u"scriptsize", u"0.7em", .7}, {u"small", u"0.85em", .85},
                               {u"normalsize", u"1em", 1},    {u"large", u"1.2em", 1.2},
                               {u"Large", u"1.44em", 1.44},   {u"LARGE", u"1.73em", 1.73},
                               {u"huge", u"2.07em", 2.07},    {u"Huge", u"2.49em", 2.49}};
            for (auto const &item : cases) {
                auto macro =
                    std::get<Macro const *>(f.maps.getMap(u"fontsizev3-macros")->lookup(item.name));
                expect(std::get<MacroHandler>(macro->function) == &BaseMethods::SetSize);
                expect(std::get<double>(macro->arguments[0]) == item.scale);
                auto nodes = sizeNodes(f.compile(u"\\" + std::u16string(item.name) + u" x"),
                                       NodeKind::mstyle);
                expect(nodes.size() == 1_u);
                if (!nodes.empty())
                    expect(std::ranges::equal(attr(*nodes[0], u"mathsize"), item.text));
            }
        };
    "fontsizev3 package selection and source package ordering preserve map precedence"_test = [] {
        SizeFixture absent(false), selected, baseLast(true, false, true);
        for (auto const &[fixture, value] :
             {std::pair<SizeFixture *, std::u16string_view>{&absent, u"0.6em"},
              {&selected, u"0.5em"},
              {&baseLast, u"0.6em"}}) {
            auto nodes = sizeNodes(fixture->compile(u"\\tiny x"), NodeKind::mstyle);
            expect(nodes.size() == 1_u);
            if (!nodes.empty())
                expect(std::ranges::equal(attr(*nodes[0], u"mathsize"), value));
        }
    };
    "fontsizev3 scoped and consecutive sizes use original environment and configured node creator"_test =
        [] {
            SizeFixture f;
            auto tokens = sizeNodes(f.compile(u"\\small a{\\Huge b}c"), NodeKind::mi);
            expect(tokens.size() == 3_u);
            if (tokens.size() == 3) {
                expect(inheritedSize(*tokens[0]) == u"0.85em");
                expect(inheritedSize(*tokens[1]) == u"2.49em");
                expect(inheritedSize(*tokens[2]) == u"0.85em");
            }
            tokens = sizeNodes(f.compile(u"{\\Huge x}y"), NodeKind::mi);
            expect(tokens.size() == 2_u);
            if (tokens.size() == 2) {
                expect(inheritedSize(*tokens[0]) == u"2.49em");
                expect(inheritedSize(*tokens[1]) == u"1em");
            }
            std::vector<std::u16string> requests;
            struct Restore {
                ~Restore() {
                    sizeRequests = nullptr;
                }
            } restore;
            sizeRequests = &requests;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.node = &observeSize});
            f.compile(u"\\tiny\\large x");
            expect(requests.size() == 2_u);
            if (requests.size() == 2) {
                expect(requests[0] == u"1.2em");
                expect(requests[1] == u"0.5em");
            }
        };
    "fontsizev3 formal let captures the original SetSize arguments across formulas"_test = [] {
        SizeFixture f(true, true);
        f.compile(u"\\let\\saved\\Huge");
        auto nodes = sizeNodes(f.compile(u"\\saved x"), NodeKind::mstyle);
        expect(nodes.size() == 1_u);
        if (!nodes.empty())
            expect(attr(*nodes[0], u"mathsize") == u"2.49em");
    };
};
