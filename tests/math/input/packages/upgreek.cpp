#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.upgreek.configuration;
import folia.math.input.tex.newcommand.configuration;
import folia.math.input.tex.boldsymbol.configuration;

namespace {
constexpr std::pair<std::u16string_view, std::u16string_view> greekCharacters[]{
    {u"upalpha", u"\u03B1"},      {u"upbeta", u"\u03B2"},     {u"upgamma", u"\u03B3"},
    {u"updelta", u"\u03B4"},      {u"upepsilon", u"\u03F5"},  {u"upzeta", u"\u03B6"},
    {u"upeta", u"\u03B7"},        {u"uptheta", u"\u03B8"},    {u"upiota", u"\u03B9"},
    {u"upkappa", u"\u03BA"},      {u"uplambda", u"\u03BB"},   {u"upmu", u"\u03BC"},
    {u"upnu", u"\u03BD"},         {u"upxi", u"\u03BE"},       {u"upomicron", u"\u03BF"},
    {u"uppi", u"\u03C0"},         {u"uprho", u"\u03C1"},      {u"upsigma", u"\u03C3"},
    {u"uptau", u"\u03C4"},        {u"upupsilon", u"\u03C5"},  {u"upphi", u"\u03D5"},
    {u"upchi", u"\u03C7"},        {u"uppsi", u"\u03C8"},      {u"upomega", u"\u03C9"},
    {u"upvarepsilon", u"\u03B5"}, {u"upvartheta", u"\u03D1"}, {u"upvarpi", u"\u03D6"},
    {u"upvarrho", u"\u03F1"},     {u"upvarsigma", u"\u03C2"}, {u"upvarphi", u"\u03C6"},
    {u"Upgamma", u"\u0393"},      {u"Updelta", u"\u0394"},    {u"Uptheta", u"\u0398"},
    {u"Uplambda", u"\u039B"},     {u"Upxi", u"\u039E"},       {u"Uppi", u"\u03A0"},
    {u"Upsigma", u"\u03A3"},      {u"Upupsilon", u"\u03A5"},  {u"Upphi", u"\u03A6"},
    {u"Uppsi", u"\u03A8"},        {u"Upomega", u"\u03A9"}};
constexpr std::array<TokenInputHandler, 1> greekCallbacks{&mathchar0miNormal};
struct GreekRequest {
    NodeKind kind;
    std::optional<std::u16string> text;
    NodeDefinition definition;
};
thread_local std::vector<GreekRequest> *greekRequests{};
MmlNode &observeGreek(NodeFactory &factory, NodeKind kind, NodeDefinition definition,
                      std::optional<std::u16string_view> text) {
    if (greekRequests)
        greekRequests->push_back(
            {kind, text ? std::optional<std::u16string>(*text) : std::nullopt, definition});
    return NodeFactory::createToken(factory, kind, std::move(definition), text);
}
struct GreekFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, greekCallbacks};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    explicit GreekFixture(bool extensions = false, bool selected = true) {
        UpgreekConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}};
        if (selected)
            options.packages->push_back({u"upgreek", {}});
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
suite upgreek = [] {
    "upgreek all forty one original symbols remain normal in surrounding fonts"_test = [] {
        GreekFixture f;
        expect(std::size(greekCharacters) == 41_u);
        for (auto const &[name, character] : greekCharacters) {
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
                expect(!f.jax->parseOptions().request().error);
            }
        }
    };
    "upgreek copies token attributes before configured creator and preserves null versus empty"_test =
        [] {
            GreekFixture f;
            auto map = std::dynamic_pointer_cast<CharacterMap>(f.maps.getMap(u"upgreek"));
            AttributeSet attributes;
            attributes.set(u"mathvariant", std::u16string(u"italic"));
            attributes.set(u"mathcolor", std::u16string(u"red"));
            attributes.set(u"class", std::u16string(u"kept"));
            map->add(Token(u"greekText", std::u16string(u"α"), attributes));
            map->add(Token(u"greekNull", std::nullopt, attributes));
            map->add(Token(u"greekEmpty", std::u16string{}, attributes));
            std::vector<GreekRequest> requests;
            greekRequests = &requests;
            struct Restore {
                ~Restore() {
                    greekRequests = nullptr;
                }
            } restore;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.token = &observeGreek});
            auto nodes = f.tokens(u"\\greekText\\greekNull\\greekEmpty");
            expect(requests.size() == 3_u && nodes.size() == 3_u);
            for (auto const &request : requests) {
                expect(request.kind == NodeKind::mi);
                expect(attr(request.definition.attributes, u"mathvariant") == u"normal");
                expect(attr(request.definition.attributes, u"mathcolor") == u"red");
                expect(attr(request.definition.attributes, u"class") == u"kept");
            }
            if (requests.size() == 3 && nodes.size() == 3) {
                expect(requests[0].text.has_value() && requests[0].text.value() == u"α");
                expect(!requests[1].text.has_value());
                expect(requests[2].text.has_value() && requests[2].text->empty());
                expect(nodes[0]->children().size() == 1_u);
                expect(nodes[1]->children().empty());
                expect(nodes[2]->children().size() == 1_u);
            }
        };
    "upgreek newcommand let retains captured Greek token and boldsymbol rewrites actual output"_test =
        [] {
            GreekFixture f(true);
            auto nodes = f.tokens(u"\\let\\saved\\upalpha\\let\\upalpha\\upbeta\\saved\\upalpha");
            expect(nodes.size() == 2_u);
            if (nodes.size() == 2) {
                expect(NodeUtil::getText(*nodes[0]) == u"α");
                expect(NodeUtil::getText(*nodes[1]) == u"β");
                expect(attr(*nodes[0], u"mathvariant") == u"normal");
                expect(attr(*nodes[1], u"mathvariant") == u"normal");
            }
            nodes = f.tokens(u"\\boldsymbol{\\saved+\\Upomega}\\upbeta");
            expect(nodes.size() == 4_u);
            if (nodes.size() == 4) {
                for (std::size_t i = 0; i < 3; ++i)
                    expect(attr(*nodes[i], u"mathvariant") == u"bold");
                expect(attr(*nodes[3], u"mathvariant") == u"normal");
            }
        };
    "upgreek package selection and registries remain independent"_test = [] {
        GreekFixture first, second, baseOnly(false, false);
        auto firstMap = std::dynamic_pointer_cast<CharacterMap>(first.maps.getMap(u"upgreek"));
        firstMap->add(Token(u"upalpha", std::u16string(u"x")));
        auto a = first.tokens(u"\\upalpha"), b = second.tokens(u"\\upalpha");
        expect(a.size() == 1_u && b.size() == 1_u);
        if (a.size() == 1 && b.size() == 1) {
            expect(NodeUtil::getText(*a.front()) == u"x");
            expect(NodeUtil::getText(*b.front()) == u"α");
        }
        baseOnly.tokens(u"\\upalpha");
        expect(baseOnly.jax->parseOptions().request().error);
        expect(!second.jax->parseOptions().request().error);
    };
};
