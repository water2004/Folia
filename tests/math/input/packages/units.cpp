#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.units.configuration;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 3> unitCallbacks{&UnitsMethods::Unit, &UnitsMethods::UnitFrac,
                                                    &UnitsMethods::NiceFrac};
thread_local std::string *unitsError{};
MmlNode &formatUnitsError(TeX &jax, TexError const &error) {
    if (unitsError)
        *unitsError = error.id();
    return jax.formatError(error);
}
struct UnitsFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{unitCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    explicit UnitsFixture(bool loose = false, bool ugly = false, bool aliases = false) {
        UnitsConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"units", {}}};
        options.units = UnitsOptions{.loose = loose, .ugly = ugly};
        options.formatError = &formatUnitsError;
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        struct Restore {
            std::string *old{unitsError};
            ~Restore() {
                unitsError = old;
            }
        } restore;
        unitsError = &error;
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
std::vector<MmlNode *> unitsNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
std::u16string unitsText(MmlNode &root) {
    std::u16string result;
    root.walkTree([&](auto &node) {
        if (node.isToken())
            result += NodeUtil::getText(node);
    });
    return result;
}
bool unitsBoolean(AttributeSet const &attributes, std::u16string_view key, bool value) {
    auto own = attributes.own(key);
    return own && std::holds_alternative<bool>(*own) && std::get<bool>(*own) == value;
}
struct UnitRequest {
    std::vector<MmlNode *> children;
    NodeDefinition definition;
};
thread_local std::vector<UnitRequest> *unitRequests{};
MmlNode &observeUnits(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                      NodeDefinition definition, MmlNode *text) {
    if (unitRequests && kind == NodeKind::mfrac)
        unitRequests->push_back({{children.begin(), children.end()}, definition});
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
} // namespace

suite units = [] {
    "units original three compiled methods and upright argument expansion"_test = [] {
        UnitsFixture f;
        const std::pair<std::u16string_view, MacroHandler> expected[]{
            {u"units", &UnitsMethods::Unit},
            {u"unitfrac", &UnitsMethods::UnitFrac},
            {u"nicefrac", &UnitsMethods::NiceFrac}};
        for (auto const &[name, callback] : expected) {
            auto macro = std::get<Macro const *>(f.maps.getMap(u"units")->lookup(name));
            expect(std::get<MacroHandler>(macro->function) == callback);
        }
        const auto defaults = f.registry.get(u"units")->options.units;
        expect(defaults && defaults->loose == false && defaults->ugly == false);
        for (auto source : {u"\\units{kg}", u"\\units[]{kg}", u"\\units k"}) {
            auto &root = f.ok(source);
            auto tokens = unitsNodes(root, NodeKind::mi);
            expect(tokens.size() == 1_u);
            expect(unitsNodes(root, NodeKind::mspace).empty());
            for (auto token : tokens)
                expect(attr(*token, u"mathvariant") == u"normal");
            expect(unitsText(root) == (std::u16string_view(source) == u"\\units k" ? u"k" : u"kg"));
        }
    };
    "units value prefixes preserve zero string spacing options and remaining input"_test = [] {
        for (bool loose : {false, true}) {
            UnitsFixture f(loose);
            for (auto source :
                 {u"\\units[2]{kg}+z", u"\\units[0]{kg}+z", u"\\unitfrac[2]{m}{s}+z"}) {
                auto &root = f.ok(source);
                auto spaces = unitsNodes(root, NodeKind::mspace);
                expect(spaces.size() == (loose ? std::size_t{0} : std::size_t{1}));
                if (loose) {
                    auto texts = unitsNodes(root, NodeKind::mtext);
                    expect(texts.size() == 1_u);
                    if (!texts.empty())
                        expect(NodeUtil::getText(*texts[0]) == u"\u00A0");
                } else if (!spaces.empty())
                    expect(attr(*spaces[0], u"width") == u"0.167em");
                expect(unitsText(root).ends_with(u"+z"));
            }
        }
    };
    "units fractions retain explicit bevel option and existing numerator denominator nodes"_test =
        [] {
            for (bool ugly : {false, true}) {
                UnitsFixture f(false, ugly);
                for (auto source : {u"\\unitfrac{m}{s}", u"\\nicefrac{m}{s}", u"\\nicefrac{}{s}"}) {
                    auto fractions = unitsNodes(f.ok(source), NodeKind::mfrac);
                    expect(fractions.size() == 1_u);
                    if (!fractions.empty()) {
                        if (ugly)
                            expect(fractions[0]->attributes().own(u"bevelled") == nullptr);
                        else
                            expect(unitsBoolean(fractions[0]->attributes(), u"bevelled", true));
                    }
                }
            }
        };
    "units local fraction parsers inherit current environment and keep custom font changes local"_test =
        [] {
            UnitsFixture f;
            auto tokens = unitsNodes(f.ok(u"\\mathbf{\\nicefrac[\\mathit]{a}{b}c}"), NodeKind::mi);
            expect(tokens.size() == 3_u);
            if (tokens.size() == 3) {
                expect(attr(*tokens[0], u"mathvariant") == u"-tex-mathit");
                expect(attr(*tokens[1], u"mathvariant") == u"-tex-mathit");
                expect(attr(*tokens[2], u"mathvariant") == u"bold");
            }
            tokens = unitsNodes(f.ok(u"\\mathbf{\\nicefrac[]{a}{b}}"), NodeKind::mi);
            expect(tokens.size() == 2_u);
            for (auto token : tokens)
                expect(attr(*token, u"mathvariant") == u"bold");
            expect(unitsNodes(f.ok(u"\\nicefrac{\\nicefrac{a}{b}}{c}"), NodeKind::mfrac).size() ==
                   2_u);
            std::vector<UnitRequest> requests;
            struct Restore {
                ~Restore() {
                    unitRequests = nullptr;
                }
            } restore;
            unitRequests = &requests;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.node = &observeUnits});
            auto fractions = unitsNodes(f.ok(u"\\nicefrac{a}{b}"), NodeKind::mfrac);
            expect(requests.size() == 1_u && fractions.size() == 1_u);
            if (requests.size() == 1 && fractions.size() == 1) {
                expect(unitsBoolean(requests[0].definition.attributes, u"bevelled", true));
                expect(requests[0].children.size() == 2_u &&
                       fractions[0]->children().size() == 2_u);
                if (requests[0].children.size() == 2 && fractions[0]->children().size() == 2)
                    for (std::size_t i = 0; i < 2; ++i)
                        expect(requests[0].children[i] == fractions[0]->children()[i]);
            }
        };
    "units argument and child errors release local parsers and allow following formulas"_test = [] {
        UnitsFixture f;
        for (auto const &[source, id] :
             {std::pair<std::u16string_view, std::string_view>{u"\\units", "MissingArgFor"},
              {u"\\unitfrac{x}", "MissingArgFor"},
              {u"\\nicefrac{x}", "MissingArgFor"},
              {u"\\nicefrac{\\unknown}{b}", "UndefinedControlSequence"},
              {u"\\nicefrac{a}{\\unknown}", "UndefinedControlSequence"},
              {u"\\units{\\unknown}", "UndefinedControlSequence"}}) {
            f.compile(std::u16string(source));
            expect(f.error == id);
            f.ok(u"\\units{kg}");
        }
    };
    "units formal aliases retain three fixed methods across expressions"_test = [] {
        UnitsFixture f(false, false, true);
        f.ok(u"\\let\\savedUnit\\units\\let\\savedFrac\\unitfrac\\let\\savedNice\\nicefrac");
        expect(unitsText(f.ok(u"\\savedUnit{kg}")) == u"kg");
        for (auto source : {u"\\savedFrac{m}{s}", u"\\savedNice{m}{s}"})
            expect(unitsNodes(f.ok(source), NodeKind::mfrac).size() == 1_u);
    };
};
