#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.centernot.configuration;
import folia.math.input.tex.base_methods;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 2> centerCallbacks{&CenterOver, &BaseMethods::Macro};
thread_local std::string *centerError{};
MmlNode &formatCenterError(TeX &jax, TexError const &error) {
    if (centerError)
        *centerError = error.id();
    return jax.formatError(error);
}
struct CenterFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{centerCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit CenterFixture(bool aliases = false) {
        CenternotConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"centernot", {}}};
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        options.formatError = &formatCenterError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{centerError};
            ~Restore() {
                centerError = old;
            }
        } restore;
        centerError = &error;
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
std::vector<MmlNode *> centerNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
MmlNode *centerOuter(MmlNode &base) {
    auto node = &base;
    for (int i = 0; i < 6 && node; ++i)
        node = node->parent();
    return node;
}
bool zeroWidth(MmlNode &node) {
    auto value = node.attributes().lookup(u"width");
    return value && std::holds_alternative<double>(*value) && std::get<double>(*value) == 0;
}
thread_local std::vector<std::u16string> *centerCreatedTokens{};
MmlNode &observeCenterToken(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                            std::optional<std::u16string_view> text) {
    if (centerCreatedTokens && text)
        centerCreatedTokens->emplace_back(*text);
    return NodeFactory::createToken(factory, kind, std::move(definition), text);
}
} // namespace
suite centernot = [] {
    "centernot original callbacks and postprocessor copy relation binary operator and ordinary classes"_test =
        [] {
            CenterFixture f;
            auto macro =
                std::get<Macro const *>(f.maps.getMap(u"centernot")->lookup(u"centerOver"));
            expect(std::get<MacroHandler>(macro->function) == &CenterOver);
            auto config = f.registry.get(u"centernot");
            expect(config->postprocessors.size() == 1_u);
            struct Case {
                std::u16string_view source;
                TexClass texClass;
            };
            for (auto const &item : {Case{u"=", TexClass::rel},
                                     {u"+", TexClass::bin},
                                     {u"x", TexClass::ord},
                                     {u"\\sum", TexClass::op},
                                     {u"\\leq", TexClass::rel}}) {
                auto &root = f.ok(u"\\centernot{" + std::u16string(item.source) + u"}");
                auto bases = f.jax->parseOptions().getList(u"centerOver");
                expect(bases.size() == 1_u);
                if (bases.empty())
                    continue;
                auto outer = centerOuter(*bases[0]);
                expect(outer != nullptr);
                if (outer) {
                    expect(outer->kind() == NodeKind::tex_atom);
                    expect(outer->texClass() == item.texClass);
                    expect(outer->properties().tex.explicit_tex_class == item.texClass);
                }
                auto pads = centerNodes(root, NodeKind::mpadded);
                expect(pads.size() == 2_u);
                for (auto pad : pads) {
                    expect(zeroWidth(*pad));
                    expect(attr(*pad, u"lspace") == u"-.5width");
                }
                expect(centerNodes(root, NodeKind::mphantom).size() == 1_u);
            }
        };
    "centernot visible and phantom branches keep original parse order and environment"_test = [] {
        CenterFixture f;
        std::vector<std::u16string> requests;
        struct Restore {
            ~Restore() {
                centerCreatedTokens = nullptr;
            }
        } restore;
        centerCreatedTokens = &requests;
        f.jax->parseOptions().request().nodeFactory.setCreators(
            NodeCreators{.token = &observeCenterToken});
        auto &root = f.ok(u"\\centerOver{xy}{z}");
        const std::array<std::u16string_view, 5> expected{u"z", u"x", u"y", u"x", u"y"};
        expect(requests.size() == expected.size());
        for (std::size_t i = 0; i < std::min(requests.size(), expected.size()); ++i)
            expect(std::ranges::equal(requests[i], expected[i]));
        auto phantom = centerNodes(root, NodeKind::mphantom);
        expect(phantom.size() == 1_u);
        if (!phantom.empty())
            expect(centerNodes(*phantom[0], NodeKind::mi).size() == 2_u);
        auto &bold = f.ok(u"\\mathbf{\\centernot{x}}");
        auto tokens = centerNodes(bold, NodeKind::mi);
        expect(tokens.size() == 2_u);
        for (auto token : tokens)
            expect(attr(*token, u"mathvariant") == u"bold");
    };
    "centernot nested overlays retain three class-copy list entries and reset them next formula"_test =
        [] {
            CenterFixture f;
            auto &root = f.ok(u"\\centernot{\\centernot{=}}");
            auto bases = f.jax->parseOptions().getList(u"centerOver");
            expect(bases.size() == 3_u);
            for (auto base : bases) {
                auto outer = centerOuter(*base);
                expect(outer && outer->texClass() == TexClass::rel);
            }
            expect(centerNodes(root, NodeKind::mphantom).size() == 3_u);
            f.ok(u"x");
            expect(f.jax->parseOptions().getList(u"centerOver").empty());
        };
    "centernot let aliases missing arguments and empty base exception leave following input usable"_test =
        [] {
            CenterFixture f(true);
            f.ok(u"\\let\\saved\\centerOver");
            f.ok(u"\\saved{=}{x}");
            auto bases = f.jax->parseOptions().getList(u"centerOver");
            expect(bases.size() == 1_u);
            if (!bases.empty()) {
                auto outer = centerOuter(*bases[0]);
                expect(outer && outer->texClass() == TexClass::rel);
            }
            for (auto const &[source, id] :
                 {std::pair<std::u16string_view, std::string_view>{u"\\centernot", "MissingArgFor"},
                  {u"\\centerOver{x}", "MissingArgFor"},
                  {u"\\centerOver{x}{\\unknown}", "UndefinedControlSequence"}}) {
                f.compile(std::u16string(source));
                expect(f.error == id);
                f.ok(u"\\centernot{=}");
            }
            expect(throws<std::out_of_range>([&] { f.compile(u"\\centernot{}"); }));
            expect(f.jax->parseOptions().request().parsers.empty());
            f.ok(u"\\centernot{=}");
        };
};
