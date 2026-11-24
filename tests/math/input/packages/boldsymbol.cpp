#include "support/folia_test.hpp"
import folia.math.input.tex.boldsymbol.configuration;

using namespace boost::ut;
using namespace folia::math::tex;
using namespace folia::math::mml;

namespace {
struct Host final : NodeFactoryConfiguration {
    TexEnvironment env;
    mutable int environmentReads{};
    bool active{true};
    std::vector<NodeKind> created;
    std::vector<MmlNode *> bold;
    TexEnvironment const &environment() const override {
        ++environmentReads;
        if (!active)
            throw std::logic_error("No active parser environment");
        return env;
    }
    void addNode(NodeKind kind, MmlNode &) override {
        created.push_back(kind);
    }
    void addNode(std::u16string_view property, MmlNode &node) override {
        if (property != u"fixBold")
            throw std::logic_error("Unexpected boldsymbol list");
        bold.push_back(&node);
    }
};
struct Warnings final : WarningSink {
    void warn(std::u16string_view) override {}
};
struct BoldFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{{}, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    BoldFixture() {
        BoldsymbolConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"boldsymbol", {}}};
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string text) {
        folia::math::core::AbstractMathItem math(std::move(text), jax.get());
        math.setDisplay(false);
        return jax->compile(math, document);
    }
};
std::vector<MmlNode *> tokens(MmlNode &root) {
    std::vector<MmlNode *> nodes;
    root.walkTree([&](auto &node) {
        if (node.isToken())
            nodes.push_back(&node);
    });
    return nodes;
}
std::u16string variant(MmlNode const &node) {
    auto value = node.attributes().lookup(u"mathvariant");
    return value ? std::get<std::u16string>(*value) : std::u16string{};
}
} // namespace

suite boldsymbol = [] {
    "boldsymbol configured token creator records the completed token in the extra list"_test = [] {
        MmlDocument document;
        Host context;
        context.env.bold_symbol = true;
        NodeFactory factory(document.factory());
        factory.setConfiguration(context);
        factory.setCreators(NodeCreators{.token = createBoldToken});
        auto &result = factory.makeToken(NodeKind::mi, u"x");
        expect(result.properties().tex.fix_bold.value());
        expect(context.created.size() == 1_u);
        expect(context.bold == std::vector<MmlNode *>{&result});
        expect(context.environmentReads == 1_i);
        context.env.bold_symbol = false;
        auto &plain = factory.makeToken(NodeKind::mi, u"y");
        expect(!plain.properties().tex.fix_bold.has_value());
        expect(context.bold.size() == 1_u);
        context.active = false;
        const auto reads = context.environmentReads;
        auto &text = factory.makeToken(NodeKind::mtext, u"text");
        expect(context.environmentReads == reads);
        expect(!text.properties().tex.fix_bold.has_value());
        const auto created = context.created.size();
        expect(throws<std::logic_error>([&] { factory.makeToken(NodeKind::mi, u"outside"); }));
        expect(context.created.size() == created + 1);
    };

    "boldsymbol rewrites the original variants and clears only true fixBold flags"_test = [] {
        static constexpr std::pair<std::u16string_view, std::u16string_view> cases[]{
            {u"normal", u"bold"},
            {u"italic", u"bold-italic"},
            {u"fraktur", u"bold-fraktur"},
            {u"script", u"bold-script"},
            {u"sans-serif", u"bold-sans-serif"},
            {u"-tex-calligraphic", u"-tex-bold-calligraphic"},
            {u"-tex-oldstyle", u"-tex-bold-oldstyle"},
            {u"-tex-mathit", u"bold-italic"},
            {u"double-struck", u"double-struck"},
            {u"custom", u"custom"}};
        MmlDocument document;
        NodeFactory factory(document.factory());
        for (auto const &[from, to] : cases) {
            NodeDefinition definition;
            definition.attributes.set(u"mathvariant", std::u16string(from));
            auto &node = factory.makeToken(NodeKind::mi, u"x", std::move(definition));
            node.properties().tex.fix_bold = true;
            MmlNode *nodes[]{&node};
            rewriteBoldTokens(nodes);
            expect(std::get<std::u16string>(*node.attributes().own(u"mathvariant")) == to);
            expect(!node.properties().tex.fix_bold.has_value());
            node.properties().tex.fix_bold = false;
            rewriteBoldTokens(nodes);
            expect(node.properties().tex.fix_bold.has_value());
            expect(!node.properties().tex.fix_bold.value());
        }
    };
    "boldsymbol actual TeX package rewrites after inheritance and restores outer scope"_test = [] {
        BoldFixture f;
        auto nodes = tokens(f.compile(u"\\boldsymbol{x+\\boldsymbol{y}}z"));
        expect(nodes.size() == 4_u);
        expect(NodeUtil::getText(*nodes[0]) == u"x");
        expect(variant(*nodes[0]) == u"bold-italic");
        expect(variant(*nodes[1]) == u"bold");
        expect(variant(*nodes[2]) == u"bold-italic");
        expect(variant(*nodes[3]) == u"italic");
        auto &options = f.jax->parseOptions();
        auto marked = options.getList(u"fixBold");
        expect(marked.size() == 3_u);
        expect(std::ranges::none_of(
            marked, [](auto node) { return node->properties().tex.fix_bold.has_value(); }));
        expect(options.request().parsers.empty());
        expect(f.registry.get(u"boldsymbol")->postprocessors.front().priority == 5.0);
    };
    "boldsymbol actual TeX package preserves unsupported variants and mtext"_test = [] {
        BoldFixture f;
        auto nodes = tokens(f.compile(u"\\boldsymbol{\\Gamma+\\alpha+1+\\mathbb{R}+\\mathrm{x}}z"));
        const std::array<std::u16string_view, 10> expected{
            u"bold", u"bold",          u"bold-italic", u"bold", u"bold",
            u"bold", u"double-struck", u"bold",        u"bold", u"italic"};
        expect(nodes.size() == expected.size());
        for (std::size_t i = 0; i < std::min(nodes.size(), expected.size()); ++i)
            expect(variant(*nodes[i]) == expected[i]);
        auto text = tokens(f.compile(u"\\boldsymbol{\\text{hello}}y"));
        expect(text.size() == 2_u);
        expect(text[0]->kind() == NodeKind::mtext);
        expect(variant(*text[0]) == u"normal");
        expect(variant(*text[1]) == u"italic");
        expect(f.jax->parseOptions().getList(u"fixBold").empty());
    };
};
