#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.gensymb.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.boldsymbol.configuration;

namespace {
constexpr std::pair<std::u16string_view, std::u16string_view> units[]{{u"ohm", u"\u2126"},
                                                                      {u"degree", u"\u00B0"},
                                                                      {u"celsius", u"\u2103"},
                                                                      {u"perthousand", u"\u2030"},
                                                                      {u"micro", u"\u00B5"}};
constexpr std::array<TokenInputHandler, 1> unitCallbacks{&mathcharUnit};
struct UnitRequest {
    NodeKind kind;
    std::optional<std::u16string> text;
    NodeDefinition definition;
};
thread_local std::vector<UnitRequest> *unitRequests{};
MmlNode &observeUnit(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                     std::optional<std::u16string_view> text) {
    if (unitRequests)
        unitRequests->push_back(
            {kind, text ? std::optional<std::u16string>(*text) : std::nullopt, definition});
    return NodeFactory::createToken(factory, kind, std::move(definition), text);
}
struct UnitFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, unitCallbacks};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    explicit UnitFixture(bool extensions = false, bool selected = true) {
        GensymbConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (selected)
            options.packages->push_back({u"gensymb", {}});
        if (extensions) {
            NewcommandConfiguration::initializeModule(registry, maps);
            BoldsymbolConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
            options.packages->push_back({u"boldsymbol", {}});
        }
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    std::vector<MmlNode *> tokens(std::u16string source) {
        folia::math::core::AbstractMathItem math(std::move(source), jax.get());
        math.setDisplay(false);
        auto &root = jax->compile(math, document);
        std::vector<MmlNode *> result;
        root.walkTree([&](auto &node) {
            if (node.isToken())
                result.push_back(&node);
        });
        return result;
    }
};
} // namespace
suite gensymb = [] {
    "gensymb all five original units retain glyph normal variant and unit class in fonts"_test =
        [] {
            UnitFixture f;
            for (auto const &[name, character] : units) {
                for (auto const &source :
                     {u"\\" + std::u16string(name), u"\\mathbf{\\" + std::u16string(name) + u"}",
                      u"\\mathit{\\" + std::u16string(name) + u"}"}) {
                    auto nodes = f.tokens(source);
                    expect(nodes.size() == 1_u);
                    if (nodes.size() != 1)
                        continue;
                    expect(nodes.front()->kind() == NodeKind::mi);
                    expect(NodeUtil::getText(*nodes.front()) == character);
                    expect(attr(*nodes.front(), u"mathvariant") == u"normal");
                    expect(attr(*nodes.front(), u"class") == u"MathML-Unit");
                    expect(!f.jax->parseOptions().request().error);
                }
            }
        };
    "gensymb overrides normal and unit class before configured creator retaining other attributes"_test =
        [] {
            UnitFixture f;
            auto map = std::dynamic_pointer_cast<CharacterMap>(f.maps.getMap(u"gensymb-symbols"));
            AttributeSet attributes;
            attributes.set(u"mathvariant", std::u16string(u"italic"));
            attributes.set(u"mathcolor", std::u16string(u"red"));
            attributes.set(u"class", std::u16string(u"replaced"));
            map->add(Token(u"unitText", std::u16string(u"Ω"), attributes));
            map->add(Token(u"unitNull", std::nullopt, attributes));
            map->add(Token(u"unitEmpty", std::u16string{}, attributes));
            std::vector<UnitRequest> requests;
            unitRequests = &requests;
            struct Restore {
                ~Restore() {
                    unitRequests = nullptr;
                }
            } restore;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.token = &observeUnit});
            auto nodes = f.tokens(u"\\unitText\\unitNull\\unitEmpty");
            expect(requests.size() == 3_u && nodes.size() == 3_u);
            for (auto const &request : requests) {
                expect(request.kind == NodeKind::mi);
                expect(attr(request.definition.attributes, u"mathvariant") == u"normal");
                expect(attr(request.definition.attributes, u"mathcolor") == u"red");
                expect(attr(request.definition.attributes, u"class") == u"MathML-Unit");
            }
            if (requests.size() == 3 && nodes.size() == 3) {
                expect(requests[0].text.has_value() && requests[0].text.value() == u"Ω");
                expect(!requests[1].text.has_value());
                expect(requests[2].text.has_value() && requests[2].text->empty());
                expect(nodes[0]->children().size() == 1_u);
                expect(nodes[1]->children().empty());
                expect(nodes[2]->children().size() == 1_u);
            }
        };
    "gensymb let captures original unit glyph attributes and boldsymbol runs actual postprocessor"_test =
        [] {
            UnitFixture f(true);
            auto nodes = f.tokens(u"\\let\\saved\\ohm\\let\\ohm\\micro\\saved\\ohm");
            expect(nodes.size() == 2_u);
            if (nodes.size() == 2) {
                expect(NodeUtil::getText(*nodes[0]) == u"Ω");
                expect(NodeUtil::getText(*nodes[1]) == u"µ");
                expect(attr(*nodes[0], u"class") == u"MathML-Unit");
                expect(attr(*nodes[1], u"class") == u"MathML-Unit");
            }
            nodes = f.tokens(u"\\boldsymbol{\\saved+\\micro}\\degree");
            expect(nodes.size() == 4_u);
            if (nodes.size() == 4) {
                for (std::size_t i = 0; i < 3; ++i)
                    expect(attr(*nodes[i], u"mathvariant") == u"bold");
                expect(attr(*nodes[3], u"mathvariant") == u"normal");
                for (auto i : {0, 2, 3})
                    expect(attr(*nodes[i], u"class") == u"MathML-Unit");
            }
        };
    "gensymb package selection and registry definitions stay independent"_test = [] {
        UnitFixture first, second, baseOnly(false, false);
        auto map = std::dynamic_pointer_cast<CharacterMap>(first.maps.getMap(u"gensymb-symbols"));
        map->add(Token(u"ohm", std::u16string(u"x")));
        auto a = first.tokens(u"\\ohm"), b = second.tokens(u"\\ohm");
        expect(a.size() == 1_u && b.size() == 1_u);
        if (a.size() == 1 && b.size() == 1) {
            expect(NodeUtil::getText(*a.front()) == u"x");
            expect(NodeUtil::getText(*b.front()) == u"Ω");
        }
        baseOnly.tokens(u"\\ohm");
        expect(baseOnly.jax->parseOptions().request().error);
        expect(!second.jax->parseOptions().request().error);
    };
};
