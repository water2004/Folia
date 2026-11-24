#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.enclose.configuration;
import folia.math.input.tex.cancel.configuration;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 3> encloseCallbacks{
    &EncloseMethods::Enclose, &CancelMethods::Cancel, &CancelMethods::CancelTo};
thread_local std::string *encloseError{};
thread_local std::u16string *encloseMessage{};
MmlNode &formatEncloseError(TeX &jax, TexError const &error) {
    if (encloseError)
        *encloseError = error.id();
    if (encloseMessage)
        *encloseMessage = error.message();
    return jax.formatError(error);
}
struct EncloseFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{encloseCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    std::u16string message;
    explicit EncloseFixture(bool combined = false, bool aliases = false) {
        EncloseConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"enclose", {}}};
        if (combined) {
            CancelConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"cancel", {}});
        }
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        options.formatError = &formatEncloseError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        message.clear();
        struct Restore {
            std::string *error{encloseError};
            std::u16string *message{encloseMessage};
            ~Restore() {
                encloseError = error;
                encloseMessage = message;
            }
        } restore;
        encloseError = &error;
        encloseMessage = &message;
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
std::vector<MmlNode *> encloseNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
thread_local std::vector<NodeDefinition> *encloseRequests{};
MmlNode &observeEnclose(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                        NodeDefinition definition, MmlNode *text) {
    if (encloseRequests && kind == NodeKind::menclose)
        encloseRequests->push_back(definition);
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
bool encloseBoolean(AttributeSet const &attrs, std::u16string_view key, bool expected) {
    auto value = attrs.own(key);
    return value && std::holds_alternative<bool>(*value) && std::get<bool>(*value) == expected;
}
} // namespace

suite enclose = [] {
    "enclose original map and seven untyped options remain independent of cancel"_test = [] {
        EncloseFixture f;
        auto macro = std::get<Macro const *>(f.maps.getMap(u"enclose")->lookup(u"enclose"));
        expect(std::get<MacroHandler>(macro->function) == &EncloseMethods::Enclose);
        constexpr std::array<std::u16string_view, 7> names{
            u"data-arrowhead", u"color",        u"mathcolor",     u"background",
            u"mathbackground", u"data-padding", u"data-thickness"};
        expect(ENCLOSE_OPTIONS.size() == names.size());
        for (std::size_t i = 0; i < names.size(); ++i) {
            expect(std::ranges::equal(ENCLOSE_OPTIONS[i].key, names[i]));
            expect(!ENCLOSE_OPTIONS[i].type.has_value());
        }
        auto nodes = encloseNodes(f.ok(u"\\enclose{box,,circle}{x}"), NodeKind::menclose);
        expect(nodes.size() == 1_u);
        if (!nodes.empty())
            expect(attr(*nodes[0], u"notation") == u"box  circle");
        f.compile(u"\\cancel{x}");
        expect(f.error == "UndefinedControlSequence");
    };
    "enclose configured creator receives filtered owned attributes and overwritten notation"_test =
        [] {
            EncloseFixture f;
            std::vector<NodeDefinition> requests;
            struct Restore {
                ~Restore() {
                    encloseRequests = nullptr;
                }
            } restore;
            encloseRequests = &requests;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.node = &observeEnclose});
            f.ok(u"\\enclose{box,updiagonalstrike}[data-arrowhead={foo\\,bar},color=red,mathcolor="
                 u"blue,"
                 u"background={a,b=c},mathbackground=yellow,data-padding={{1em}},data-thickness="
                 u"false,"
                 u"unknown=ignored,notation=wrong]{x}");
            expect(requests.size() == 1_u);
            if (!requests.empty()) {
                auto const &attrs = requests[0].attributes;
                for (auto const &[key, value] :
                     {std::pair<std::u16string_view, std::u16string_view>{u"data-arrowhead",
                                                                          u"foo\\,bar"},
                      {u"color", u"red"},
                      {u"mathcolor", u"blue"},
                      {u"background", u"a,b=c"},
                      {u"mathbackground", u"yellow"},
                      {u"data-padding", u"1em"},
                      {u"notation", u"box updiagonalstrike"}})
                    expect(attr(attrs, key) == value);
                expect(encloseBoolean(attrs, u"data-thickness", false));
                expect(attrs.own(u"unknown") == nullptr);
            }
            f.ok(u"\\enclose{circle}[data-arrowhead=true,data-padding={x\\}y}]{x}");
            expect(requests.size() == 2_u);
            if (requests.size() == 2) {
                expect(encloseBoolean(requests[1].attributes, u"data-arrowhead", true));
                expect(attr(requests[1].attributes, u"data-padding") == u"x\\}y");
            }
            // Later requests do not mutate the previously captured definition.
            if (!requests.empty())
                expect(attr(requests[0].attributes, u"notation") == u"box updiagonalstrike");
        };
    "enclose and cancel combine as nested original enclosure nodes"_test = [] {
        EncloseFixture f(true);
        auto &root = f.ok(u"\\enclose{box}{\\cancel{x}+\\cancelto{0}{y}}");
        expect(encloseNodes(root, NodeKind::menclose).size() == 3_u);
        expect(encloseNodes(root, NodeKind::msup).size() == 1_u);
    };
    "enclose argument scanning precedes attribute filtering and recovers after errors"_test = [] {
        struct Example {
            std::u16string_view source;
            std::string_view id;
        };
        for (auto const &example :
             {Example{u"\\enclose", "MissingArgFor"},
              {u"\\enclose{box}", "MissingArgFor"},
              {u"\\enclose{box}[color=x}]{\\unknown}", "ExtraCloseLooking"},
              {u"\\enclose{box}[color={x]{\\unknown}", "MissingCloseBracket"},
              {u"\\enclose{box}[unknown=x]{\\unknown}", "UndefinedControlSequence"}}) {
            EncloseFixture f;
            f.compile(std::u16string(example.source));
            expect(f.error == example.id);
            expect(encloseNodes(f.ok(u"\\enclose{box}{x}"), NodeKind::menclose).size() == 1_u);
        }
    };
    "enclose formal let aliases retain the captured compiled callback across formulas"_test = [] {
        EncloseFixture f(true, true);
        f.ok(u"\\let\\savedEnclose\\enclose\\let\\savedCancel\\cancel");
        auto &root = f.ok(u"\\savedEnclose{circle}{\\savedCancel{x}}");
        auto nodes = encloseNodes(root, NodeKind::menclose);
        expect(nodes.size() == 2_u);
        if (nodes.size() == 2) {
            expect(attr(*nodes[0], u"notation") == u"circle");
            expect(attr(*nodes[1], u"notation") == u"updiagonalstrike");
        }
    };
};
