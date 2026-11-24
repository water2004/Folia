#include "math/support/tex_fixture.hpp"
import folia.math.input.tex.cancel.configuration;
import folia.math.input.tex.newcommand.configuration;

namespace {
constexpr std::array<MacroHandler, 2> cancelCallbacks{&CancelMethods::Cancel,
                                                      &CancelMethods::CancelTo};
thread_local std::string *cancelError{};
thread_local std::u16string *cancelMessage{};
MmlNode &formatCancelError(TeX &jax, TexError const &error) {
    if (cancelError)
        *cancelError = error.id();
    if (cancelMessage)
        *cancelMessage = error.message();
    return jax.formatError(error);
}
struct CancelFixture {
    ConfigurationHandler registry;
    MapHandler maps;
    MacroCallbacks callbacks{cancelCallbacks, {}};
    Warnings warnings;
    MmlDocument document;
    std::unique_ptr<TeX> jax;
    std::unique_ptr<TeXRequest> request;
    std::string error;
    std::u16string message;
    explicit CancelFixture(bool aliases = false) {
        CancelConfiguration::initializeModule(registry, maps);
        TexOptionValues options;
        options.packages = std::vector<PackageSelection>{{u"base", {}}, {u"cancel", {}}};
        if (aliases) {
            NewcommandConfiguration::initializeModule(registry, maps);
            options.packages->push_back({u"newcommand", {}});
        }
        options.formatError = &formatCancelError;
        jax = std::make_unique<TeX>(registry, maps, callbacks, warnings, options);
        request = std::make_unique<TeXRequest>(*jax, document.factory());
    }
    MmlNode &compile(std::u16string source) {
        error.clear();
        message.clear();
        struct Restore {
            std::string *error{cancelError};
            std::u16string *message{cancelMessage};
            ~Restore() {
                cancelError = error;
                cancelMessage = message;
            }
        } restore;
        cancelError = &error;
        cancelMessage = &message;
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
std::vector<MmlNode *> cancelNodes(MmlNode &root, NodeKind kind) {
    std::vector<MmlNode *> result;
    root.walkTree([&](auto &node) {
        if (node.kind() == kind)
            result.push_back(&node);
    });
    return result;
}
struct CancelRequest {
    NodeKind kind;
    NodeDefinition definition;
};
thread_local std::vector<CancelRequest> *cancelRequests{};
MmlNode &observeCancel(NodeFactory &factory, NodeKind kind, std::span<MmlNode *const> children,
                       NodeDefinition definition, MmlNode *text) {
    if (cancelRequests &&
        (kind == NodeKind::mpadded || kind == NodeKind::menclose || kind == NodeKind::msup))
        cancelRequests->push_back({kind, definition});
    return NodeFactory::createNode(factory, kind, children, std::move(definition), text);
}
} // namespace

suite cancel = [] {
    "cancel original four commands select independently while initializing the enclose dependency"_test =
        [] {
            CancelFixture f;
            expect(f.registry.get(u"enclose") != nullptr);
            expect(f.maps.getMap(u"enclose") != nullptr);
            struct Entry {
                std::u16string_view name, notation;
            };
            for (auto const &entry : {Entry{u"cancel", u"updiagonalstrike"},
                                      {u"bcancel", u"downdiagonalstrike"},
                                      {u"xcancel", u"updiagonalstrike downdiagonalstrike"}}) {
                auto macro = std::get<Macro const *>(f.maps.getMap(u"cancel")->lookup(entry.name));
                expect(std::get<MacroHandler>(macro->function) == &CancelMethods::Cancel);
                expect(std::ranges::equal(std::get<std::u16string>(macro->arguments[0]),
                                          entry.notation));
                auto nodes = cancelNodes(f.ok(u"\\" + std::u16string(entry.name) +
                                              u"[mathcolor=red,notation=wrong,unknown=x]{x}"),
                                         NodeKind::menclose);
                expect(nodes.size() == 1_u);
                if (!nodes.empty()) {
                    expect(attr(*nodes[0], u"notation") == entry.notation);
                    expect(attr(*nodes[0], u"mathcolor") == u"red");
                    expect(nodes[0]->attributes().own(u"unknown") == nullptr);
                }
            }
            auto macro = std::get<Macro const *>(f.maps.getMap(u"cancel")->lookup(u"cancelto"));
            expect(std::get<MacroHandler>(macro->function) == &CancelMethods::CancelTo);
            f.compile(u"\\enclose{box}{x}");
            expect(f.error == "UndefinedControlSequence");
        };
    "cancelto parses optional attributes first and creates the original raised script slots"_test =
        [] {
            CancelFixture f;
            auto &root = f.ok(u"\\cancelto[mathcolor=blue]{0}{x}");
            auto supers = cancelNodes(root, NodeKind::msup);
            auto pads = cancelNodes(root, NodeKind::mpadded);
            auto enclosures = cancelNodes(root, NodeKind::menclose);
            expect(supers.size() == 1_u && pads.size() == 1_u && enclosures.size() == 1_u);
            if (supers.size() == 1 && pads.size() == 1 && enclosures.size() == 1) {
                expect(supers[0]->children().size() == 2_u);
                if (supers[0]->children().size() == 2) {
                    expect(supers[0]->children()[0] == enclosures[0]);
                    expect(supers[0]->children()[1] == pads[0]);
                }
                expect(attr(*enclosures[0], u"notation") ==
                       u"updiagonalstrike updiagonalarrow northeastarrow");
                expect(attr(*enclosures[0], u"mathcolor") == u"blue");
                expect(attr(*pads[0], u"depth") == u"-.1em");
                expect(attr(*pads[0], u"height") == u"+.1em");
                expect(attr(*pads[0], u"voffset") == u".1em");
                auto value = cancelNodes(*pads[0], NodeKind::mn);
                auto math = cancelNodes(*enclosures[0], NodeKind::mi);
                expect(value.size() == 1_u && math.size() == 1_u);
                if (!value.empty())
                    expect(NodeUtil::getText(*value[0]) == u"0");
                if (!math.empty())
                    expect(NodeUtil::getText(*math[0]) == u"x");
            }
        };
    "cancelto configured creators see padding enclosure and superscript requests in source order"_test =
        [] {
            CancelFixture f;
            std::vector<CancelRequest> requests;
            struct Restore {
                ~Restore() {
                    cancelRequests = nullptr;
                }
            } restore;
            cancelRequests = &requests;
            f.jax->parseOptions().request().nodeFactory.setCreators(
                NodeCreators{.node = &observeCancel});
            f.ok(u"\\cancelto[data-padding={{1em}},data-thickness=false]{0}{x}");
            expect(requests.size() == 3_u);
            if (requests.size() == 3) {
                expect(requests[0].kind == NodeKind::mpadded);
                expect(requests[1].kind == NodeKind::menclose);
                expect(requests[2].kind == NodeKind::msup);
                expect(attr(requests[0].definition.attributes, u"depth") == u"-.1em");
                expect(attr(requests[0].definition.attributes, u"height") == u"+.1em");
                expect(attr(requests[0].definition.attributes, u"voffset") == u".1em");
                expect(attr(requests[1].definition.attributes, u"data-padding") == u"1em");
                auto thickness = requests[1].definition.attributes.own(u"data-thickness");
                expect(thickness && std::holds_alternative<bool>(*thickness) &&
                       !std::get<bool>(*thickness));
                expect(attr(requests[1].definition.attributes, u"notation") ==
                       u"updiagonalstrike updiagonalarrow northeastarrow");
                expect(requests[2].definition.attributes.own(u"notation") == nullptr);
            }
        };
    "cancel scanning and child parsing errors precede later attribute processing and recover"_test =
        [] {
            struct Example {
                std::u16string_view source;
                std::string_view id;
            };
            for (auto const &example :
                 {Example{u"\\cancel", "MissingArgFor"},
                  {u"\\cancelto", "MissingArgFor"},
                  {u"\\cancelto{0}", "MissingArgFor"},
                  {u"\\cancelto[color=x}]{\\unknown}{x}", "ExtraCloseLooking"},
                  {u"\\cancelto[unknown=x]{\\unknown}{x}", "UndefinedControlSequence"}}) {
                CancelFixture f;
                f.compile(std::u16string(example.source));
                expect(f.error == example.id);
                expect(cancelNodes(f.ok(u"\\cancel{x}"), NodeKind::menclose).size() == 1_u);
            }
        };
    "cancelto formal let aliases retain input names in scanner errors and recover next formula"_test =
        [] {
            CancelFixture f(true);
            f.ok(u"\\let\\savedTo\\cancelto");
            f.compile(u"\\savedTo{0}");
            expect(f.error == "MissingArgFor");
            expect(f.message == u"Missing argument for \\savedTo");
            expect(cancelNodes(f.ok(u"\\savedTo{0}{x}"), NodeKind::msup).size() == 1_u);
        };
};
